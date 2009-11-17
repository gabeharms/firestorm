/**
 * @file llviewermedia.cpp
 * @brief Client interface to the media engine
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llagent.h"
#include "llviewermedia.h"
#include "llviewermediafocus.h"
#include "llmimetypes.h"
#include "llmediaentry.h"
#include "llviewercontrol.h"
#include "llviewertexture.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llversionviewer.h"
#include "llviewertexturelist.h"
#include "llvovolume.h"
#include "llpluginclassmedia.h"

#include "llevent.h"		// LLSimpleListener
#include "llnotifications.h"
#include "lluuid.h"
#include "llkeyboard.h"
#include "llmutelist.h"

#include <boost/bind.hpp>	// for SkinFolder listener
#include <boost/signals2.hpp>

/*static*/ const char* LLViewerMedia::AUTO_PLAY_MEDIA_SETTING = "AutoPlayMedia";

// Move this to its own file.

LLViewerMediaEventEmitter::~LLViewerMediaEventEmitter()
{
	observerListType::iterator iter = mObservers.begin();

	while( iter != mObservers.end() )
	{
		LLViewerMediaObserver *self = *iter;
		iter++;
		remObserver(self);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaEventEmitter::addObserver( LLViewerMediaObserver* observer )
{
	if ( ! observer )
		return false;

	if ( std::find( mObservers.begin(), mObservers.end(), observer ) != mObservers.end() )
		return false;

	mObservers.push_back( observer );
	observer->mEmitters.push_back( this );

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaEventEmitter::remObserver( LLViewerMediaObserver* observer )
{
	if ( ! observer )
		return false;

	mObservers.remove( observer );
	observer->mEmitters.remove(this);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
void LLViewerMediaEventEmitter::emitEvent( LLPluginClassMedia* media, LLViewerMediaObserver::EMediaEvent event )
{
	// Broadcast the event to any observers.
	observerListType::iterator iter = mObservers.begin();
	while( iter != mObservers.end() )
	{
		LLViewerMediaObserver *self = *iter;
		++iter;
		self->handleMediaEvent( media, event );
	}
}

// Move this to its own file.
LLViewerMediaObserver::~LLViewerMediaObserver()
{
	std::list<LLViewerMediaEventEmitter *>::iterator iter = mEmitters.begin();

	while( iter != mEmitters.end() )
	{
		LLViewerMediaEventEmitter *self = *iter;
		iter++;
		self->remObserver( this );
	}
}


// Move this to its own file.
// helper class that tries to download a URL from a web site and calls a method
// on the Panel Land Media and to discover the MIME type
class LLMimeDiscoveryResponder : public LLHTTPClient::Responder
{
LOG_CLASS(LLMimeDiscoveryResponder);
public:
	LLMimeDiscoveryResponder( viewer_media_t media_impl)
		: mMediaImpl(media_impl),
		  mInitialized(false)
	{
		if(mMediaImpl->mMimeTypeProbe != NULL)
		{
			llerrs << "impl already has an outstanding responder" << llendl;
		}
		
		mMediaImpl->mMimeTypeProbe = this;
	}

	~LLMimeDiscoveryResponder()
	{
		disconnectOwner();
	}

	virtual void completedHeader(U32 status, const std::string& reason, const LLSD& content)
	{
		std::string media_type = content["content-type"].asString();
		std::string::size_type idx1 = media_type.find_first_of(";");
		std::string mime_type = media_type.substr(0, idx1);
		completeAny(status, mime_type);
	}

	virtual void error( U32 status, const std::string& reason )
	{
		if(status == 401)
		{
			// This is the "you need to authenticate" status.  
			// Treat this like an html page.
			completeAny(status, "text/html");
		}
		else
		{
			llwarns << "responder failed with status " << status << ", reason " << reason << llendl;
		
			if(mMediaImpl)
			{
				mMediaImpl->mMediaSourceFailed = true;
			}
		}
	}

	void completeAny(U32 status, const std::string& mime_type)
	{
		// the call to initializeMedia may disconnect the responder, which will clear mMediaImpl.
		// Make a local copy so we can call loadURI() afterwards.
		LLViewerMediaImpl *impl = mMediaImpl;
		
		if(impl && !mInitialized && ! mime_type.empty())
		{
			if(impl->initializeMedia(mime_type))
			{
				mInitialized = true;
				impl->loadURI();
				disconnectOwner();
			}
		}
	}
	
	void cancelRequest()
	{
		disconnectOwner();
	}
	
private:
	void disconnectOwner()
	{
		if(mMediaImpl)
		{
			if(mMediaImpl->mMimeTypeProbe != this)
			{
				llerrs << "internal error: mMediaImpl->mMimeTypeProbe != this" << llendl;
			}

			mMediaImpl->mMimeTypeProbe = NULL;
		}
		mMediaImpl = NULL;
	}
	
	
public:
		LLViewerMediaImpl *mMediaImpl;
		bool mInitialized;
};
static LLViewerMedia::impl_list sViewerMediaImplList;
static LLTimer sMediaCreateTimer;
static const F32 LLVIEWERMEDIA_CREATE_DELAY = 1.0f;
static F32 sGlobalVolume = 1.0f;

//////////////////////////////////////////////////////////////////////////////////////////
static void add_media_impl(LLViewerMediaImpl* media)
{
	sViewerMediaImplList.push_back(media);
}

//////////////////////////////////////////////////////////////////////////////////////////
static void remove_media_impl(LLViewerMediaImpl* media)
{
	LLViewerMedia::impl_list::iterator iter = sViewerMediaImplList.begin();
	LLViewerMedia::impl_list::iterator end = sViewerMediaImplList.end();
	
	for(; iter != end; iter++)
	{
		if(media == *iter)
		{
			sViewerMediaImplList.erase(iter);
			return;
		}
	}
}

class LLViewerMediaMuteListObserver : public LLMuteListObserver
{
	/* virtual */ void onChange()  { LLViewerMedia::muteListChanged();}
};

static LLViewerMediaMuteListObserver sViewerMediaMuteListObserver;
static bool sViewerMediaMuteListObserverInitialized = false;
static bool sInWorldMediaDisabled = false;


//////////////////////////////////////////////////////////////////////////////////////////
// LLViewerMedia

//////////////////////////////////////////////////////////////////////////////////////////
// static
viewer_media_t LLViewerMedia::newMediaImpl(
											 const LLUUID& texture_id,
											 S32 media_width, 
											 S32 media_height, 
											 U8 media_auto_scale,
											 U8 media_loop)
{
	LLViewerMediaImpl* media_impl = getMediaImplFromTextureID(texture_id);
	if(media_impl == NULL || texture_id.isNull())
	{
		// Create the media impl
		media_impl = new LLViewerMediaImpl(texture_id, media_width, media_height, media_auto_scale, media_loop);
	}
	else
	{
		media_impl->stop();
		media_impl->mTextureId = texture_id;
		media_impl->mMediaWidth = media_width;
		media_impl->mMediaHeight = media_height;
		media_impl->mMediaAutoScale = media_auto_scale;
		media_impl->mMediaLoop = media_loop;
	}

	return media_impl;
}

viewer_media_t LLViewerMedia::updateMediaImpl(LLMediaEntry* media_entry, const std::string& previous_url, bool update_from_self)
{	
	// Try to find media with the same media ID
	viewer_media_t media_impl = getMediaImplFromTextureID(media_entry->getMediaID());

	bool was_loaded = false;
	bool needs_navigate = false;
	
	if(media_impl)
	{	
		was_loaded = media_impl->hasMedia();
		
		media_impl->setHomeURL(media_entry->getHomeURL());
		
		media_impl->mMediaAutoScale = media_entry->getAutoScale();
		media_impl->mMediaLoop = media_entry->getAutoLoop();
		media_impl->mMediaWidth = media_entry->getWidthPixels();
		media_impl->mMediaHeight = media_entry->getHeightPixels();
		if (media_impl->mMediaSource)
		{
			media_impl->mMediaSource->setAutoScale(media_impl->mMediaAutoScale);
			media_impl->mMediaSource->setLoop(media_impl->mMediaLoop);
			media_impl->mMediaSource->setSize(media_entry->getWidthPixels(), media_entry->getHeightPixels());
		}
		
		if(media_entry->getCurrentURL().empty())
		{
			// The current media URL is now empty.  Unload the media source.
			media_impl->unload();
		}
		else
		{
			// The current media URL is not empty.
			// If (the media was already loaded OR the media was set to autoplay) AND this update didn't come from this agent,
			// do a navigate.
			
			if((was_loaded || (media_entry->getAutoPlay() && gSavedSettings.getBOOL(AUTO_PLAY_MEDIA_SETTING))) && !update_from_self)
			{
				needs_navigate = (media_entry->getCurrentURL() != previous_url);
			}
		}
	}
	else
	{
		media_impl = newMediaImpl(
			media_entry->getMediaID(), 
			media_entry->getWidthPixels(),
			media_entry->getHeightPixels(), 
			media_entry->getAutoScale(), 
			media_entry->getAutoLoop());
		
		media_impl->setHomeURL(media_entry->getHomeURL());
		
		if(media_entry->getAutoPlay() && gSavedSettings.getBOOL(AUTO_PLAY_MEDIA_SETTING))
		{
			needs_navigate = true;
		}
	}
	
	if(media_impl)
	{
		std::string url = media_entry->getCurrentURL();
		if(needs_navigate)
		{
			media_impl->navigateTo(url, "", true, true);
		}
		else if(!media_impl->mMediaURL.empty() && (media_impl->mMediaURL != url))
		{
			// If we already have a non-empty media URL set and we aren't doing a navigate, update the media URL to match the media entry.
			media_impl->mMediaURL = url;

			// If this causes a navigate at some point (such as after a reload), it should be considered server-driven so it isn't broadcast.
			media_impl->mNavigateServerRequest = true;
		}
	}
	
	return media_impl;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
LLViewerMediaImpl* LLViewerMedia::getMediaImplFromTextureID(const LLUUID& texture_id)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* media_impl = *iter;
		if(media_impl->getMediaTextureID() == texture_id)
		{
			return media_impl;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
std::string LLViewerMedia::getCurrentUserAgent()
{
	// Don't use user-visible string to avoid 
	// punctuation and strange characters.
	std::string skin_name = gSavedSettings.getString("SkinCurrent");

	// Just in case we need to check browser differences in A/B test
	// builds.
	std::string channel = gSavedSettings.getString("VersionChannelName");

	// append our magic version number string to the browser user agent id
	// See the HTTP 1.0 and 1.1 specifications for allowed formats:
	// http://www.ietf.org/rfc/rfc1945.txt section 10.15
	// http://www.ietf.org/rfc/rfc2068.txt section 3.8
	// This was also helpful:
	// http://www.mozilla.org/build/revised-user-agent-strings.html
	std::ostringstream codec;
	codec << "SecondLife/";
	codec << LL_VERSION_MAJOR << "." << LL_VERSION_MINOR << "." << LL_VERSION_PATCH << "." << LL_VERSION_BUILD;
	codec << " (" << channel << "; " << skin_name << " skin)";
	llinfos << codec.str() << llendl;
	
	return codec.str();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateBrowserUserAgent()
{
	std::string user_agent = getCurrentUserAgent();
	
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if(pimpl->mMediaSource && pimpl->mMediaSource->pluginSupportsMediaBrowser())
		{
			pimpl->mMediaSource->setBrowserUserAgent(user_agent);
		}
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::handleSkinCurrentChanged(const LLSD& /*newvalue*/)
{
	// gSavedSettings is already updated when this function is called.
	updateBrowserUserAgent();
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::textureHasMedia(const LLUUID& texture_id)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if(pimpl->getMediaTextureID() == texture_id)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setVolume(F32 volume)
{
	if(volume != sGlobalVolume)
	{
		sGlobalVolume = volume;
		impl_list::iterator iter = sViewerMediaImplList.begin();
		impl_list::iterator end = sViewerMediaImplList.end();

		for(; iter != end; iter++)
		{
			LLViewerMediaImpl* pimpl = *iter;
			pimpl->updateVolume();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
F32 LLViewerMedia::getVolume()
{
	return sGlobalVolume;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::muteListChanged()
{
	// When the mute list changes, we need to check mute status on all impls.
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->mNeedsMuteCheck = true;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setInWorldMediaDisabled(bool disabled)
{
	sInWorldMediaDisabled = disabled;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::getInWorldMediaDisabled()
{
	return sInWorldMediaDisabled;
}

LLViewerMedia::impl_list &LLViewerMedia::getPriorityList()
{
	return sViewerMediaImplList;
}

// This is the predicate function used to sort sViewerMediaImplList by priority.
bool LLViewerMedia::priorityComparitor(const LLViewerMediaImpl* i1, const LLViewerMediaImpl* i2)
{
	if(i1->isForcedUnloaded() && !i2->isForcedUnloaded())
	{
		// Muted or failed items always go to the end of the list, period.
		return false;
	}
	else if(i2->isForcedUnloaded() && !i1->isForcedUnloaded())
	{
		// Muted or failed items always go to the end of the list, period.
		return true;
	}
	else if(i1->hasFocus())
	{
		// The item with user focus always comes to the front of the list, period.
		return true;
	}
	else if(i2->hasFocus())
	{
		// The item with user focus always comes to the front of the list, period.
		return false;
	}
	else if(i1->isParcelMedia())
	{
		// The parcel media impl sorts above all other inworld media, unless one has focus.
		return true;
	}
	else if(i2->isParcelMedia())
	{
		// The parcel media impl sorts above all other inworld media, unless one has focus.
		return false;
	}
	else if(i1->getUsedInUI() && !i2->getUsedInUI())
	{
		// i1 is a UI element, i2 is not.  This makes i1 "less than" i2, so it sorts earlier in our list.
		return true;
	}
	else if(i2->getUsedInUI() && !i1->getUsedInUI())
	{
		// i2 is a UI element, i1 is not.  This makes i2 "less than" i1, so it sorts earlier in our list.
		return false;
	}
	else if(i1->isPlayable() && !i2->isPlayable())
	{
		// Playable items sort above ones that wouldn't play even if they got high enough priority
		return true;
	}
	else if(!i1->isPlayable() && i2->isPlayable())
	{
		// Playable items sort above ones that wouldn't play even if they got high enough priority
		return false;
	}
	else if(i1->getInterest() == i2->getInterest())
	{
		// Generally this will mean both objects have zero interest.  In this case, sort on distance.
		return (i1->getProximityDistance() < i2->getProximityDistance());
	}
	else
	{
		// The object with the larger interest value should be earlier in the list, so we reverse the sense of the comparison here.
		return (i1->getInterest() > i2->getInterest());
	}
}

static bool proximity_comparitor(const LLViewerMediaImpl* i1, const LLViewerMediaImpl* i2)
{
	return (i1->getProximityDistance() < i2->getProximityDistance());
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateMedia()
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->update();
		pimpl->calculateInterest();
	}
		
	// Sort the static instance list using our interest criteria
	std::stable_sort(sViewerMediaImplList.begin(), sViewerMediaImplList.end(), priorityComparitor);

	// Go through the list again and adjust according to priority.
	iter = sViewerMediaImplList.begin();
	end = sViewerMediaImplList.end();
	
	F64 total_cpu = 0.0f;
	int impl_count_total = 0;
	int impl_count_interest_low = 0;
	int impl_count_interest_normal = 0;
	
	std::vector<LLViewerMediaImpl*> proximity_order;
	
	U32 max_instances = gSavedSettings.getU32("PluginInstancesTotal");
	U32 max_normal = gSavedSettings.getU32("PluginInstancesNormal");
	U32 max_low = gSavedSettings.getU32("PluginInstancesLow");
	F32 max_cpu = gSavedSettings.getF32("PluginInstancesCPULimit");
	// Setting max_cpu to 0.0 disables CPU usage checking.
	bool check_cpu_usage = (max_cpu != 0.0f);
	
	// Notes on tweakable params:
	// max_instances must be set high enough to allow the various instances used in the UI (for the help browser, search, etc.) to be loaded.
	// If max_normal + max_low is less than max_instances, things will tend to get unloaded instead of being set to slideshow.
	
	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		
		LLPluginClassMedia::EPriority new_priority = LLPluginClassMedia::PRIORITY_NORMAL;

		if(pimpl->isForcedUnloaded() || (impl_count_total > (int)max_instances))
		{
			// Never load muted or failed impls.
			// Hard limit on the number of instances that will be loaded at one time
			new_priority = LLPluginClassMedia::PRIORITY_UNLOADED;
		}
		else if(!pimpl->getVisible())
		{
			new_priority = LLPluginClassMedia::PRIORITY_HIDDEN;
		}
		else if(pimpl->hasFocus())
		{
			new_priority = LLPluginClassMedia::PRIORITY_HIGH;
			impl_count_interest_normal++;	// count this against the count of "normal" instances for priority purposes
		}
		else if(pimpl->getUsedInUI())
		{
			new_priority = LLPluginClassMedia::PRIORITY_NORMAL;
			impl_count_interest_normal++;
		}
		else
		{
			// Look at interest and CPU usage for instances that aren't in any of the above states.
			
			// Heuristic -- if the media texture's approximate screen area is less than 1/4 of the native area of the texture,
			// turn it down to low instead of normal.  This may downsample for plugins that support it.
			bool media_is_small = false;
			F64 approximate_interest = pimpl->getApproximateTextureInterest();
			if(approximate_interest == 0.0f)
			{
				// this media has no current size, which probably means it's not loaded.
				media_is_small = true;
			}
			else if(pimpl->getInterest() < (approximate_interest / 4))
			{
				media_is_small = true;
			}
			
			if(pimpl->getInterest() == 0.0f)
			{
				// This media is completely invisible, due to being outside the view frustrum or out of range.
				new_priority = LLPluginClassMedia::PRIORITY_HIDDEN;
			}
			else if(check_cpu_usage && (total_cpu > max_cpu))
			{
				// Higher priority plugins have already used up the CPU budget.  Set remaining ones to slideshow priority.
				new_priority = LLPluginClassMedia::PRIORITY_SLIDESHOW;
			}
			else if((impl_count_interest_normal < (int)max_normal) && !media_is_small)
			{
				// Up to max_normal inworld get normal priority
				new_priority = LLPluginClassMedia::PRIORITY_NORMAL;
				impl_count_interest_normal++;
			}
			else if (impl_count_interest_low + impl_count_interest_normal < (int)max_low + (int)max_normal)
			{
				// The next max_low inworld get turned down
				new_priority = LLPluginClassMedia::PRIORITY_LOW;
				impl_count_interest_low++;
				
				// Set the low priority size for downsampling to approximately the size the texture is displayed at.
				{
					F32 approximate_interest_dimension = fsqrtf(pimpl->getInterest());
					
					pimpl->setLowPrioritySizeLimit(llround(approximate_interest_dimension));
				}
			}
			else
			{
				// Any additional impls (up to max_instances) get very infrequent time
				new_priority = LLPluginClassMedia::PRIORITY_SLIDESHOW;
			}
		}
		
		if(!pimpl->getUsedInUI() && (new_priority != LLPluginClassMedia::PRIORITY_UNLOADED))
		{
			impl_count_total++;
		}
		
		pimpl->setPriority(new_priority);
		
		if(pimpl->getUsedInUI())
		{
			// Any impls used in the UI should not be in the proximity list.
			pimpl->mProximity = -1;
		}
		else
		{
			proximity_order.push_back(pimpl);
		}

		total_cpu += pimpl->getCPUUsage();
	}
	
	if(gSavedSettings.getBOOL("MediaPerformanceManagerDebug"))
	{
		// Give impls the same ordering as the priority list
		// they're already in the right order for this.
	}
	else
	{
		// Use a distance-based sort for proximity values.  
		std::stable_sort(proximity_order.begin(), proximity_order.end(), proximity_comparitor);
	}

	// Transfer the proximity order to the proximity fields in the objects.
	for(int i = 0; i < (int)proximity_order.size(); i++)
	{
		proximity_order[i]->mProximity = i;
	}
	
	LL_DEBUGS("PluginPriority") << "Total reported CPU usage is " << total_cpu << llendl;

}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::cleanupClass()
{
	// This is no longer necessary, since sViewerMediaImplList is no longer smart pointers.
}

//////////////////////////////////////////////////////////////////////////////////////////
// LLViewerMediaImpl
//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaImpl::LLViewerMediaImpl(	  const LLUUID& texture_id, 
										  S32 media_width, 
										  S32 media_height, 
										  U8 media_auto_scale, 
										  U8 media_loop)
:	
	mMediaSource( NULL ),
	mMovieImageHasMips(false),
	mTextureId(texture_id),
	mMediaWidth(media_width),
	mMediaHeight(media_height),
	mMediaAutoScale(media_auto_scale),
	mMediaLoop(media_loop),
	mNeedsNewTexture(true),
	mTextureUsedWidth(0),
	mTextureUsedHeight(0),
	mSuspendUpdates(false),
	mVisible(true),
	mLastSetCursor( UI_CURSOR_ARROW ),
	mMediaNavState( MEDIANAVSTATE_NONE ),
	mInterest(0.0f),
	mUsedInUI(false),
	mHasFocus(false),
	mPriority(LLPluginClassMedia::PRIORITY_UNLOADED),
	mNavigateRediscoverType(false),
	mNavigateServerRequest(false),
	mMediaSourceFailed(false),
	mRequestedVolume(1.0f),
	mIsMuted(false),
	mNeedsMuteCheck(false),
	mPreviousMediaState(MEDIA_NONE),
	mPreviousMediaTime(0.0f),
	mIsDisabled(false),
	mIsParcelMedia(false),
	mProximity(-1),
	mProximityDistance(0.0f),
	mMimeTypeProbe(NULL),
	mIsUpdated(false)
{ 

	// Set up the mute list observer if it hasn't been set up already.
	if(!sViewerMediaMuteListObserverInitialized)
	{
		LLMuteList::getInstance()->addObserver(&sViewerMediaMuteListObserver);
		sViewerMediaMuteListObserverInitialized = true;
	}
	
	add_media_impl(this);
	
	// connect this media_impl to the media texture, creating it if it doesn't exist.0
	// This is necessary because we need to be able to use getMaxVirtualSize() even if the media plugin is not loaded.
	LLViewerMediaTexture* media_tex = LLViewerTextureManager::getMediaTexture(mTextureId);
	if(media_tex)
	{
		media_tex->setMediaImpl();
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaImpl::~LLViewerMediaImpl()
{
	if( gEditMenuHandler == this )
	{
		gEditMenuHandler = NULL;
	}
	
	destroyMediaSource();
	
	LLViewerMediaTexture::removeMediaImplFromTexture(mTextureId) ;

	remove_media_impl(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::emitEvent(LLPluginClassMedia* plugin, LLViewerMediaObserver::EMediaEvent event)
{
	// Broadcast to observers using the superclass version
	LLViewerMediaEventEmitter::emitEvent(plugin, event);
	
	// If this media is on one or more LLVOVolume objects, tell them about the event as well.
	std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
	while(iter != mObjectList.end())
	{
		LLVOVolume *self = *iter;
		++iter;
		self->mediaEvent(this, plugin, event);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::initializeMedia(const std::string& mime_type)
{
	bool mimeTypeChanged = (mMimeType != mime_type);
	bool pluginChanged = (LLMIMETypes::implType(mMimeType) != LLMIMETypes::implType(mime_type));
	
	if(!mMediaSource || pluginChanged)
	{
		// We don't have a plugin at all, or the new mime type is handled by a different plugin than the old mime type.
		(void)initializePlugin(mime_type);
	}
	else if(mimeTypeChanged)
	{
		// The same plugin should be able to handle the new media -- just update the stored mime type.
		mMimeType = mime_type;
	}

	return (mMediaSource != NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::createMediaSource()
{
	if(mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		// This media shouldn't be created yet.
		return;
	}
	
	if(! mMediaURL.empty())
	{
		navigateInternal();
	}
	else if(! mMimeType.empty())
	{
		initializeMedia(mMimeType);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::destroyMediaSource()
{
	mNeedsNewTexture = true;

	// Tell the viewer media texture it's no longer active
	LLViewerMediaTexture* oldImage = LLViewerTextureManager::findMediaTexture( mTextureId );
	if (oldImage)
	{
		oldImage->setPlaying(FALSE) ;
	}
	
	cancelMimeTypeProbe();
	
	if(mMediaSource)
	{
		delete mMediaSource;
		mMediaSource = NULL;
	}	
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setMediaType(const std::string& media_type)
{
	mMimeType = media_type;
}

//////////////////////////////////////////////////////////////////////////////////////////
/*static*/
LLPluginClassMedia* LLViewerMediaImpl::newSourceFromMediaType(std::string media_type, LLPluginClassMediaOwner *owner /* may be NULL */, S32 default_width, S32 default_height)
{
	std::string plugin_basename = LLMIMETypes::implType(media_type);
	
	if(plugin_basename.empty())
	{
		LL_WARNS("Media") << "Couldn't find plugin for media type " << media_type << LL_ENDL;
	}
	else
	{
		std::string plugins_path = gDirUtilp->getLLPluginDir();
		plugins_path += gDirUtilp->getDirDelimiter();
		
		std::string launcher_name = gDirUtilp->getLLPluginLauncher();
		std::string plugin_name = gDirUtilp->getLLPluginFilename(plugin_basename);

		// See if the plugin executable exists
		llstat s;
		if(LLFile::stat(launcher_name, &s))
		{
			LL_WARNS("Media") << "Couldn't find launcher at " << launcher_name << LL_ENDL;
		}
		else if(LLFile::stat(plugin_name, &s))
		{
			LL_WARNS("Media") << "Couldn't find plugin at " << plugin_name << LL_ENDL;
		}
		else
		{
			LLPluginClassMedia* media_source = new LLPluginClassMedia(owner);
			media_source->setSize(default_width, default_height);
			if (media_source->init(launcher_name, plugin_name, gSavedSettings.getBOOL("PluginAttachDebuggerToPlugins")))
			{
				return media_source;
			}
			else
			{
				LL_WARNS("Media") << "Failed to init plugin.  Destroying." << LL_ENDL;
				delete media_source;
			}
		}
	}
	
	LL_WARNS("Plugin") << "plugin intialization failed for mime type: " << media_type << LL_ENDL;
	LLSD args;
	args["MIME_TYPE"] = media_type;
	LLNotifications::instance().add("NoPlugin", args);

	return NULL;
}							

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::initializePlugin(const std::string& media_type)
{
	if(mMediaSource)
	{
		// Save the previous media source's last set size before destroying it.
		mMediaWidth = mMediaSource->getSetWidth();
		mMediaHeight = mMediaSource->getSetHeight();
	}
	
	// Always delete the old media impl first.
	destroyMediaSource();
	
	// and unconditionally set the mime type
	mMimeType = media_type;

	if(mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		// This impl should not be loaded at this time.
		LL_DEBUGS("PluginPriority") << this << "Not loading (PRIORITY_UNLOADED)" << LL_ENDL;
		
		return false;
	}

	// If we got here, we want to ignore previous init failures.
	mMediaSourceFailed = false;

	LLPluginClassMedia* media_source = newSourceFromMediaType(mMimeType, this, mMediaWidth, mMediaHeight);
	
	if (media_source)
	{
		media_source->setDisableTimeout(gSavedSettings.getBOOL("DebugPluginDisableTimeout"));
		media_source->setLoop(mMediaLoop);
		media_source->setAutoScale(mMediaAutoScale);
		media_source->setBrowserUserAgent(LLViewerMedia::getCurrentUserAgent());
		media_source->focus(mHasFocus);
		
		mMediaSource = media_source;

		updateVolume();

		return true;
	}

	// Make sure the timer doesn't try re-initing this plugin repeatedly until something else changes.
	mMediaSourceFailed = true;

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::loadURI()
{
	if(mMediaSource)
	{
		mMediaSource->loadURI( mMediaURL );

		if(mPreviousMediaState == MEDIA_PLAYING)
		{
			// This media was playing before this instance was unloaded.

			if(mPreviousMediaTime != 0.0f)
			{
				// Seek back to where we left off, if possible.
				seek(mPreviousMediaTime);
			}
			
			start();
		}
		else if(mPreviousMediaState == MEDIA_PAUSED)
		{
			// This media was paused before this instance was unloaded.

			if(mPreviousMediaTime != 0.0f)
			{
				// Seek back to where we left off, if possible.
				seek(mPreviousMediaTime);
			}
			
			pause();
		}
		else
		{
			// No relevant previous media play state -- if we're loading the URL, we want to start playing.
			start();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setSize(int width, int height)
{
	mMediaWidth = width;
	mMediaHeight = height;
	if(mMediaSource)
	{
		mMediaSource->setSize(width, height);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::play()
{
	// If the media source isn't there, try to initialize it and load an URL.
	if(mMediaSource == NULL)
	{
	 	if(!initializeMedia(mMimeType))
		{
			// This may be the case where the plugin's priority is PRIORITY_UNLOADED
			return;
		}
		
		// Only do this if the media source was just loaded.
		loadURI();
	}
	
	// always start the media
	start();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::stop()
{
	if(mMediaSource)
	{
		if(mMediaSource->pluginSupportsMediaBrowser())
		{
			mMediaSource->browse_stop();
		}
		else
		{
			mMediaSource->stop();
		}

		// destroyMediaSource();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::pause()
{
	if(mMediaSource)
	{
		mMediaSource->pause();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::start()
{
	if(mMediaSource)
	{
		mMediaSource->start();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::seek(F32 time)
{
	if(mMediaSource)
	{
		mMediaSource->seek(time);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setVolume(F32 volume)
{
	mRequestedVolume = volume;
	updateVolume();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateVolume()
{
	if(mMediaSource)
	{
		mMediaSource->setVolume(mRequestedVolume * LLViewerMedia::getVolume());
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
F32 LLViewerMediaImpl::getVolume()
{
	return mRequestedVolume;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::focus(bool focus)
{
	mHasFocus = focus;
	
	if (mMediaSource)
	{
		// call focus just for the hell of it, even though this apopears to be a nop
		mMediaSource->focus(focus);
		if (focus)
		{
			// spoof a mouse click to *actually* pass focus
			// Don't do this anymore -- it actually clicks through now.
//			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, 1, 1, 0);
//			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, 1, 1, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::hasFocus() const
{
	// FIXME: This might be able to be a bit smarter by hooking into LLViewerMediaFocus, etc.
	return mHasFocus;
}

std::string LLViewerMediaImpl::getCurrentMediaURL()
{
	if(!mCurrentMediaURL.empty())
	{
		return mCurrentMediaURL;
	}
	
	return mMediaURL;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDown(S32 x, S32 y, MASK mask, S32 button)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
//	llinfos << "mouse down (" << x << ", " << y << ")" << llendl;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, button, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseUp(S32 x, S32 y, MASK mask, S32 button)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
//	llinfos << "mouse up (" << x << ", " << y << ")" << llendl;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, button, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseMove(S32 x, S32 y, MASK mask)
{
    scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
//	llinfos << "mouse move (" << x << ", " << y << ")" << llendl;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_MOVE, 0, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDown(const LLVector2& texture_coords, MASK mask, S32 button)
{
	if(mMediaSource)
	{
		// scale x and y to texel units.
		S32 x = llround(texture_coords.mV[VX] * mMediaSource->getTextureWidth());
		S32 y = llround((1.0f - texture_coords.mV[VY]) * mMediaSource->getTextureHeight());

		// Adjust for the difference between the actual texture height and the amount of the texture in use.
		y -= (mMediaSource->getTextureHeight() - mMediaSource->getHeight());

		mouseDown(x, y, mask, button);
	}
}

void LLViewerMediaImpl::mouseUp(const LLVector2& texture_coords, MASK mask, S32 button)
{
	if(mMediaSource)
	{		
		// scale x and y to texel units.
		S32 x = llround(texture_coords.mV[VX] * mMediaSource->getTextureWidth());
		S32 y = llround((1.0f - texture_coords.mV[VY]) * mMediaSource->getTextureHeight());

		// Adjust for the difference between the actual texture height and the amount of the texture in use.
		y -= (mMediaSource->getTextureHeight() - mMediaSource->getHeight());

		mouseUp(x, y, mask, button);
	}
}

void LLViewerMediaImpl::mouseMove(const LLVector2& texture_coords, MASK mask)
{
	if(mMediaSource)
	{		
		// scale x and y to texel units.
		S32 x = llround(texture_coords.mV[VX] * mMediaSource->getTextureWidth());
		S32 y = llround((1.0f - texture_coords.mV[VY]) * mMediaSource->getTextureHeight());

		// Adjust for the difference between the actual texture height and the amount of the texture in use.
		y -= (mMediaSource->getTextureHeight() - mMediaSource->getHeight());

		mouseMove(x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDoubleClick(S32 x, S32 y, MASK mask, S32 button)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOUBLE_CLICK, button, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::scrollWheel(S32 x, S32 y, MASK mask)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		mMediaSource->scrollEvent(x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::onMouseCaptureLost()
{
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, 0, mLastMouseX, mLastMouseY, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
BOOL LLViewerMediaImpl::handleMouseUp(S32 x, S32 y, MASK mask) 
{ 
	// NOTE: this is called when the mouse is released when we have capture.
	// Due to the way mouse coordinates are mapped to the object, we can't use the x and y coordinates that come in with the event.
	
	if(hasMouseCapture())
	{
		// Release the mouse -- this will also send a mouseup to the media
		gFocusMgr.setMouseCapture( FALSE );
	}

	return TRUE; 
}

//////////////////////////////////////////////////////////////////////////////////////////
std::string LLViewerMediaImpl::getName() const 
{ 
	if (mMediaSource)
	{
		return mMediaSource->getMediaName();
	}
	
	return LLStringUtil::null; 
};

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateBack()
{
	if (mMediaSource)
	{
		if(mMediaSource->pluginSupportsMediaTime())
		{
			F64 step_scale = 0.02; // temp , can be changed
			F64 back_step = mMediaSource->getCurrentTime() - (mMediaSource->getDuration()*step_scale);
			if(back_step < 0.0)
			{
				back_step = 0.0;
			}
			mMediaSource->seek(back_step);
			//mMediaSource->start(-2.0);
		}
		else
		{
			mMediaSource->browse_back();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateForward()
{
	if (mMediaSource)
	{
		if(mMediaSource->pluginSupportsMediaTime())
		{
			F64 step_scale = 0.02; // temp , can be changed
			F64 forward_step = mMediaSource->getCurrentTime() + (mMediaSource->getDuration()*step_scale);
			if(forward_step > mMediaSource->getDuration())
			{
				forward_step = mMediaSource->getDuration();
			}
			mMediaSource->seek(forward_step);
			//mMediaSource->start(2.0);
		}
		else
		{
			mMediaSource->browse_forward();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateReload()
{
	navigateTo(getCurrentMediaURL(), "", true, false);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateHome()
{
	navigateTo(mHomeURL, "", true, false);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::unload()
{
	// Unload the media impl and clear its state.
	destroyMediaSource();
	resetPreviousMediaState();
	mMediaURL.clear();
	mMimeType.clear();
	mCurrentMediaURL.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateTo(const std::string& url, const std::string& mime_type,  bool rediscover_type, bool server_request)
{
	cancelMimeTypeProbe();

	if(mMediaURL != url)
	{
		// Don't carry media play state across distinct URLs.
		resetPreviousMediaState();
	}
	
	// Always set the current URL and MIME type.
	mMediaURL = url;
	mMimeType = mime_type;
	
	// Clear the current media URL, since it will no longer be correct.
	mCurrentMediaURL.clear();
	
	// if mime type discovery was requested, we'll need to do it when the media loads
	mNavigateRediscoverType = rediscover_type;
	
	// and if this was a server request, the navigate on load will also need to be one.
	mNavigateServerRequest = server_request;
	
	// An explicit navigate resets the "failed" flag.
	mMediaSourceFailed = false;

	if(mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		// Helpful to have media urls in log file. Shouldn't be spammy.
		llinfos << "NOT LOADING media id= " << mTextureId << " url=" << url << " mime_type=" << mime_type << llendl;

		// This impl should not be loaded at this time.
		LL_DEBUGS("PluginPriority") << this << "Not loading (PRIORITY_UNLOADED)" << LL_ENDL;
		
		return;
	}

	navigateInternal();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateInternal()
{
	// Helpful to have media urls in log file. Shouldn't be spammy.
	llinfos << "media id= " << mTextureId << " url=" << mMediaURL << " mime_type=" << mMimeType << llendl;

	if(mMimeTypeProbe != NULL)
	{
		llwarns << "MIME type probe already in progress -- bailing out." << llendl;
		return;
	}
	
	if(mNavigateServerRequest)
	{
		setNavState(MEDIANAVSTATE_SERVER_SENT);
	}
	else
	{
		setNavState(MEDIANAVSTATE_NONE);
	}
			
	// If the caller has specified a non-empty MIME type, look that up in our MIME types list.
	// If we have a plugin for that MIME type, use that instead of attempting auto-discovery.
	// This helps in supporting legacy media content where the server the media resides on returns a bogus MIME type
	// but the parcel owner has correctly set the MIME type in the parcel media settings.
	
	if(!mMimeType.empty() && (mMimeType != "none/none"))
	{
		std::string plugin_basename = LLMIMETypes::implType(mMimeType);
		if(!plugin_basename.empty())
		{
			// We have a plugin for this mime type
			mNavigateRediscoverType = false;
		}
	}

	if(mNavigateRediscoverType)
	{

		LLURI uri(mMediaURL);
		std::string scheme = uri.scheme();

		if(scheme.empty() || "http" == scheme || "https" == scheme)
		{
			LLHTTPClient::getHeaderOnly( mMediaURL, new LLMimeDiscoveryResponder(this), 10.0f);
		}
		else if("data" == scheme || "file" == scheme || "about" == scheme)
		{
			// FIXME: figure out how to really discover the type for these schemes
			// We use "data" internally for a text/html url for loading the login screen
			if(initializeMedia("text/html"))
			{
				loadURI();
			}
		}
		else
		{
			// This catches 'rtsp://' urls
			if(initializeMedia(scheme))
			{
				loadURI();
			}
		}
	}
	else if(initializeMedia(mMimeType))
	{
		loadURI();
	}
	else
	{
		LL_WARNS("Media") << "Couldn't navigate to: " << mMediaURL << " as there is no media type for: " << mMimeType << LL_ENDL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateStop()
{
	if(mMediaSource)
	{
		mMediaSource->browse_stop();
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleKeyHere(KEY key, MASK mask)
{
	bool result = false;
	// *NOTE:Mani - if this doesn't exist llmozlib goes crashy in the debug build.
	// LLMozlib::init wants to write some files to <exe_dir>/components
	std::string debug_init_component_dir( gDirUtilp->getExecutableDir() );
	debug_init_component_dir += "/components";
	LLAPRFile::makeDir(debug_init_component_dir.c_str()); 
	
	if (mMediaSource)
	{
		// FIXME: THIS IS SO WRONG.
		// Menu keys should be handled by the menu system and not passed to UI elements, but this is how LLTextEditor and LLLineEditor do it...
		if( MASK_CONTROL & mask )
		{
			if( 'C' == key )
			{
				mMediaSource->copy();
				result = true;
			}
			else
			if( 'V' == key )
			{
				mMediaSource->paste();
				result = true;
			}
			else
			if( 'X' == key )
			{
				mMediaSource->cut();
				result = true;
			}
		}
		
		if(!result)
		{
			result = mMediaSource->keyEvent(LLPluginClassMedia::KEY_EVENT_DOWN ,key, mask);
			// Since the viewer internal event dispatching doesn't give us key-up events, simulate one here.
			(void)mMediaSource->keyEvent(LLPluginClassMedia::KEY_EVENT_UP ,key, mask);
		}
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleUnicodeCharHere(llwchar uni_char)
{
	bool result = false;
	
	if (mMediaSource)
	{
		// only accept 'printable' characters, sigh...
		if (uni_char >= 32 // discard 'control' characters
			&& uni_char != 127) // SDL thinks this is 'delete' - yuck.
		{
			mMediaSource->textInput(wstring_to_utf8str(LLWString(1, uni_char)), gKeyboard->currentMask(FALSE));
		}
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::canNavigateForward()
{
	BOOL result = FALSE;
	if (mMediaSource)
	{
		result = mMediaSource->getHistoryForwardAvailable();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::canNavigateBack()
{
	BOOL result = FALSE;
	if (mMediaSource)
	{
		result = mMediaSource->getHistoryBackAvailable();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::update()
{
	if(mMediaSource == NULL)
	{
		if(mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
		{
			// This media source should not be loaded.
		}
		else if(mPriority <= LLPluginClassMedia::PRIORITY_SLIDESHOW)
		{
			// Don't load new instances that are at PRIORITY_SLIDESHOW or below.  They're just kept around to preserve state.
		}
		else if(mMimeTypeProbe != NULL)
		{
			// this media source is doing a MIME type probe -- don't try loading it again.
		}
		else
		{
			// This media may need to be loaded.
			if(sMediaCreateTimer.hasExpired())
			{
				LL_DEBUGS("PluginPriority") << this << ": creating media based on timer expiration" << LL_ENDL;
				createMediaSource();
				sMediaCreateTimer.setTimerExpirySec(LLVIEWERMEDIA_CREATE_DELAY);
			}
			else
			{
				LL_DEBUGS("PluginPriority") << this << ": NOT creating media (waiting on timer)" << LL_ENDL;
			}
		}
	}
	
	if(mMediaSource == NULL)
	{
		return;
	}
	
	mMediaSource->idle();
	
	if(mMediaSource->isPluginExited())
	{
		resetPreviousMediaState();
		destroyMediaSource();
		return;
	}

	if(!mMediaSource->textureValid())
	{
		return;
	}
	
	if(mSuspendUpdates || !mVisible)
	{
		return;
	}
	
	LLViewerMediaTexture* placeholder_image = updatePlaceholderImage();
		
	if(placeholder_image)
	{
		LLRect dirty_rect;
		
		// Since we're updating this texture, we know it's playing.  Tell the texture to do its replacement magic so it gets rendered.
		placeholder_image->setPlaying(TRUE);

		if(mMediaSource->getDirty(&dirty_rect))
		{
			// Constrain the dirty rect to be inside the texture
			S32 x_pos = llmax(dirty_rect.mLeft, 0);
			S32 y_pos = llmax(dirty_rect.mBottom, 0);
			S32 width = llmin(dirty_rect.mRight, placeholder_image->getWidth()) - x_pos;
			S32 height = llmin(dirty_rect.mTop, placeholder_image->getHeight()) - y_pos;
			
			if(width > 0 && height > 0)
			{

				U8* data = mMediaSource->getBitsData();

				// Offset the pixels pointer to match x_pos and y_pos
				data += ( x_pos * mMediaSource->getTextureDepth() * mMediaSource->getBitsWidth() );
				data += ( y_pos * mMediaSource->getTextureDepth() );
				
				placeholder_image->setSubImage(
						data, 
						mMediaSource->getBitsWidth(), 
						mMediaSource->getBitsHeight(),
						x_pos, 
						y_pos, 
						width, 
						height);

			}
			
			mMediaSource->resetDirty();
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateImagesMediaStreams()
{
}


//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaTexture* LLViewerMediaImpl::updatePlaceholderImage()
{
	if(mTextureId.isNull())
	{
		// The code that created this instance will read from the plugin's bits.
		return NULL;
	}
	
	LLViewerMediaTexture* placeholder_image = LLViewerTextureManager::getMediaTexture( mTextureId );
	
	if (mNeedsNewTexture 
		|| placeholder_image->getUseMipMaps()
		|| (placeholder_image->getWidth() != mMediaSource->getTextureWidth())
		|| (placeholder_image->getHeight() != mMediaSource->getTextureHeight())
		|| (mTextureUsedWidth > mMediaSource->getWidth())
		|| (mTextureUsedHeight > mMediaSource->getHeight())
		)
	{
		LL_DEBUGS("Media") << "initializing media placeholder" << LL_ENDL;
		LL_DEBUGS("Media") << "movie image id " << mTextureId << LL_ENDL;

		int texture_width = mMediaSource->getTextureWidth();
		int texture_height = mMediaSource->getTextureHeight();
		int texture_depth = mMediaSource->getTextureDepth();
		
		// MEDIAOPT: check to see if size actually changed before doing work
		placeholder_image->destroyGLTexture();
		// MEDIAOPT: apparently just calling setUseMipMaps(FALSE) doesn't work?
		placeholder_image->reinit(FALSE);	// probably not needed

		// MEDIAOPT: seems insane that we actually have to make an imageraw then
		// immediately discard it
		LLPointer<LLImageRaw> raw = new LLImageRaw(texture_width, texture_height, texture_depth);
		raw->clear(0x00, 0x00, 0x00, 0xff);
		int discard_level = 0;

		// ask media source for correct GL image format constants
		placeholder_image->setExplicitFormat(mMediaSource->getTextureFormatInternal(),
											 mMediaSource->getTextureFormatPrimary(),
											 mMediaSource->getTextureFormatType(),
											 mMediaSource->getTextureFormatSwapBytes());

		placeholder_image->createGLTexture(discard_level, raw);

		// MEDIAOPT: set this dynamically on play/stop
		// FIXME
//		placeholder_image->mIsMediaTexture = true;
		mNeedsNewTexture = false;
				
		// If the amount of the texture being drawn by the media goes down in either width or height, 
		// recreate the texture to avoid leaving parts of the old image behind.
		mTextureUsedWidth = mMediaSource->getWidth();
		mTextureUsedHeight = mMediaSource->getHeight();
	}
	
	return placeholder_image;
}


//////////////////////////////////////////////////////////////////////////////////////////
LLUUID LLViewerMediaImpl::getMediaTextureID() const
{
	return mTextureId;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setVisible(bool visible)
{
	mVisible = visible;
	
	if(mVisible)
	{
		if(mMediaSource && mMediaSource->isPluginExited())
		{
			destroyMediaSource();
		}
		
		if(!mMediaSource)
		{
			createMediaSource();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseCapture()
{
	gFocusMgr.setMouseCapture(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::scaleMouse(S32 *mouse_x, S32 *mouse_y)
{
#if 0
	S32 media_width, media_height;
	S32 texture_width, texture_height;
	getMediaSize( &media_width, &media_height );
	getTextureSize( &texture_width, &texture_height );
	S32 y_delta = texture_height - media_height;

	*mouse_y -= y_delta;
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaPlaying()
{
	bool result = false;
	
	if(mMediaSource)
	{
		EMediaStatus status = mMediaSource->getStatus();
		if(status == MEDIA_PLAYING || status == MEDIA_LOADING)
			result = true;
	}
	
	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaPaused()
{
	bool result = false;

	if(mMediaSource)
	{
		if(mMediaSource->getStatus() == MEDIA_PAUSED)
			result = true;
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::hasMedia() const
{
	return mMediaSource != NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void LLViewerMediaImpl::resetPreviousMediaState()
{
	mPreviousMediaState = MEDIA_NONE;
	mPreviousMediaTime = 0.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isForcedUnloaded() const
{
	if(mIsMuted || mMediaSourceFailed || mIsDisabled)
	{
		return true;
	}
	
	if(sInWorldMediaDisabled)
	{
		// When inworld media is disabled, all instances that aren't marked as "used in UI" will not be loaded.
		if(!mUsedInUI)
		{
			return true;
		}
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isPlayable() const
{
	if(isForcedUnloaded())
	{
		// All of the forced-unloaded criteria also imply not playable.
		return false;
	}
	
	if(hasMedia())
	{
		// Anything that's already playing is, by definition, playable.
		return true;
	}
	
	if(!mMediaURL.empty())
	{
		// If something has navigated the instance, it's ready to be played.
		return true;
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::handleMediaEvent(LLPluginClassMedia* plugin, LLPluginClassMediaOwner::EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			// The plugin failed to load properly.  Make sure the timer doesn't retry.
			// TODO: maybe mark this plugin as not loadable somehow?
			mMediaSourceFailed = true;

			// Reset the last known state of the media to defaults.
			resetPreviousMediaState();
			
			// TODO: may want a different message for this case?
			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mMimeType);
			LLNotifications::instance().add("MediaPluginFailed", args);
		}
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			// The plugin crashed.
			mMediaSourceFailed = true;

			// Reset the last known state of the media to defaults.
			resetPreviousMediaState();

			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mMimeType);
			// SJB: This is getting called every frame if the plugin fails to load, continuously respawining the alert!
			//LLNotifications::instance().add("MediaPluginFailed", args);
		}
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << plugin->getCursorName() << LL_ENDL;

			std::string cursor = plugin->getCursorName();
			
			if(cursor == "arrow")
				mLastSetCursor = UI_CURSOR_ARROW;
			else if(cursor == "ibeam")
				mLastSetCursor = UI_CURSOR_IBEAM;
			else if(cursor == "splith")
				mLastSetCursor = UI_CURSOR_SIZEWE;
			else if(cursor == "splitv")
				mLastSetCursor = UI_CURSOR_SIZENS;
			else if(cursor == "hand")
				mLastSetCursor = UI_CURSOR_HAND;
			else // for anything else, default to the arrow
				mLastSetCursor = UI_CURSOR_ARROW;
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_NAVIGATE_BEGIN, uri is: " << plugin->getNavigateURI() << LL_ENDL;

			if(getNavState() == MEDIANAVSTATE_SERVER_SENT)
			{
				setNavState(MEDIANAVSTATE_SERVER_BEGUN);
			}
			else
			{
				setNavState(MEDIANAVSTATE_BEGUN);
			}
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_NAVIGATE_COMPLETE, uri is: " << plugin->getNavigateURI() << LL_ENDL;

			if(getNavState() == MEDIANAVSTATE_BEGUN)
			{
				mCurrentMediaURL = plugin->getNavigateURI();
				setNavState(MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED);
			}
			else if(getNavState() == MEDIANAVSTATE_SERVER_BEGUN)
			{
				mCurrentMediaURL = plugin->getNavigateURI();
				setNavState(MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED);
			}
			else
			{
				// all other cases need to leave the state alone.
			}
		}
		break;
		
		case LLViewerMediaObserver::MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_LOCATION_CHANGED, uri is: " << plugin->getLocation() << LL_ENDL;

			if(getNavState() == MEDIANAVSTATE_BEGUN)
			{
				mCurrentMediaURL = plugin->getLocation();
				setNavState(MEDIANAVSTATE_FIRST_LOCATION_CHANGED);
			}
			else if(getNavState() == MEDIANAVSTATE_SERVER_BEGUN)
			{
				mCurrentMediaURL = plugin->getLocation();
				setNavState(MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED);
			}
			else
			{
				// Don't track redirects.
				setNavState(MEDIANAVSTATE_NONE);
			}
		}
		break;

		
		default:
		break;
	}

	// Just chain the event to observers.
	emitEvent(plugin, event);
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::cut()
{
	if (mMediaSource)
		mMediaSource->cut();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canCut() const
{
	if (mMediaSource)
		return mMediaSource->canCut();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::copy()
{
	if (mMediaSource)
		mMediaSource->copy();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canCopy() const
{
	if (mMediaSource)
		return mMediaSource->canCopy();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::paste()
{
	if (mMediaSource)
		mMediaSource->paste();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canPaste() const
{
	if (mMediaSource)
		return mMediaSource->canPaste();
	else
		return FALSE;
}

void LLViewerMediaImpl::setUpdated(BOOL updated)
{
	mIsUpdated = updated ;
}

BOOL LLViewerMediaImpl::isUpdated()
{
	return mIsUpdated ;
}

void LLViewerMediaImpl::calculateInterest()
{
	LLViewerMediaTexture* texture = LLViewerTextureManager::findMediaTexture( mTextureId );
	
	if(texture != NULL)
	{
		mInterest = texture->getMaxVirtualSize();
	}
	else
	{
		// This will be a relatively common case now, since it will always be true for unloaded media.
		mInterest = 0.0f;
	}
	
	// Calculate distance from the avatar, for use in the proximity calculation.
	mProximityDistance = 0.0f;
	if(!mObjectList.empty())
	{
		// Just use the first object in the list.  We could go through the list and find the closest object, but this should work well enough.
		LLVector3d global_delta = gAgent.getPositionGlobal() - (*mObjectList.begin())->getPositionGlobal();
		mProximityDistance = global_delta.magVecSquared();  // use distance-squared because it's cheaper and sorts the same.
	}
	
	if(mNeedsMuteCheck)
	{
		// Check all objects this instance is associated with, and those objects' owners, against the mute list
		mIsMuted = false;
		
		std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
		for(; iter != mObjectList.end() ; ++iter)
		{
			LLVOVolume *obj = *iter;
			if(LLMuteList::getInstance()->isMuted(obj->getID()))
				mIsMuted = true;
			else
			{
				// We won't have full permissions data for all objects.  Attempt to mute objects when we can tell their owners are muted.
				LLPermissions* obj_perm = LLSelectMgr::getInstance()->findObjectPermissions(obj);
				if(obj_perm)
				{
					if(LLMuteList::getInstance()->isMuted(obj_perm->getOwner()))
						mIsMuted = true;
				}
			}
		}
		
		mNeedsMuteCheck = false;
	}
}

F64 LLViewerMediaImpl::getApproximateTextureInterest()
{
	F64 result = 0.0f;
	
	if(mMediaSource)
	{
		result = mMediaSource->getFullWidth();
		result *= mMediaSource->getFullHeight();
	}
	else
	{
		// No media source is loaded -- all we have to go on is the texture size that has been set on the impl, if any.
		result = mMediaWidth;
		result *= mMediaHeight;
	}

	return result;
}

void LLViewerMediaImpl::setUsedInUI(bool used_in_ui)
{
	mUsedInUI = used_in_ui; 
	
	// HACK: Force elements used in UI to load right away.
	// This fixes some issues where UI code that uses the browser instance doesn't expect it to be unloaded.
	if(mUsedInUI && (mPriority == LLPluginClassMedia::PRIORITY_UNLOADED))
	{
		if(getVisible())
		{
			setPriority(LLPluginClassMedia::PRIORITY_NORMAL);
		}
		else
		{
			setPriority(LLPluginClassMedia::PRIORITY_HIDDEN);
		}

		createMediaSource();
	}
};

F64 LLViewerMediaImpl::getCPUUsage() const
{
	F64 result = 0.0f;
	
	if(mMediaSource)
	{
		result = mMediaSource->getCPUUsage();
	}
	
	return result;
}

void LLViewerMediaImpl::setPriority(LLPluginClassMedia::EPriority priority)
{
	if(mPriority != priority)
	{
		LL_DEBUGS("PluginPriority")
			<< "changing priority of media id " << mTextureId
			<< " from " << LLPluginClassMedia::priorityToString(mPriority)
			<< " to " << LLPluginClassMedia::priorityToString(priority)
			<< LL_ENDL;
	}
	
	mPriority = priority;
	
	if(priority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		if(mMediaSource)
		{
			// Need to unload the media source
			
			// First, save off previous media state
			mPreviousMediaState = mMediaSource->getStatus();
			mPreviousMediaTime = mMediaSource->getCurrentTime();
			
			destroyMediaSource();
		}
	}

	if(mMediaSource)
	{
		mMediaSource->setPriority(mPriority);
	}
	
	// NOTE: loading (or reloading) media sources whose priority has risen above PRIORITY_UNLOADED is done in update().
}

void LLViewerMediaImpl::setLowPrioritySizeLimit(int size)
{
	if(mMediaSource)
	{
		mMediaSource->setLowPrioritySizeLimit(size);
	}
}

void LLViewerMediaImpl::setNavState(EMediaNavState state)
{
	mMediaNavState = state;
	
	switch (state) 
	{
		case MEDIANAVSTATE_NONE: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_NONE" << llendl; break;
		case MEDIANAVSTATE_BEGUN: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_BEGUN" << llendl; break;
		case MEDIANAVSTATE_FIRST_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_FIRST_LOCATION_CHANGED" << llendl; break;
		case MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED" << llendl; break;
		case MEDIANAVSTATE_SERVER_SENT: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_SENT" << llendl; break;
		case MEDIANAVSTATE_SERVER_BEGUN: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_BEGUN" << llendl; break;
		case MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED" << llendl; break;
		case MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED" << llendl; break;
	}
}

void LLViewerMediaImpl::cancelMimeTypeProbe()
{
	if(mMimeTypeProbe != NULL)
	{
		// There doesn't seem to be a way to actually cancel an outstanding request.
		// Simulate it by telling the LLMimeDiscoveryResponder not to write back any results.
		mMimeTypeProbe->cancelRequest();
		
		// The above should already have set mMimeTypeProbe to NULL.
		if(mMimeTypeProbe != NULL)
		{
			llerrs << "internal error: mMimeTypeProbe is not NULL after cancelling request." << llendl;
		}
	}
}

void LLViewerMediaImpl::addObject(LLVOVolume* obj) 
{
	std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
	for(; iter != mObjectList.end() ; ++iter)
	{
		if(*iter == obj)
		{
			return ; //already in the list.
		}
	}

	mObjectList.push_back(obj) ;
	mNeedsMuteCheck = true;
}
	
void LLViewerMediaImpl::removeObject(LLVOVolume* obj) 
{
	mObjectList.remove(obj) ;	
	mNeedsMuteCheck = true;
}
	
const std::list< LLVOVolume* >* LLViewerMediaImpl::getObjectList() const 
{
	return &mObjectList ;
}

LLVOVolume *LLViewerMediaImpl::getSomeObject()
{
	LLVOVolume *result = NULL;
	
	std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
	if(iter != mObjectList.end())
	{
		result = *iter;
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
//static
void LLViewerMedia::toggleMusicPlay(void*)
{
// FIXME: This probably doesn't belong here
#if 0
	if (mMusicState != PLAYING)
	{
		mMusicState = PLAYING; // desired state
		if (gAudiop)
		{
			LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			if ( parcel )
			{
				gAudiop->startInternetStream(parcel->getMusicURL());
			}
		}
	}
	else
	{
		mMusicState = STOPPED; // desired state
		if (gAudiop)
		{
			gAudiop->stopInternetStream();
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
//static
void LLViewerMedia::toggleMediaPlay(void*)
{
// FIXME: This probably doesn't belong here
#if 0
	if (LLViewerMedia::isMediaPaused())
	{
		LLViewerParcelMedia::start();
	}
	else if(LLViewerMedia::isMediaPlaying())
	{
		LLViewerParcelMedia::pause();
	}
	else
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			LLViewerParcelMedia::play(parcel);
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
//static
void LLViewerMedia::mediaStop(void*)
{
// FIXME: This probably doesn't belong here
#if 0
	LLViewerParcelMedia::stop();
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
//static 
bool LLViewerMedia::isMusicPlaying()
{	
// FIXME: This probably doesn't belong here
// FIXME: make this work
	return false;	
}
