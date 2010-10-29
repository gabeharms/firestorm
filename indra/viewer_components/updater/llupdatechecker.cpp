/** 
 * @file llupdaterservice.cpp
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

#include "linden_common.h"
#include <boost/format.hpp>
#include "llhttpclient.h"
#include "llupdatechecker.h"


class LLUpdateChecker::Implementation:
	public LLHTTPClient::Responder
{
public:
	
	Implementation(Client & client);
	~Implementation();
	void check(std::string const & host, std::string channel, std::string version);
	
	// Responder:
	virtual void completed(U32 status,
						   const std::string & reason,
						   const LLSD& content);
	virtual void error(U32 status, const std::string & reason);
	
private:
	std::string buildUrl(std::string const & host, std::string channel, std::string version);
	
	Client & mClient;
	LLHTTPClient mHttpClient;
	bool mInProgress;
	LLHTTPClient::ResponderPtr mMe;
	std::string mVersion;
	
	LOG_CLASS(LLUpdateChecker::Implementation);
};



// LLUpdateChecker
//-----------------------------------------------------------------------------


LLUpdateChecker::LLUpdateChecker(LLUpdateChecker::Client & client):
	mImplementation(new LLUpdateChecker::Implementation(client))
{
	; // No op.
}


void LLUpdateChecker::check(std::string const & host, std::string channel, std::string version)
{
	mImplementation->check(host, channel, version);
}



// LLUpdateChecker::Implementation
//-----------------------------------------------------------------------------


LLUpdateChecker::Implementation::Implementation(LLUpdateChecker::Client & client):
	mClient(client),
	mInProgress(false),
	mMe(this)
{
	; // No op.
}


LLUpdateChecker::Implementation::~Implementation()
{
	mMe.reset(0);
}


void LLUpdateChecker::Implementation::check(std::string const & host, std::string channel, std::string version)
{
	// llassert(!mInProgress);
		
	mInProgress = true;
	mVersion = version;
	std::string checkUrl = buildUrl(host, channel, version);
	LL_INFOS("UpdateCheck") << "checking for updates at " << checkUrl << llendl;
	mHttpClient.get(checkUrl, mMe);
}

void LLUpdateChecker::Implementation::completed(U32 status,
							  const std::string & reason,
							  const LLSD & content)
{
	mInProgress = false;
	
	if(status != 200) {
		LL_WARNS("UpdateCheck") << "html error " << status << " (" << reason << ")" << llendl;
		mClient.error(reason);
	} else if(!content["valid"].asBoolean()) {
		LL_INFOS("UpdateCheck") << "version invalid" << llendl;
		mClient.requiredUpdate(content["latest_version"].asString());
	} else if(content["latest_version"].asString() != mVersion) {
		LL_INFOS("UpdateCheck") << "newer version " << content["latest_version"].asString() << " available" << llendl;
		mClient.optionalUpdate(content["latest_version"].asString());
	} else {
		LL_INFOS("UpdateCheck") << "up to date" << llendl;
		mClient.upToDate();
	}
}


void LLUpdateChecker::Implementation::error(U32 status, const std::string & reason)
{
	mInProgress = false;
	LL_WARNS("UpdateCheck") << "update check failed; " << reason << llendl;
}


std::string LLUpdateChecker::Implementation::buildUrl(std::string const & host, std::string channel, std::string version)
{	
	static boost::format urlFormat("%s/version/%s/%s");
	urlFormat % host % channel % version;
	return urlFormat.str();
}

