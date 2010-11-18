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

#include "llupdatedownloader.h"
#include "llevents.h"
#include "lltimer.h"
#include "llupdaterservice.h"
#include "llupdatechecker.h"
#include "llupdateinstaller.h"
#include "llversionviewer.h"

#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include "lldir.h"
#include "llsdserialize.h"
#include "llfile.h"

#if LL_WINDOWS
#pragma warning (disable : 4355) // 'this' used in initializer list: yes, intentionally
#endif


namespace 
{
	boost::weak_ptr<LLUpdaterServiceImpl> gUpdater;

	const std::string UPDATE_MARKER_FILENAME("SecondLifeUpdateReady.xml");
	std::string update_marker_path()
	{
		return gDirUtilp->getExpandedFilename(LL_PATH_LOGS, 
											  UPDATE_MARKER_FILENAME);
	}
	
	std::string install_script_path(void)
	{
#ifdef LL_WINDOWS
		std::string scriptFile = "update_install.bat";
#else
		std::string scriptFile = "update_install";
#endif
		return gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, scriptFile);
	}
	
	LLInstallScriptMode install_script_mode(void) 
	{
#ifdef LL_WINDOWS
		return LL_COPY_INSTALL_SCRIPT_TO_TEMP;
#else
		return LL_RUN_INSTALL_SCRIPT_IN_PLACE;
#endif
	};
	
}

class LLUpdaterServiceImpl : 
	public LLUpdateChecker::Client,
	public LLUpdateDownloader::Client
{
	static const std::string sListenerName;
	
	std::string mProtocolVersion;
	std::string mUrl;
	std::string mPath;
	std::string mChannel;
	std::string mVersion;
	
	unsigned int mCheckPeriod;
	bool mIsChecking;
	bool mIsDownloading;
	
	LLUpdateChecker mUpdateChecker;
	LLUpdateDownloader mUpdateDownloader;
	LLTimer mTimer;

	LLUpdaterService::app_exit_callback_t mAppExitCallback;
	
	LOG_CLASS(LLUpdaterServiceImpl);
	
public:
	LLUpdaterServiceImpl();
	virtual ~LLUpdaterServiceImpl();

	void initialize(const std::string& protocol_version,
				   const std::string& url, 
				   const std::string& path,
				   const std::string& channel,
				   const std::string& version);
	
	void setCheckPeriod(unsigned int seconds);

	void startChecking();
	void stopChecking();
	bool isChecking();
	
	void setAppExitCallback(LLUpdaterService::app_exit_callback_t aecb) { mAppExitCallback = aecb;}

	bool checkForInstall(); // Test if a local install is ready.
	bool checkForResume(); // Test for resumeable d/l.

	// LLUpdateChecker::Client:
	virtual void error(std::string const & message);
	virtual void optionalUpdate(std::string const & newVersion,
								LLURI const & uri,
								std::string const & hash);
	virtual void requiredUpdate(std::string const & newVersion,
								LLURI const & uri,
								std::string const & hash);
	virtual void upToDate(void);
	
	// LLUpdateDownloader::Client
	void downloadComplete(LLSD const & data);
	void downloadError(std::string const & message);

	bool onMainLoop(LLSD const & event);

private:
	void restartTimer(unsigned int seconds);
};

const std::string LLUpdaterServiceImpl::sListenerName = "LLUpdaterServiceImpl";

LLUpdaterServiceImpl::LLUpdaterServiceImpl() :
	mIsChecking(false),
	mIsDownloading(false),
	mCheckPeriod(0),
	mUpdateChecker(*this),
	mUpdateDownloader(*this)
{
}

LLUpdaterServiceImpl::~LLUpdaterServiceImpl()
{
	LL_INFOS("UpdaterService") << "shutting down updater service" << LL_ENDL;
	LLEventPumps::instance().obtain("mainloop").stopListening(sListenerName);
}

void LLUpdaterServiceImpl::initialize(const std::string& protocol_version,
									  const std::string& url, 
									  const std::string& path,
									  const std::string& channel,
									  const std::string& version)
{
	if(mIsChecking || mIsDownloading)
	{
		throw LLUpdaterService::UsageError("LLUpdaterService::initialize call "
										   "while updater is running.");
	}
		
	mProtocolVersion = protocol_version;
	mUrl = url;
	mPath = path;
	mChannel = channel;
	mVersion = version;

	// Check to see if an install is ready.
	if(!checkForInstall())
	{
		checkForResume();
	}	
}

void LLUpdaterServiceImpl::setCheckPeriod(unsigned int seconds)
{
	mCheckPeriod = seconds;
}

void LLUpdaterServiceImpl::startChecking()
{
	if(mUrl.empty() || mChannel.empty() || mVersion.empty())
	{
		throw LLUpdaterService::UsageError("Set params before call to "
			"LLUpdaterService::startCheck().");
	}

	mIsChecking = true;

	if(!mIsDownloading)
	{
		// Checking can only occur during the mainloop.
		// reset the timer to 0 so that the next mainloop event 
		// triggers a check;
		restartTimer(0); 
	}
}

void LLUpdaterServiceImpl::stopChecking()
{
	if(mIsChecking)
	{
		mIsChecking = false;
		mTimer.stop();
	}
}

bool LLUpdaterServiceImpl::isChecking()
{
	return mIsChecking;
}

bool LLUpdaterServiceImpl::checkForInstall()
{
	bool result = false; // return true if install is found.

	llifstream update_marker(update_marker_path(), 
							 std::ios::in | std::ios::binary);

	if(update_marker.is_open())
	{
		// Found an update info - now lets see if its valid.
		LLSD update_info;
		LLSDSerialize::fromXMLDocument(update_info, update_marker);
		update_marker.close();
		LLFile::remove(update_marker_path());

		// Get the path to the installer file.
		LLSD path = update_info.get("path");
		if(update_info["current_version"].asString() != ll_get_version())
		{
			// This viewer is not the same version as the one that downloaded
			// the update.  Do not install this update.
			if(!path.asString().empty())
			{
				llinfos << "ignoring update dowloaded by different client version" << llendl;
				LLFile::remove(path.asString());
			}
			else
			{
				; // Nothing to clean up.
			}
			
			result = false;
		} 
		else if(path.isDefined() && !path.asString().empty())
		{
			int result = ll_install_update(install_script_path(),
										   update_info["path"].asString(),
										   install_script_mode());	
			
			if((result == 0) && mAppExitCallback)
			{
				mAppExitCallback();
			} else if(result != 0) {
				llwarns << "failed to run update install script" << LL_ENDL;
			} else {
				; // No op.
			}
			
			result = true;
		}
	}
	return result;
}

bool LLUpdaterServiceImpl::checkForResume()
{
	bool result = false;
	std::string download_marker_path = mUpdateDownloader.downloadMarkerPath();
	if(LLFile::isfile(download_marker_path))
	{
		llifstream download_marker_stream(download_marker_path, 
								 std::ios::in | std::ios::binary);
		if(download_marker_stream.is_open())
		{
			LLSD download_info;
			LLSDSerialize::fromXMLDocument(download_info, download_marker_stream);
			download_marker_stream.close();
			if(download_info["current_version"].asString() == ll_get_version())
			{
				mIsDownloading = true;
				mUpdateDownloader.resume();
				result = true;
			}
			else 
			{
				// The viewer that started this download is not the same as this viewer; ignore.
				llinfos << "ignoring partial download from different viewer version" << llendl;
				std::string path = download_info["path"].asString();
				if(!path.empty()) LLFile::remove(path);
				LLFile::remove(download_marker_path);
			}
		} 
	}
	return result;
}

void LLUpdaterServiceImpl::error(std::string const & message)
{
	if(mIsChecking)
	{
		restartTimer(mCheckPeriod);
	}
}

void LLUpdaterServiceImpl::optionalUpdate(std::string const & newVersion,
										  LLURI const & uri,
										  std::string const & hash)
{
	mTimer.stop();
	mIsDownloading = true;
	mUpdateDownloader.download(uri, hash);
}

void LLUpdaterServiceImpl::requiredUpdate(std::string const & newVersion,
										  LLURI const & uri,
										  std::string const & hash)
{
	mTimer.stop();
	mIsDownloading = true;
	mUpdateDownloader.download(uri, hash);
}

void LLUpdaterServiceImpl::upToDate(void)
{
	if(mIsChecking)
	{
		restartTimer(mCheckPeriod);
	}
}

void LLUpdaterServiceImpl::downloadComplete(LLSD const & data) 
{ 
	mIsDownloading = false;

	// Save out the download data to the SecondLifeUpdateReady
	// marker file. 
	llofstream update_marker(update_marker_path());
	LLSDSerialize::toPrettyXML(data, update_marker);
	
	LLSD event;
	event["pump"] = LLUpdaterService::pumpName();
	LLSD payload;
	payload["type"] = LLSD(LLUpdaterService::DOWNLOAD_COMPLETE);
	event["payload"] = payload;
	LLEventPumps::instance().obtain("mainlooprepeater").post(event);
}

void LLUpdaterServiceImpl::downloadError(std::string const & message) 
{ 
	LL_INFOS("UpdaterService") << "Error downloading: " << message << LL_ENDL;

	mIsDownloading = false;

	// Restart the timer on error
	if(mIsChecking)
	{
		restartTimer(mCheckPeriod); 
	}

	LLSD event;
	event["pump"] = LLUpdaterService::pumpName();
	LLSD payload;
	payload["type"] = LLSD(LLUpdaterService::DOWNLOAD_ERROR);
	payload["message"] = message;
	event["payload"] = payload;
	LLEventPumps::instance().obtain("mainlooprepeater").post(event);
}

void LLUpdaterServiceImpl::restartTimer(unsigned int seconds)
{
	LL_INFOS("UpdaterService") << "will check for update again in " << 
	seconds << " seconds" << LL_ENDL; 
	mTimer.start();
	mTimer.setTimerExpirySec(seconds);
	LLEventPumps::instance().obtain("mainloop").listen(
		sListenerName, boost::bind(&LLUpdaterServiceImpl::onMainLoop, this, _1));
}

bool LLUpdaterServiceImpl::onMainLoop(LLSD const & event)
{
	if(mTimer.getStarted() && mTimer.hasExpired())
	{
		mTimer.stop();
		LLEventPumps::instance().obtain("mainloop").stopListening(sListenerName);

		// Check for failed install.
		if(LLFile::isfile(ll_install_failed_marker_path()))
		{
			// TODO: notify the user.
			llinfos << "found marker " << ll_install_failed_marker_path() << llendl;
			llinfos << "last install attempt failed" << llendl;
			LLFile::remove(ll_install_failed_marker_path());
		}
		else
		{
			mUpdateChecker.check(mProtocolVersion, mUrl, mPath, mChannel, mVersion);
		}
	} 
	else 
	{
		// Keep on waiting...
	}
	
	return false;
}


//-----------------------------------------------------------------------
// Facade interface

std::string const & LLUpdaterService::pumpName(void)
{
	static std::string name("updater_service");
	return name;
}

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

void LLUpdaterService::initialize(const std::string& protocol_version,
								 const std::string& url, 
								 const std::string& path,
								 const std::string& channel,
								 const std::string& version)
{
	mImpl->initialize(protocol_version, url, path, channel, version);
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

void LLUpdaterService::setImplAppExitCallback(LLUpdaterService::app_exit_callback_t aecb)
{
	return mImpl->setAppExitCallback(aecb);
}


std::string const & ll_get_version(void) {
	static std::string version("");
	
	if (version.empty()) {
		std::ostringstream stream;
		stream << LL_VERSION_MAJOR << "."
		<< LL_VERSION_MINOR << "."
		<< LL_VERSION_PATCH << "."
		<< LL_VERSION_BUILD;
		version = stream.str();
	}
	
	return version;
}

