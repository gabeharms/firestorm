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

#include "llupdaterservice.h"

#include "llpluginprocessparent.h"
#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>

boost::weak_ptr<LLUpdaterServiceImpl> gUpdater;

class LLUpdaterServiceImpl : public LLPluginProcessParentOwner
{
	std::string mUrl;
	std::string mChannel;
	std::string mVersion;
	
	unsigned int mCheckPeriod;
	bool mIsChecking;
	boost::scoped_ptr<LLPluginProcessParent> mPlugin;

public:
	LLUpdaterServiceImpl();
	virtual ~LLUpdaterServiceImpl() {}

	// LLPluginProcessParentOwner interfaces
	virtual void receivePluginMessage(const LLPluginMessage &message);
	virtual bool receivePluginMessageEarly(const LLPluginMessage &message);
	virtual void pluginLaunchFailed();
	virtual void pluginDied();

	void setParams(const std::string& url,
				   const std::string& channel,
				   const std::string& version);

	void setCheckPeriod(unsigned int seconds);

	void startChecking();
	void stopChecking();
	bool isChecking();
};

LLUpdaterServiceImpl::LLUpdaterServiceImpl() :
	mIsChecking(false),
	mCheckPeriod(0),
	mPlugin(0)
{
	// Create the plugin parent, this is the owner.
	mPlugin.reset(new LLPluginProcessParent(this));
}

// LLPluginProcessParentOwner interfaces
void LLUpdaterServiceImpl::receivePluginMessage(const LLPluginMessage &message)
{
}

bool LLUpdaterServiceImpl::receivePluginMessageEarly(const LLPluginMessage &message) 
{
	return false;
};

void LLUpdaterServiceImpl::pluginLaunchFailed() 
{
};

void LLUpdaterServiceImpl::pluginDied() 
{
};

void LLUpdaterServiceImpl::setParams(const std::string& url,
									 const std::string& channel,
									 const std::string& version)
{
	if(mIsChecking)
	{
		throw LLUpdaterService::UsageError("Call LLUpdaterService::stopCheck()"
			" before setting params.");
	}
		
	mUrl = url;
	mChannel = channel;
	mVersion = version;
}

void LLUpdaterServiceImpl::setCheckPeriod(unsigned int seconds)
{
	mCheckPeriod = seconds;
}

void LLUpdaterServiceImpl::startChecking()
{
	if(!mIsChecking)
	{
		if(mUrl.empty() || mChannel.empty() || mVersion.empty())
		{
			throw LLUpdaterService::UsageError("Set params before call to "
				"LLUpdaterService::startCheck().");
		}
		mIsChecking = true;
	}
}

void LLUpdaterServiceImpl::stopChecking()
{
	if(mIsChecking)
	{
		mIsChecking = false;
	}
}

bool LLUpdaterServiceImpl::isChecking()
{
	return mIsChecking;
}

//-----------------------------------------------------------------------
// Facade interface
LLUpdaterService::LLUpdaterService()
{
	if(gUpdater.expired())
	{
		mImpl = 
			boost::shared_ptr<LLUpdaterServiceImpl>(new LLUpdaterServiceImpl());
		gUpdater = mImpl;
	}
	else
	{
		mImpl = gUpdater.lock();
	}
}

LLUpdaterService::~LLUpdaterService()
{
}

void LLUpdaterService::setParams(const std::string& url,
								 const std::string& chan,
								 const std::string& vers)
{
	mImpl->setParams(url, chan, vers);
}

void LLUpdaterService::setCheckPeriod(unsigned int seconds)
{
	mImpl->setCheckPeriod(seconds);
}
	
void LLUpdaterService::startChecking()
{
	mImpl->startChecking();
}

void LLUpdaterService::stopChecking()
{
	mImpl->stopChecking();
}

bool LLUpdaterService::isChecking()
{
	return mImpl->isChecking();
}
