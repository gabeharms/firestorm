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

#include "llevents.h"
#include "lltimer.h"
#include "llupdaterservice.h"
#include "llupdatechecker.h"

#include "llpluginprocessparent.h"
#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>

boost::weak_ptr<LLUpdaterServiceImpl> gUpdater;

class LLUpdaterServiceImpl : 
	public LLPluginProcessParentOwner,
	public LLUpdateChecker::Client
{
	static const std::string ListenerName;
	
	std::string mUrl;
	std::string mChannel;
	std::string mVersion;
	
	unsigned int mCheckPeriod;
	bool mIsChecking;
	boost::scoped_ptr<LLPluginProcessParent> mPlugin;
	
	LLUpdateChecker mUpdateChecker;
	LLTimer mTimer;

	void retry(void);
	
	LOG_CLASS(LLUpdaterServiceImpl);
	
public:
	LLUpdaterServiceImpl();
	virtual ~LLUpdaterServiceImpl();

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
	
	// LLUpdateChecker::Client:
	virtual void error(std::string const & message);
	virtual void optionalUpdate(std::string const & newVersion);
	virtual void requiredUpdate(std::string const & newVersion);
	virtual void upToDate(void);
	
	bool onMainLoop(LLSD const & event);	
};

const std::string LLUpdaterServiceImpl::ListenerName = "LLUpdaterServiceImpl";

LLUpdaterServiceImpl::LLUpdaterServiceImpl() :
	mIsChecking(false),
	mCheckPeriod(0),
	mPlugin(0),
	mUpdateChecker(*this)
{
	// Create the plugin parent, this is the owner.
	mPlugin.reset(new LLPluginProcessParent(this));
}

LLUpdaterServiceImpl::~LLUpdaterServiceImpl()
{
	LL_INFOS("UpdaterService") << "shutting down updater service" << LL_ENDL;
	LLEventPumps::instance().obtain("mainloop").stopListening(ListenerName);
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
		
		mUpdateChecker.check(mUrl, mChannel, mVersion);
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

void LLUpdaterServiceImpl::error(std::string const & message)
{
	retry();
}

void LLUpdaterServiceImpl::optionalUpdate(std::string const & newVersion)
{
	retry();
}

void LLUpdaterServiceImpl::requiredUpdate(std::string const & newVersion)
{
	retry();
}

void LLUpdaterServiceImpl::upToDate(void)
{
	retry();
}

void LLUpdaterServiceImpl::retry(void)
{
	LL_INFOS("UpdaterService") << "will check for update again in " << 
	mCheckPeriod << " seconds" << LL_ENDL; 
	mTimer.start();
	mTimer.setTimerExpirySec(mCheckPeriod);
	LLEventPumps::instance().obtain("mainloop").listen(
		ListenerName, boost::bind(&LLUpdaterServiceImpl::onMainLoop, this, _1));
}

bool LLUpdaterServiceImpl::onMainLoop(LLSD const & event)
{
	if(mTimer.hasExpired())
	{
		mTimer.stop();
		LLEventPumps::instance().obtain("mainloop").stopListening(ListenerName);
		mUpdateChecker.check(mUrl, mChannel, mVersion);
	} else {
		// Keep on waiting...
	}
	
	return false;
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
