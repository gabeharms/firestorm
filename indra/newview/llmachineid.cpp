/** 
 * @file llmachineid.cpp
 * @brief retrieves unique machine ids
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "lluuid.h"
#include "llmachineid.h"
#define _WIN32_DCOM
#include <iostream>
using namespace std;
#include <comdef.h>
#include <Wbemidl.h>

unsigned char static_unique_id[] =  {0,0,0,0,0,0};
bool static has_static_unique_id = false;

// get an unique machine id.
// NOT THREAD SAFE - do before setting up threads.
// MAC Address doesn't work for Windows 7 since the first returned hardware MAC address changes with each reboot,  Go figure??

S32 LLMachineID::init()
{
    memset(static_unique_id,0,sizeof(static_unique_id));
    size_t len = sizeof(static_unique_id);
    S32 ret_code = 0;
#if	LL_WINDOWS
# pragma comment(lib, "wbemuuid.lib")

        // algorithm to detect BIOS serial number found at:
        // http://msdn.microsoft.com/en-us/library/aa394077%28VS.85%29.aspx
        // we can't use the MAC address since on Windows 7, the first returned MAC address changes with every reboot.


        HRESULT hres;

        // Step 1: --------------------------------------------------
        // Initialize COM. ------------------------------------------

        hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
        if (FAILED(hres))
        {
            LL_DEBUGS("AppInit") << "Failed to initialize COM library. Error code = 0x"   << hex << hres << LL_ENDL;
            return 1;                  // Program has failed.
        }

        // Step 2: --------------------------------------------------
        // Set general COM security levels --------------------------
        // Note: If you are using Windows 2000, you need to specify -
        // the default authentication credentials for a user by using
        // a SOLE_AUTHENTICATION_LIST structure in the pAuthList ----
        // parameter of CoInitializeSecurity ------------------------

        hres =  CoInitializeSecurity(
            NULL, 
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
            );

                          
        if (FAILED(hres))
        {
            LL_DEBUGS("AppInit") << "Failed to initialize security. Error code = 0x"  << hex << hres << LL_ENDL;
            CoUninitialize();
            return 1;                    // Program has failed.
        }
        
        // Step 3: ---------------------------------------------------
        // Obtain the initial locator to WMI -------------------------

        IWbemLocator *pLoc = NULL;

        hres = CoCreateInstance(
            CLSID_WbemLocator,             
            0, 
            CLSCTX_INPROC_SERVER, 
            IID_IWbemLocator, (LPVOID *) &pLoc);
     
        if (FAILED(hres))
        {
            LL_DEBUGS("AppInit") << "Failed to create IWbemLocator object." << " Err code = 0x" << hex << hres << LL_ENDL;
            CoUninitialize();
            return 1;                 // Program has failed.
        }

        // Step 4: -----------------------------------------------------
        // Connect to WMI through the IWbemLocator::ConnectServer method

        IWbemServices *pSvc = NULL;
    	
        // Connect to the root\cimv2 namespace with
        // the current user and obtain pointer pSvc
        // to make IWbemServices calls.
        hres = pLoc->ConnectServer(
             _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
             NULL,                    // User name. NULL = current user
             NULL,                    // User password. NULL = current
             0,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             0,                       // Authority (e.g. Kerberos)
             0,                       // Context object 
             &pSvc                    // pointer to IWbemServices proxy
             );
        
        if (FAILED(hres))
        {
            LL_DEBUGS("AppInit") << "Could not connect. Error code = 0x"  << hex << hres << LL_ENDL;
            pLoc->Release();     
            CoUninitialize();
            return 1;                // Program has failed.
        }

        LL_DEBUGS("AppInit") << "Connected to ROOT\\CIMV2 WMI namespace" << LL_ENDL;


        // Step 5: --------------------------------------------------
        // Set security levels on the proxy -------------------------

        hres = CoSetProxyBlanket(
           pSvc,                        // Indicates the proxy to set
           RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
           RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
           NULL,                        // Server principal name 
           RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
           RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
           NULL,                        // client identity
           EOAC_NONE                    // proxy capabilities 
        );

        if (FAILED(hres))
        {
            LL_DEBUGS("AppInit") << "Could not set proxy blanket. Error code = 0x"   << hex << hres << LL_ENDL;
            pSvc->Release();
            pLoc->Release();     
            CoUninitialize();
            return 1;               // Program has failed.
        }

        // Step 6: --------------------------------------------------
        // Use the IWbemServices pointer to make requests of WMI ----

        // For example, get the name of the operating system
        IEnumWbemClassObject* pEnumerator = NULL;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"), 
            bstr_t("SELECT * FROM Win32_OperatingSystem"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
            NULL,
            &pEnumerator);
        
        if (FAILED(hres))
        {
            LL_DEBUGS("AppInit") << "Query for operating system name failed." << " Error code = 0x"  << hex << hres << LL_ENDL;
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return 1;               // Program has failed.
        }

        // Step 7: -------------------------------------------------
        // Get the data from the query in step 6 -------------------
     
        IWbemClassObject *pclsObj = NULL;
        ULONG uReturn = 0;
       
        while (pEnumerator)
        {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
                &pclsObj, &uReturn);

            if(0 == uReturn)
            {
                break;
            }

            VARIANT vtProp;

            // Get the value of the Name property
            hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
            LL_DEBUGS("AppInit") << " Serial Number : " << vtProp.bstrVal << LL_ENDL;
            // use characters in the returned Serial Number to create a byte array of size len
            BSTR serialNumber ( vtProp.bstrVal);
            unsigned int j = 0;
            while( vtProp.bstrVal[j] != 0)
            {
                for (unsigned int i = 0; i < len; i++)
                {
                    if (vtProp.bstrVal[j] == 0)
                        break;
                    
                    static_unique_id[i] = (unsigned int)(static_unique_id[i] + serialNumber[j]);
                    j++;
                }
            }
            VariantClear(&vtProp);

            pclsObj->Release();
            pclsObj = NULL;
            break;
        }

        // Cleanup
        // ========
        
        if (pSvc)
            pSvc->Release();
        if (pLoc)
            pLoc->Release();
        if (pEnumerator)
            pEnumerator->Release();
        CoUninitialize();
        ret_code=0;
#else
        ret_code = LLUUID::getNodeID(&static_unique_id);
#endif
        has_static_unique_id = true;
        return ret_code;
}


S32 LLMachineID::getUniqueID(unsigned char *unique_id, size_t len)
{
    if (has_static_unique_id)
    {
        memcpy ( unique_id, &static_unique_id, len);
        LL_DEBUGS("AppInit") << "UniqueID: " << unique_id[0] << unique_id[1]<< unique_id[2] << unique_id[3] << unique_id[4] << unique_id [5] << LL_ENDL;
        return 1;
    }
    return 0;
}




