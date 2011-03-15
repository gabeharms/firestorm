/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "llviewerparcelmedia.h"

#include "llagent.h"
#include "llaudioengine.h"
#include "llmimetypes.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lluuid.h"
#include "message.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmediaautoplay.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llnotificationmanager.h"
//#include "llfirstuse.h"
#include "llpluginclassmedia.h"
#include "llviewertexture.h"
#include "lloverlaybar.h"
#include "llsdserialize.h"

// Static Variables

S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
LLUUID LLViewerParcelMedia::sMediaRegionID;
viewer_media_t LLViewerParcelMedia::sMediaImpl;
LLSD LLViewerParcelMedia::sMediaFilterList;
bool LLViewerParcelMedia::sMediaLastActionPlay = FALSE;
std::string LLViewerParcelMedia::sMediaLastURL = "";
bool LLViewerParcelMedia::sAudioLastActionPlay = FALSE;
std::string LLViewerParcelMedia::sAudioLastURL = "";

bool LLViewerParcelMedia::sMediaFilterAlertActive = FALSE;
std::string LLViewerParcelMedia::sQueuedMusic = "";
std::string LLViewerParcelMedia::sCurrentMusic = "";
LLParcel LLViewerParcelMedia::sQueuedMedia;
LLParcel LLViewerParcelMedia::sCurrentMedia;
LLParcel LLViewerParcelMedia::sCurrentAlertMedia;
bool LLViewerParcelMedia::sMediaQueueEmpty = TRUE;
bool LLViewerParcelMedia::sMusicQueueEmpty = TRUE;

// Local functions
bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_media_alert(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_audio_alert(const LLSD& notification, const LLSD& response, std::string media_url);

// static
void LLViewerParcelMedia::initClass()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->setHandlerFunc("ParcelMediaCommandMessage", processParcelMediaCommandMessage );
	msg->setHandlerFunc("ParcelMediaUpdate", processParcelMediaUpdate );
	LLViewerParcelMediaAutoPlay::initClass();
	loadDomainFilterList();
}

//static 
void LLViewerParcelMedia::cleanupClass()
{
	// This needs to be destroyed before global destructor time.
	sMediaImpl = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (/*LLViewerMedia::hasMedia()*/ true)
	{
		// we have a player
		if (parcel)
		{
			if(!gAgent.getRegion())
			{
				sMediaRegionID = LLUUID() ;
				stop() ;
				LL_DEBUGS("Media") << "no agent region, bailing out." << LL_ENDL;
				return ;				
			}

			// we're in a parcel
			bool new_parcel = false;
			S32 parcelid = parcel->getLocalID();						

			LLUUID regionid = gAgent.getRegion()->getRegionID();
			if (parcelid != sMediaParcelLocalID || regionid != sMediaRegionID)
			{
				LL_DEBUGS("Media") << "New parcel, parcel id = " << parcelid << ", region id = " << regionid << LL_ENDL;
				sMediaParcelLocalID = parcelid;
				sMediaRegionID = regionid;
				new_parcel = true;
			}

			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			std::string mediaCurrentUrl = std::string( parcel->getMediaCurrentURL());

			// First use warning
			if(!mediaUrl.empty() && gWarningSettings.getBOOL("FirstStreamingVideo"))
			{
				LLNotifications::instance().add("ParcelCanPlayMedia", LLSD(), LLSD(), 
					boost::bind(callback_play_media, _1, _2, parcel));
				return;
			}

			// if we have a current (link sharing) url, use it instead
			if (mediaCurrentUrl != "" && parcel->getMediaType() == "text/html")
			{
				mediaUrl = mediaCurrentUrl;
			}
			
			LLStringUtil::trim(mediaUrl);
			
			// If no parcel media is playing, nothing left to do
			if(sMediaImpl.isNull())

			{
				return;
			}

			// Media is playing...has something changed?
			else if (( sMediaImpl->getMediaURL() != mediaUrl )
				|| ( sMediaImpl->getMediaTextureID() != parcel->getMediaID() )
				|| ( sMediaImpl->getMimeType() != parcel->getMediaType() ))
			{
				// Only play if the media types are the same.
				if(sMediaImpl->getMimeType() == parcel->getMediaType())
				{
					if (gSavedSettings.getBOOL("MediaEnableFilter"))
					{
						llinfos << "Filtering media URL." << llendl;
						filterMediaUrl(parcel);
					}
					else
					{
						play(parcel);
					}
				}

				else
				{
					stop();
				}
			}
		}
		else
		{
			stop();
		}
	}
	/*
	else
	{
		// no audio player, do a first use dialog if there is media here
		if (parcel)
		{
			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			if (!mediaUrl.empty ())
			{
				if (gWarningSettings.getBOOL("QuickTimeInstalled"))
				{
					gWarningSettings.setBOOL("QuickTimeInstalled", FALSE);

					LLNotificationsUtil::add("NoQuickTime" );
				};
			}
		}
	}
	*/
}

// static
void LLViewerParcelMedia::play(LLParcel* parcel)
{
	llinfos << "LLViewerParcelMedia::play" << llendl;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingMedia"))
		return;

	std::string media_url = parcel->getMediaURL();
	std::string media_current_url = parcel->getMediaCurrentURL();
	std::string mime_type = parcel->getMediaType();
	LLUUID placeholder_texture_id = parcel->getMediaID();
	U8 media_auto_scale = parcel->getMediaAutoScale();
	U8 media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();

	if(sMediaImpl)
	{
		// If the url and mime type are the same, call play again
		if(sMediaImpl->getMediaURL() == media_url 
			&& sMediaImpl->getMimeType() == mime_type
			&& sMediaImpl->getMediaTextureID() == placeholder_texture_id)
		{
			LL_DEBUGS("Media") << "playing with existing url " << media_url << LL_ENDL;

			sMediaImpl->play();
		}
		// Else if the texture id's are the same, navigate and rediscover type
		// MBW -- This causes other state from the previous parcel (texture size, autoscale, and looping) to get re-used incorrectly.
		// It's also not really necessary -- just creating a new instance is fine.
//		else if(sMediaImpl->getMediaTextureID() == placeholder_texture_id)
//		{
//			sMediaImpl->navigateTo(media_url, mime_type, true);
//		}
		else
		{
			// Since the texture id is different, we need to generate a new impl

			// Delete the old one first so they don't fight over the texture.
			sMediaImpl = NULL;
			
			// A new impl will be created below.
		}
	}
	
	// Don't ever try to play if the media type is set to "none/none"
	if(stricmp(mime_type.c_str(), LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if(!sMediaImpl)
		{
			LL_DEBUGS("Media") << "new media impl with mime type " << mime_type << ", url " << media_url << LL_ENDL;

			// There is no media impl, make a new one
			sMediaImpl = LLViewerMedia::newMediaImpl(
				placeholder_texture_id,
				media_width, 
				media_height, 
				media_auto_scale,
				media_loop);
			sMediaImpl->setIsParcelMedia(true);
			sMediaImpl->navigateTo(media_url, mime_type, true);
		}

		//LLFirstUse::useMedia();

		LLViewerParcelMediaAutoPlay::playStarted();
	}
}

// static
void LLViewerParcelMedia::stop()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	
	// We need to remove the media HUD if it is up.
	LLViewerMediaFocus::getInstance()->clearFocus();

	// This will unload & kill the media instance.
	sMediaImpl = NULL;
}

// static
void LLViewerParcelMedia::pause()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->pause();
}

// static
void LLViewerParcelMedia::start()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->start();

	//LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::seek(F32 time)
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->seek(time);
}

// static
void LLViewerParcelMedia::focus(bool focus)
{
	sMediaImpl->focus(focus);
}

// static
LLPluginClassMediaOwner::EMediaStatus LLViewerParcelMedia::getStatus()
{	
	LLPluginClassMediaOwner::EMediaStatus result = LLPluginClassMediaOwner::MEDIA_NONE;
	
	if(sMediaImpl.notNull() && sMediaImpl->hasMedia())
	{
		result = sMediaImpl->getMediaPlugin()->getStatus();
	}
	
	return result;
}

// static
std::string LLViewerParcelMedia::getMimeType()
{
	return sMediaImpl.notNull() ? sMediaImpl->getMimeType() : LLMIMETypes::getDefaultMimeType();
}

//static 
std::string LLViewerParcelMedia::getURL()
{
	std::string url;
	if(sMediaImpl.notNull())
		url = sMediaImpl->getMediaURL();
	
	if(stricmp(LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaType().c_str(), LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if (url.empty())
			url = LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaCurrentURL();
		
		if (url.empty())
			url = LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaURL();
	}
	
	return url;
}

//static 
std::string LLViewerParcelMedia::getName()
{
	if(sMediaImpl.notNull())
		return sMediaImpl->getName();
	return "";
}

viewer_media_t LLViewerParcelMedia::getParcelMedia()
{
	return sMediaImpl;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaCommandMessage( LLMessageSystem *msg, void ** )
{
	// extract the agent id
	//	LLUUID agent_id;
	//	msg->getUUID( agent_id );

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32( "CommandBlock", "Flags", flags );
	msg->getU32( "CommandBlock", "Command", command);
	msg->getF32( "CommandBlock", "Time", time );

	if (flags &( (1<<PARCEL_MEDIA_COMMAND_STOP)
				| (1<<PARCEL_MEDIA_COMMAND_PAUSE)
				| (1<<PARCEL_MEDIA_COMMAND_PLAY)
				| (1<<PARCEL_MEDIA_COMMAND_LOOP)
				| (1<<PARCEL_MEDIA_COMMAND_UNLOAD) ))
	{
		// stop
		if( command == PARCEL_MEDIA_COMMAND_STOP )
		{
			stop();
		}
		else
		// pause
		if( command == PARCEL_MEDIA_COMMAND_PAUSE )
		{
			pause();
		}
		else
		// play
		if(( command == PARCEL_MEDIA_COMMAND_PLAY ) ||
		   ( command == PARCEL_MEDIA_COMMAND_LOOP ))
		{
			if (getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
			{
				start();
			}
			else
			{
				//AO: Disallow scripted media option
				if (( !gSavedSettings.getBOOL("PermAllowScriptedMedia")) && (!gSavedSettings.getBOOL("TempAllowScriptedMedia")))
				{
					llinfos << "Disallowing scripted media." <<llendl;
				}
				else 
				{
					LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
					if (gSavedSettings.getBOOL("MediaEnableFilter"))
					{
						llinfos << "PARCEL_MEDIA_COMMAND_PLAY: Filtering media URL." << llendl;
						filterMediaUrl(parcel);
					}
					else
					{
						play(parcel);
					}
				}
			}
		}
		else
		// unload
		if( command == PARCEL_MEDIA_COMMAND_UNLOAD )
		{
			stop();
		}
	}

	if (flags & (1<<PARCEL_MEDIA_COMMAND_TIME))
	{
		if(sMediaImpl.isNull())
		{
			//AO: Disallow scripted media option
			if (( !gSavedSettings.getBOOL("PermAllowScriptedMedia")) && (!gSavedSettings.getBOOL("TempAllowScriptedMedia")))
			{
				llinfos << "Disallowing scripted media." << llendl;
			}
			else 
			{
				LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					llinfos << "PARCEL_MEDIA_COMMAND_TIME: Filtering media URL." << llendl;
					filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
		seek(time);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaUpdate( LLMessageSystem *msg, void ** )
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 media_auto_scale = FALSE;
	U8 media_loop = FALSE;

	msg->getUUID( "DataBlock", "MediaID", media_id );
	char media_url_buffer[257];
	msg->getString( "DataBlock", "MediaURL", 255, media_url_buffer );
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", media_auto_scale);

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255, media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", media_loop);
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	BOOL same = FALSE;
	if (parcel)
	{
		same = ((parcel->getMediaURL() == media_url) &&
				(parcel->getMediaType() == media_type) &&
				(parcel->getMediaID() == media_id) &&
				(parcel->getMediaWidth() == media_width) &&
				(parcel->getMediaHeight() == media_height) &&
				(parcel->getMediaAutoScale() == media_auto_scale) &&
				(parcel->getMediaLoop() == media_loop));

		if (!same)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			if (sMediaImpl.notNull())
			{
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					llinfos << "Parcel media changed. Filtering media URL." << llendl;
					filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
	}
}
// Static
/////////////////////////////////////////////////////////////////////////////////////////
void LLViewerParcelMedia::sendMediaNavigateMessage(const std::string& url)
{
	std::string region_url = gAgent.getRegion()->getCapability("ParcelNavigateMedia");
	if (!region_url.empty())
	{
		// send navigate event to sim for link sharing
		LLSD body;
		body["agent-id"] = gAgent.getID();
		body["local-id"] = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
		body["url"] = url;
		LLHTTPClient::post(region_url, body, new LLHTTPClient::Responder);
	}
	else
	{
		llwarns << "can't get ParcelNavigateMedia capability" << llendl;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
// virtual 
void LLViewerParcelMedia::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_CONTENT_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CONTENT_UPDATED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_TIME_DURATION_UPDATED, time is " << self->getCurrentTime() << " of " << self->getDuration() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_SIZE_CHANGED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PROGRESS_UPDATED, loading at " << self->getProgressPercent() << "%" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: " << self->getStatusText() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LOCATION_CHANGED, new uri is: " << self->getLocation() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << self->getClickTarget() << "\", uri is " << self->getClickURL() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED_LAUNCH" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAME_CHANGED" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLOSE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PICK_FILE_REQUEST" << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE, uuid is " << self->getClickUUID() << LL_ENDL;
		}
		break;
	};
}

bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", TRUE);
		if (gSavedSettings.getBOOL("MediaEnableFilter"))
		{
			LLViewerParcelMedia::filterMediaUrl(parcel);
		}
		else
		{
			LLViewerParcelMedia::play(parcel);
		}
	}
	else
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", FALSE);
	}
	gWarningSettings.setBOOL("FirstStreamingVideo", FALSE);
	return false;
}

void LLViewerParcelMedia::filterMediaUrl(LLParcel* parcel)
{
	LLParcel *currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	llinfos << "Current media: "+sCurrentMedia.getMediaURL() << llendl;
	llinfos << "New media: "+parcel->getMediaURL() << llendl;
	// If there is no alert active, filter the media and flag media
	//  queue empty.
	if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
	{
		if (parcel->getMediaURL() == sCurrentMedia.getMediaURL())
		{
			llinfos << "Media URL filter: no active alert, same URL as previous: " +parcel->getMediaURL() << llendl;
			sCurrentMedia = *parcel;
			if (parcel->getName() == currentparcel->getName())
			{
				// Only play if we're still there.
				LLViewerParcelMedia::play(parcel);
			}
			sMediaQueueEmpty = true;
			return;
		}
		llinfos << "Media URL filter: no active alert, filtering new URL: "+parcel->getMediaURL() << llendl;
		sMediaQueueEmpty = true;
	}
	// If an alert is active, place the media in the media queue if not the same as previous request
	else
	{
		if (sMediaQueueEmpty == false)
		{
			if (parcel->getMediaURL() != sQueuedMedia.getMediaURL())
			{	
				llinfos << "Media URL filter: active alert, replacing current queued media URL with: "+sQueuedMedia.getMediaURL() << llendl;
				sQueuedMedia = *parcel;
				sMediaQueueEmpty = false;
			}
			return;
		}
		else
		{
			if (parcel->getMediaURL() != sCurrentMedia.getMediaURL())
			{
				llinfos << "Media URL filter: active alert, nothing queued, adding new queued media URL: "+sQueuedMedia.getMediaURL() << llendl;
				sQueuedMedia = *parcel;
				sMediaQueueEmpty = false;
			}
			return;
		}
	}

	std::string media_url = parcel->getMediaURL();
	if (media_url.empty())
	{
		// Treat it as allowed; it'll get stopped elsewhere
		sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			// We haven't moved, so let it run.
			LLViewerParcelMedia::play(parcel);
		}
		return;
	}

	if (media_url == sMediaLastURL)
	{
		// Don't bother the user if all we're doing is repeating
		//  ourselves.
		if (sMediaLastActionPlay)
		{
			// We played it last time...so if we're still there...
			sCurrentMedia = *parcel;
			if (parcel->getName() == currentparcel->getName())
			{
				// The parcel hasn't changed (we didn't
				//  teleport, or move), so play it again, Sam.
				LLViewerParcelMedia::play(parcel);
			}
		}
		return;
	}

	sMediaLastURL = media_url;

	std::string media_action;
	std::string domain = extractDomain(media_url);
    
	for(S32 i = 0;i<(S32)sMediaFilterList.size();i++)
	{
		if (sMediaFilterList[i]["domain"].asString() == domain)
		{
			media_action = sMediaFilterList[i]["action"].asString();
			break;
		}
	}
	if (media_action=="allow")
	{
		llinfos << "Media filter: URL allowed by whitelist: "+parcel->getMediaURL() << llendl;
		sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			LLViewerParcelMedia::play(parcel);
		}
		sMediaLastActionPlay = true;
	}
	else if (media_action=="deny")
	{
		LLChat chat;
		chat.mText = "Media blocked - Blacklisted domain: "+domain;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLSD args;
		args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		sMediaLastActionPlay = false;
	}
	else
	{
		// We haven't been told what to do, and no alert is already
		//  active, so put up the alert and note the fact.
		LLSD args;
		args["MEDIAURL"] = media_url;
		LLViewerParcelMedia::sMediaFilterAlertActive = true;
		LLViewerParcelMedia::sCurrentAlertMedia = *parcel;
		LLParcel* pParcel = &LLViewerParcelMedia::sCurrentAlertMedia;
		LLNotifications::instance().add("MediaAlert", args,LLSD(),boost::bind(callback_media_alert, _1, _2, pParcel));
	}
}

void callback_media_alert(const LLSD &notification, const LLSD &response, LLParcel* parcel)
{
	LLParcel *currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	std::string media_url = parcel->getMediaURL();
	std::string domain = LLViewerParcelMedia::extractDomain(media_url);

	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLSD args;
	args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
	LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);

	LLViewerParcelMedia::sMediaLastActionPlay = false;
	if (option == 0) //allow
	{
		LLViewerParcelMedia::sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			LLViewerParcelMedia::play(parcel);
		}
		LLViewerParcelMedia::sMediaLastActionPlay = true;	
	}
	else if (option == 2) //Blacklist
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now blacklisted";
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
	}
	else if (option == 3) // Whitelist
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now whitelisted";
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		LLViewerParcelMedia::sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			LLViewerParcelMedia::play(parcel);
		}
		LLViewerParcelMedia::sMediaLastActionPlay = true;
	}

	// We've dealt with the alert, so mark it as inactive.
	LLViewerParcelMedia::sMediaFilterAlertActive = false;

	// Check for any queued alerts.
	if (LLViewerParcelMedia::sMusicQueueEmpty == false)
	{
		// There's a queued audio stream. Ask about it.
		LLViewerParcelMedia::filterAudioUrl(LLViewerParcelMedia::sQueuedMusic);
	}
	else if (LLViewerParcelMedia::sMediaQueueEmpty == false)
	{
		// There's a queued media stream. Ask about it.
		LLParcel* pParcel = &LLViewerParcelMedia::sQueuedMedia;
		LLViewerParcelMedia::filterMediaUrl(pParcel);
	}

	
}

void LLViewerParcelMedia::filterAudioUrl(std::string media_url)
{
	// If there is no alert active, filter the media and flag the music
	//  queue empty.
	if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
	{
		if (media_url == sCurrentMusic)
		{
			llinfos << "Audio URL filter: no active alert, same URL as previous: " + media_url << llendl;
			// The music hasn't changed, so keep playing.
			if (gAudiop != NULL)
			{
				gAudiop->startInternetStream(media_url);
				//LLOverlayBar::audioFilterPlay();
			}
			sMusicQueueEmpty = true;
			return;
		}
		// New music, so flag the queue empty and filter it.
		llinfos << "Audio URL filter: no active alert, filtering new URL: " + media_url << llendl;
		sMusicQueueEmpty = true;
	}
	// If an alert is active, place the media url in the music queue
	//  if not the same as previous request.
	else
	{
		if (sMusicQueueEmpty == false)
		{
			if (media_url != sQueuedMusic)
			{
				llinfos << "Audio URL filter: active alert, replacing existing queue with: " + media_url << llendl;
				sQueuedMusic = media_url;
				sMusicQueueEmpty = false;
			}
			
			return;
		}
		else
		{
			if (media_url != sCurrentMusic)
			{
				llinfos << "Audio URL filter: active alert, nothing queued, adding queue with: " + media_url << llendl;
				sQueuedMusic = media_url;
				sMusicQueueEmpty = false;
			}

			return;
		}
	}	

	sCurrentMusic = media_url;

	// If the new URL is empty, just play it.
	if (media_url.empty())
	{
		// Treat it as allowed; it'll get stopped elsewhere
		if (gAudiop != NULL)
		{
			gAudiop->startInternetStream(media_url);
			//LLOverlayBar::audioFilterPlay();
		}
		return;
	}

	// If this is the same as the last one we asked about, don't bug the
	//  user with it again.
	if (media_url == sAudioLastURL)
	{
		if (sAudioLastActionPlay)
		{
			if (gAudiop != NULL)
			{
				gAudiop->startInternetStream(media_url);
				//LLOverlayBar::audioFilterPlay();
			}
		}
		return;
	}

	sAudioLastURL = media_url;

	std::string media_action;
	std::string domain = extractDomain(media_url);
    
	for(S32 i = 0;i<(S32)sMediaFilterList.size();i++)
	{
		if (sMediaFilterList[i]["domain"].asString() == domain)
		{
			media_action = sMediaFilterList[i]["action"].asString();
			break;
		}
	}
	if (media_action=="allow")
	{
		if (gAudiop != NULL)
		{
			llinfos << "Audio filter: URL allowed by whitelist" << llendl;
			gAudiop->startInternetStream(media_url);
			//LLOverlayBar::audioFilterPlay();
		}
		sAudioLastActionPlay = true;
	}
	else if (media_action=="deny")
	{
		LLChat chat;
		chat.mText = "Audio blocked - Blacklisted domain: "+domain;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLSD args;
		args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		//LLOverlayBar::audioFilterStop();
		sAudioLastActionPlay = false;
	}
	else
	{
		LLSD args;
		args["AUDIOURL"] = media_url;
		LLViewerParcelMedia::sMediaFilterAlertActive = true;
		LLNotifications::instance().add("AudioAlert", args,LLSD(),boost::bind(callback_audio_alert, _1, _2, media_url));
	}
}

void callback_audio_alert(const LLSD &notification, const LLSD &response, std::string media_url)
{
	LLViewerParcelMedia::sMediaFilterAlertActive = true;
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	std::string domain = LLViewerParcelMedia::extractDomain(media_url);

	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLSD args;
	args["type"] = LLNotificationsUI::NT_NEARBYCHAT;

	if (option== 0) //allow
	{
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = media_url;
			gAudiop->startInternetStream(media_url);
			//LLOverlayBar::audioFilterPlay();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = true;
	}
	else if (option== 1) //deny
	{
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = "";
			gAudiop->stopInternetStream();
			//LLOverlayBar::audioFilterStop();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = false;
	}
	else if (option== 2) //Blacklist
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now blacklisted";
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = "";
			gAudiop->stopInternetStream();
			//LLOverlayBar::audioFilterStop();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = false;
	}
	else if (option== 3) // Whitelist
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now whitelisted";
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = media_url;
			gAudiop->startInternetStream(media_url);
			//LLOverlayBar::audioFilterPlay();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = true;
	}
	LLViewerParcelMedia::sMediaFilterAlertActive = false;
	
	// Check for queues 
	if (LLViewerParcelMedia::sMusicQueueEmpty == false)
	{
		LLViewerParcelMedia::filterAudioUrl(LLViewerParcelMedia::sQueuedMusic);
	}
	else
	{
		LLParcel* pParcel = &LLViewerParcelMedia::sQueuedMedia;
		LLViewerParcelMedia::filterMediaUrl(pParcel);
	}
}

bool LLViewerParcelMedia::saveDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	llofstream medialistFile(medialist_filename);
	LLSDSerialize::toPrettyXML(sMediaFilterList, medialistFile);
	medialistFile.close();
	return true;
}

bool LLViewerParcelMedia::loadDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	if(!LLFile::isfile(medialist_filename))
	{
		LLSD emptyllsd;
		llofstream medialistFile(medialist_filename);
		LLSDSerialize::toPrettyXML(emptyllsd, medialistFile);
		medialistFile.close();
	}

	if(LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename);
		LLSDSerialize::fromXML(sMediaFilterList, medialistFile);
		medialistFile.close();
		return true;
	}
	else
	{
		return false;
	}
}

std::string LLViewerParcelMedia::extractDomain(std::string url)
{
	// First, find and strip any protocol prefix.
	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		S32 count = url.size()-pos+2;
		url = url.substr(pos+2, count);
	}

	// Now, look for a / marking a local part; if there is one,
	//  strip it and anything after.
	pos = url.find("/");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}

	// If there's a user{,:password}@ part, remove it,
	pos = url.find("@");

	if (pos != std::string::npos)
	{
		S32 count = url.size()-pos+1;
		url = url.substr(pos+1, count);
	}

	// Finally, find and strip away any port number. This has to be done
	//  after the previous step, or else the extra : for the password,
	//  if supplied, will confuse things.
	pos = url.find(":");  

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}
	
	// Now map the whole thing to lowercase, since domain names aren't
	//  case sensitive.
	std::transform(url.begin(), url.end(),url.begin(), ::tolower);

	return url;
}

// TODO: observer
/*
void LLViewerParcelMediaNavigationObserver::onNavigateComplete( const EventType& event_in )
{
	std::string url = event_in.getStringValue();

	if (mCurrentURL != url && ! mFromMessage)
	{
		LLViewerParcelMedia::sendMediaNavigateMessage(url);
	}

	mCurrentURL = url;
	mFromMessage = false;

}
*/
