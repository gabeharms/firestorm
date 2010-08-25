/** 
 * @file llsecapi_test.cpp
 * @author Roxie
 * @date 2009-02-10
 * @brief Test the sec api functionality
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
#include "../llviewerprecompiledheaders.h"
#include "../llviewernetwork.h"
#include "../test/lltut.h"
#include "../llsecapi.h"
#include "../../llxml/llcontrol.h"


//----------------------------------------------------------------------------               
// Mock objects for the dependencies of the code we're testing                               

LLControlGroup::LLControlGroup(const std::string& name)
: LLInstanceTracker<LLControlGroup, std::string>(name) {}
LLControlGroup::~LLControlGroup() {}
BOOL LLControlGroup::declareString(const std::string& name,
                                   const std::string& initial_val,
                                   const std::string& comment,
                                   BOOL persist) {return TRUE;}
void LLControlGroup::setString(const std::string& name, const std::string& val){}
std::string LLControlGroup::getString(const std::string& name)
{
	return "";
}


LLControlGroup gSavedSettings("test");
class LLSecAPIBasicHandler : public LLSecAPIHandler
{
protected:
	LLPointer<LLCertificateChain> mCertChain;
	LLPointer<LLCertificate> mCert;
	LLPointer<LLCertificateStore> mCertStore;
	LLSD mLLSD;
	
public:
	LLSecAPIBasicHandler() {}
	
	virtual ~LLSecAPIBasicHandler() {}
	
	// instantiate a certificate from a pem string
	virtual LLPointer<LLCertificate> getCertificate(const std::string& pem_cert) 
	{ 
		return mCert; 
	}
	
	
	// instiate a certificate from an openssl X509 structure
	virtual LLPointer<LLCertificate> getCertificate(X509* openssl_cert) 
	{	
		return mCert; 
	}
	
	
	// instantiate a chain from an X509_STORE_CTX
	virtual LLPointer<LLCertificateChain> getCertificateChain(const X509_STORE_CTX* chain) 
	{ 
		return mCertChain; 
	}
	
	// instantiate a cert store given it's id.  if a persisted version
	// exists, it'll be loaded.  If not, one will be created (but not
	// persisted)
	virtual LLPointer<LLCertificateStore> getCertificateStore(const std::string& store_id) 
	{ 
		return mCertStore; 
	}
	
	// persist data in a protected store
	virtual void setProtectedData(const std::string& data_type,
								  const std::string& data_id,
								  const LLSD& data) {}
	
	// retrieve protected data
	virtual LLSD getProtectedData(const std::string& data_type,
								  const std::string& data_id) 
	{ 
		return mLLSD;
	}
	
	virtual void deleteProtectedData(const std::string& data_type,
									 const std::string& data_id)
	{
	}
	
	virtual LLPointer<LLCredential> createCredential(const std::string& grid,
													 const LLSD& identifier, 
													 const LLSD& authenticator)
	{
		LLPointer<LLCredential> cred = NULL;
		return cred;
	}
	
	virtual LLPointer<LLCredential> loadCredential(const std::string& grid)
	{
		LLPointer<LLCredential> cred = NULL;
		return cred;
	}
	
	virtual void saveCredential(LLPointer<LLCredential> cred, bool save_authenticator) {}
	
	virtual void deleteCredential(LLPointer<LLCredential> cred) {}
};

// -------------------------------------------------------------------------------------------
// TUT
// -------------------------------------------------------------------------------------------
namespace tut
{
	// Test wrapper declaration : wrapping nothing for the moment
	struct secapiTest
	{
		
		secapiTest()
		{
		}
		~secapiTest()
		{
		}
	};
	
	// Tut templating thingamagic: test group, object and test instance
	typedef test_group<secapiTest> secapiTestFactory;
	typedef secapiTestFactory::object secapiTestObject;
	tut::secapiTestFactory tut_test("LLSecAPI");
	
	// ---------------------------------------------------------------------------------------
	// Test functions 
	// ---------------------------------------------------------------------------------------
	// registration
	template<> template<>
	void secapiTestObject::test<1>()
	{
		// retrieve an unknown handler

		ensure("'Unknown' handler should be NULL", !(BOOL)getSecHandler("unknown"));
		LLPointer<LLSecAPIHandler> test1_handler =  new LLSecAPIBasicHandler();
		registerSecHandler("sectest1", test1_handler);
		ensure("'Unknown' handler should be NULL", !(BOOL)getSecHandler("unknown"));
		LLPointer<LLSecAPIHandler> retrieved_test1_handler = getSecHandler("sectest1");
		ensure("Retrieved sectest1 handler should be the same", 
			   retrieved_test1_handler == test1_handler);
		
		// insert a second handler
		LLPointer<LLSecAPIHandler> test2_handler =  new LLSecAPIBasicHandler();
		registerSecHandler("sectest2", test2_handler);
		ensure("'Unknown' handler should be NULL", !(BOOL)getSecHandler("unknown"));
		retrieved_test1_handler = getSecHandler("sectest1");
		ensure("Retrieved sectest1 handler should be the same", 
			   retrieved_test1_handler == test1_handler);

		LLPointer<LLSecAPIHandler> retrieved_test2_handler = getSecHandler("sectest2");
		ensure("Retrieved sectest1 handler should be the same", 
			   retrieved_test2_handler == test2_handler);
		
	}
}
