/** 
 * @file lllogin.cpp
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
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

#include <boost/coroutine/coroutine.hpp>
#include "linden_common.h"
#include "llsd.h"
#include "llsdutil.h"

/*==========================================================================*|
#ifdef LL_WINDOWS
	// non-virtual destructor warning, boost::statechart does this intentionally.
	#pragma warning (disable : 4265) 
#endif
|*==========================================================================*/

#include "lllogin.h"

#include <boost/bind.hpp>

#include "llcoros.h"
#include "llevents.h"
#include "lleventfilter.h"
#include "lleventcoro.h"

//*********************
// LLLogin
// *NOTE:Mani - Is this Impl needed now that the state machine runs the show?
class LLLogin::Impl
{
public:
    Impl():
		mPump("login", true) // Create the module's event pump with a tweaked (unique) name.
    {
        mValidAuthResponse["status"]        = LLSD();
        mValidAuthResponse["errorcode"]     = LLSD();
        mValidAuthResponse["error"]         = LLSD();
        mValidAuthResponse["transfer_rate"] = LLSD();
    }

    void connect(const std::string& uri, const LLSD& credentials);
    void disconnect();
	LLEventPump& getEventPump() { return mPump; }

private:
	void sendProgressEvent(const std::string& state, const std::string& change,
						   const LLSD& data = LLSD())
	{
		LLSD status_data;
		status_data["state"] = state;
		status_data["change"] = change;
		status_data["progress"] = 0.0f;

		if(mAuthResponse.has("transfer_rate"))
		{
			status_data["transfer_rate"] = mAuthResponse["transfer_rate"];
		}

		if(data.isDefined())
		{
			status_data["data"] = data;
		}

		mPump.post(status_data);
	}

    LLSD validateResponse(const std::string& pumpName, const LLSD& response)
    {
        // Validate the response. If we don't recognize it, things
        // could get ugly.
        std::string mismatch(llsd_matches(mValidAuthResponse, response));
        if (! mismatch.empty())
        {
            LL_ERRS("LLLogin") << "Received unrecognized event (" << mismatch << ") on "
                               << pumpName << "pump: " << response
                               << LL_ENDL;
            return LLSD();
        }

        return response;
    }

    // In a coroutine's top-level function args, do NOT NOT NOT accept
    // references (const or otherwise) to anything but the self argument! Pass
    // by value only!
    void login_(LLCoros::self& self, std::string uri, LLSD credentials);

    LLEventStream mPump;
	LLSD mAuthResponse, mValidAuthResponse;
};

void LLLogin::Impl::connect(const std::string& uri, const LLSD& credentials)
{
    // Launch a coroutine with our login_() method. Run the coroutine until
    // its first wait; at that point, return here.
    std::string coroname = 
        LLCoros::instance().launch("LLLogin::Impl::login_",
                                   boost::bind(&Impl::login_, this, _1, uri, credentials));
}

void LLLogin::Impl::login_(LLCoros::self& self, std::string uri, LLSD credentials)
{
    LL_INFOS("LLLogin") << "Entering coroutine " << LLCoros::instance().getName(self)
                        << " with uri '" << uri << "', credentials " << credentials << LL_ENDL;
    // Arriving in SRVRequest state
    LLEventStream replyPump("reply", true);
    // Should be an array of one or more uri strings.
    LLSD rewrittenURIs;
    {
        LLEventTimeout filter(replyPump);
        sendProgressEvent("offline", "srvrequest");

        // Request SRV record.
        LL_INFOS("LLLogin") << "Requesting SRV record from " << uri << LL_ENDL;

        // *NOTE:Mani - Completely arbitrary timeout value for SRV request.
        // filter.errorAfter(5, "SRV Request timed out!");
	
		// 
		F32 seconds_to_timeout = 5.0f;
		filter.actionAfter(seconds_to_timeout, 
			sendProgressEvent("offline", "fail.login", LLSD()));

		// Make request
        LLSD request;
        request["op"] = "rewriteURI";
        request["uri"] = uri;
        request["reply"] = replyPump.getName();
        rewrittenURIs = postAndWait(self, request, "LLAres", filter);
    } // we no longer need the filter

    LLEventPump& xmlrpcPump(LLEventPumps::instance().obtain("LLXMLRPCTransaction"));

    // Loop through the rewrittenURIs, counting attempts along the way.
    // Because of possible redirect responses, we may make more than one
    // attempt per rewrittenURIs entry.
    LLSD::Integer attempts = 0;
    for (LLSD::array_const_iterator urit(rewrittenURIs.beginArray()),
             urend(rewrittenURIs.endArray());
         urit != urend; ++urit)
    {
        LLSD request(credentials);
        request["reply"] = replyPump.getName();
        request["uri"] = *urit;
        std::string status;

        // Loop back to here if login attempt redirects to a different
        // request["uri"]
        for (;;)
        {
            ++attempts;
            LLSD progress_data;
            progress_data["attempt"] = attempts;
            progress_data["request"] = request;
            sendProgressEvent("offline", "authenticating", progress_data);

            // We expect zero or more "Downloading" status events, followed by
            // exactly one event with some other status. Use postAndWait() the
            // first time, because -- at least in unit-test land -- it's
            // possible for the reply to arrive before the post() call
            // returns. Subsequent responses, of course, must be awaited
            // without posting again.
            for (mAuthResponse = validateResponse(replyPump.getName(),
                                     postAndWait(self, request, xmlrpcPump, replyPump, "reply"));
                 mAuthResponse["status"].asString() == "Downloading";
                 mAuthResponse = validateResponse(replyPump.getName(),
                                     waitForEventOn(self, replyPump)))
            {
                // Still Downloading -- send progress update.
                sendProgressEvent("offline", "downloading");
            }
            status = mAuthResponse["status"].asString();

            // Okay, we've received our final status event for this
            // request. Unless we got a redirect response, break the retry
            // loop for the current rewrittenURIs entry.
            if (! (status == "Complete" &&
                   mAuthResponse["responses"]["login"].asString() == "indeterminate"))
            {
                break;
            }

            // Here the login service at the current URI is redirecting us
            // to some other URI ("indeterminate" -- why not "redirect"?).
            // The response should contain another uri to try, with its
            // own auth method.
            request["uri"] = mAuthResponse["next_url"];
            request["method"] = mAuthResponse["next_method"];
        } // loop back to try the redirected URI

        // Here we're done with redirects for the current rewrittenURIs
        // entry.
        if (status == "Complete")
        {
            // StatusComplete does not imply auth success. Check the
            // actual outcome of the request. We've already handled the
            // "indeterminate" case in the loop above.
            if (mAuthResponse["responses"]["login"].asString() == "true")
            {
                sendProgressEvent("online", "connect", mAuthResponse["responses"]);
            }
            else
            {
                sendProgressEvent("offline", "fail.login", mAuthResponse["responses"]);
            }
            return;             // Done!
        }
        // If we don't recognize status at all, trouble
        if (! (status == "CURLError"
               || status == "XMLRPCError"
               || status == "OtherError"))
        {
            LL_ERRS("LLLogin") << "Unexpected status from " << xmlrpcPump.getName() << " pump: "
                               << mAuthResponse << LL_ENDL;
            return;
        }

        // Here status IS one of the errors tested above.
    } // Retry if there are any more rewrittenURIs.

    // Here we got through all the rewrittenURIs without succeeding. Tell
    // caller this didn't work out so well. Of course, the only failure data
    // we can reasonably show are from the last of the rewrittenURIs.
    sendProgressEvent("offline", "fail.login", mAuthResponse["responses"]);
}

void LLLogin::Impl::disconnect()
{
    sendProgressEvent("offline", "disconnect");
}

//*********************
// LLLogin
LLLogin::LLLogin() :
	mImpl(new LLLogin::Impl())
{
}

LLLogin::~LLLogin()
{
}

void LLLogin::connect(const std::string& uri, const LLSD& credentials)
{
	mImpl->connect(uri, credentials);
}


void LLLogin::disconnect()
{
	mImpl->disconnect();
}

LLEventPump& LLLogin::getEventPump()
{
	return mImpl->getEventPump();
}

// The following is the list of important functions that happen in the 
// current login process that we want to move to this login module.

// The list associates to event with the original idle_startup() 'STATE'.

// Rewrite URIs
 // State_LOGIN_AUTH_INIT
// Given a vector of login uris (usually just one), perform a dns lookup for the 
// SRV record from each URI. I think this is used to distribute login requests to 
// a single URI to multiple hosts.
// This is currently a synchronous action. (See LLSRV::rewriteURI() implementation)
// On dns lookup error the output uris == the input uris.
//
// Input: A vector of login uris
// Output: A vector of login uris
//
// Code:
// std::vector<std::string> uris;
// LLViewerLogin::getInstance()->getLoginURIs(uris);
// std::vector<std::string>::const_iterator iter, end;
// for (iter = uris.begin(), end = uris.end(); iter != end; ++iter)
// {
//	std::vector<std::string> rewritten;
//	rewritten = LLSRV::rewriteURI(*iter);
//	sAuthUris.insert(sAuthUris.end(),
//					 rewritten.begin(), rewritten.end());
// }
// sAuthUriNum = 0;

// Authenticate 
// STATE_LOGIN_AUTHENTICATE
// Connect to the login server, presumably login.cgi, requesting the login 
// and a slew of related initial connection information.
// This is an asynch action. The final response, whether success or error
// is handled by STATE_LOGIN_PROCESS_REPONSE.
// There is no immediate error or output from this call.
// 
// Input: 
//  URI
//  Credentials (first, last, password)
//  Start location
//  Bool Flags:
//    skip optional update
//    accept terms of service
//    accept critical message
//  Last exec event. (crash state of previous session)
//  requested optional data (inventory skel, initial outfit, etc.)
//  local mac address
//  viewer serial no. (md5 checksum?)

//sAuthUriNum = llclamp(sAuthUriNum, 0, (S32)sAuthUris.size()-1);
//LLUserAuth::getInstance()->authenticate(
//	sAuthUris[sAuthUriNum],
//	auth_method,
//	firstname,
//	lastname,			
//	password, // web_login_key,
//	start.str(),
//	gSkipOptionalUpdate,
//	gAcceptTOS,
//	gAcceptCriticalMessage,
//	gLastExecEvent,
//	requested_options,
//	hashed_mac_string,
//	LLAppViewer::instance()->getSerialNumber());

//
// Download the Response
// STATE_LOGIN_NO_REPONSE_YET and STATE_LOGIN_DOWNLOADING
// I had assumed that this was default behavior of the message system. However...
// During login, the message system is checked only by these two states in idle_startup().
// I guess this avoids the overhead of checking network messages for those login states
// that don't need to do so, but geez!
// There are two states to do this one function just to update the login
// status text from 'Logging In...' to 'Downloading...'
// 

//
// Handle Login Response
// STATE_LOGIN_PROCESS_RESPONSE
// 
// This state handle the result of the request to login. There is a metric ton of
// code in this case. This state will transition to:
// STATE_WORLD_INIT, on success.
// STATE_AUTHENTICATE, on failure.
// STATE_UPDATE_CHECK, to handle user during login interaction like TOS display.
//
// Much of the code in this case belongs on the viewer side of the fence and not in login. 
// Login should probably return with a couple of events, success and failure.
// Failure conditions can be specified in the events data pacet to allow the viewer 
// to re-engauge login as is appropriate. (Or should there be multiple failure messages?)
// Success is returned with the data requested from the login. According to OGP specs 
// there may be intermediate steps before reaching this result in future login 
// implementations.
