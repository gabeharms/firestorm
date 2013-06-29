/** 
 * @file llnotificationtiphandler.cpp
 * @brief Notification Handler Class for Notification Tips
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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


#include "llviewerprecompiledheaders.h" // must be first include

#include "llfloaterreg.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaterimnearbychat.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
#include "llnotificationhandler.h"
#include "llnotifications.h"
#include "lltoastnotifypanel.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llnotificationmanager.h"
#include "llpaneltiptoast.h"
#include "lggcontactsets.h" // [FIRE-3522 : SJ]

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLTipHandler::LLTipHandler()
:	LLSystemNotificationHandler("NotificationTips", "notifytip")
{
	// Getting a Channel for our notifications
	LLScreenChannel* channel = LLChannelManager::getInstance()->createNotificationChannel();
	if(channel)
	{
		mChannel = channel->getHandle();
	}
}

//--------------------------------------------------------------------------
LLTipHandler::~LLTipHandler()
{
}

//--------------------------------------------------------------------------
void LLTipHandler::initChannel()
{
	S32 channel_right_bound = gViewerWindow->getWorldViewRectScaled().mRight - gSavedSettings.getS32("NotificationChannelRightMargin"); 
	S32 channel_width = gSavedSettings.getS32("NotifyBoxWidth");
	mChannel.get()->init(channel_right_bound - channel_width, channel_right_bound);
}

//--------------------------------------------------------------------------
bool LLTipHandler::processNotification(const LLNotificationPtr& notification)
{
	if(mChannel.isDead())
	{
		return false;
	}

	// arrange a channel on a screen
	if(!mChannel.get()->getVisible())
	{
		initChannel();
	}

		// archive message in nearby chat
	if (notification->canLogToChat())
	{
		LLHandlerUtil::logToNearbyChat(notification, CHAT_SOURCE_SYSTEM);

		// don't show toast if Nearby Chat is opened
		// <FS:Ansariel> [FS communication UI]
		//LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
		//if (nearby_chat->isChatVisible())
		FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
		// </FS:Ansariel> [FS communication UI]
		if (nearby_chat->getVisible())
		{
			return false;
		}
	}

	std::string session_name = notification->getPayload()["SESSION_NAME"];
	const std::string name = notification->getSubstitutions()["NAME"];
		const LLUUID agent_id = notification->getSubstitutions()["AGENT-ID"]; // [FIRE-3522 : SJ]
		
	if (session_name.empty())
	{
		session_name = name;
	}
	LLUUID from_id = notification->getPayload()["from_id"];
	if (notification->canLogToIM())
	{
//	LLHandlerUtil::logToIM(IM_NOTHING_SPECIAL, session_name, name,
//			notification->getMessage(), from_id, from_id);
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
			LLHandlerUtil::logToIMP2P(notification, false);
// [/SL:KB]
	}

	if (notification->canLogToIM() && notification->hasFormElements())
	{
		LLHandlerUtil::spawnIMSession(name, from_id);
	}

	if (notification->canLogToIM() && LLHandlerUtil::isIMFloaterOpened(notification))
	{
		return false;
		}

		// [FIRE-3522 : SJ] Only show Online/Offline toast when ChatOnlineNotification is enabled or the Friend is one you want to have on/offline notices from
		if (!gSavedSettings.getBOOL("ChatOnlineNotification") && ("FriendOffline" == notification->getName() || "FriendOnline" == notification->getName()))
		{
			// [FIRE-3522 : SJ] Only show Online/Offline toast for groups which have enabled "Show notice for this set" and in the settingpage of CS is checked that the messages need to be in Toasts
			if (!(gSavedSettings.getBOOL("FSContactSetsNotificationToast") && LGGContactSets::getInstance()->notifyForFriend(agent_id)))
			{
				return false;
			}
	}

	LLToastPanel* notify_box = LLToastPanel::buidPanelFromNotification(notification);

	LLToast::Params p;
	p.notif_id = notification->getID();
	p.notification = notification;
	p.lifetime_secs = gSavedSettings.getS32("NotificationTipToastLifeTime");
	p.panel = notify_box;
	p.is_tip = true;
	p.can_be_stored = false;
		
	LLScreenChannel* channel = dynamic_cast<LLScreenChannel*>(mChannel.get());
	if(channel)
		channel->addToast(p);
	return false;
}
