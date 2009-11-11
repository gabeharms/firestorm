/** 
 * @file llavataractions.cpp
 * @brief Friend-related actions (add, remove, offer teleport, etc)
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


#include "llviewerprecompiledheaders.h"

#include "llavataractions.h"

#include "llsd.h"
#include "lldarray.h"
#include "llnotifications.h"

#include "roles_constants.h"    // for GP_MEMBER_INVITE

#include "llagent.h"
#include "llappviewer.h"		// for gLastVersionChannel
#include "llcachename.h"
#include "llcallingcard.h"		// for LLAvatarTracker
#include "llfloatergroupinvite.h"
#include "llfloatergroups.h"
#include "llfloaterreg.h"
#include "llfloaterpay.h"
#include "llinventorymodel.h"	// for gInventory.findCategoryUUIDForType
#include "llimview.h"			// for gIMMgr
#include "llmutelist.h"
#include "llrecentpeople.h"
#include "llsidetray.h"
#include "llviewerobjectlist.h"
#include "llviewermessage.h"	// for handle_lure
#include "llviewerregion.h"
#include "llimfloater.h"
#include "lltrans.h"

// callback connection to auto-call when the IM floater initializes
boost::signals2::connection gAdhocAutoCall;

// static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id, const std::string& name)
{
	if(id == gAgentID)
	{
		LLNotifications::instance().add("AddSelfFriend");
		return;
	}

	LLSD args;
	args["NAME"] = name;
	LLSD payload;
	payload["id"] = id;
	payload["name"] = name;
    // Look for server versions like: Second Life Server 1.24.4.95600
	if (gLastVersionChannel.find(" 1.24.") != std::string::npos)
	{
		// Old and busted server version, doesn't support friend
		// requests with messages.
    	LLNotifications::instance().add("AddFriend", args, payload, &callbackAddFriend);
	}
	else
	{
    	LLNotifications::instance().add("AddFriendWithMessage", args, payload, &callbackAddFriendWithMessage);
	}

	// add friend to recent people list
	LLRecentPeople::instance().add(id);
}

// static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id)
{
	if(id.isNull())
	{
		return;
	}

	std::string full_name;
	gCacheName->getFullName(id, full_name);
	requestFriendshipDialog(id, full_name);
}

// static
void LLAvatarActions::removeFriendDialog(const LLUUID& id)
{
	if (id.isNull())
		return;

	std::vector<LLUUID> ids;
	ids.push_back(id);
	removeFriendsDialog(ids);
}

// static
void LLAvatarActions::removeFriendsDialog(const std::vector<LLUUID>& ids)
{
	if(ids.size() == 0)
		return;

	LLSD args;
	std::string msgType;
	if(ids.size() == 1)
	{
		LLUUID agent_id = ids[0];
		std::string first, last;
		if(gCacheName->getName(agent_id, first, last))
		{
			args["FIRST_NAME"] = first;
			args["LAST_NAME"] = last;	
		}

		msgType = "RemoveFromFriends";
	}
	else
	{
		msgType = "RemoveMultipleFromFriends";
	}

	LLSD payload;
	for (std::vector<LLUUID>::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		payload["ids"].append(*it);
	}

	LLNotifications::instance().add(msgType,
		args,
		payload,
		&handleRemove);
}

// static
void LLAvatarActions::offerTeleport(const LLUUID& invitee)
{
	if (invitee.isNull())
		return;

	LLDynamicArray<LLUUID> ids;
	ids.push_back(invitee);
	offerTeleport(ids);
}

// static
void LLAvatarActions::offerTeleport(const std::vector<LLUUID>& ids) 
{
	if (ids.size() > 0)
		handle_lure(ids);
}

// static
void LLAvatarActions::startIM(const LLUUID& id)
{
	if (id.isNull())
		return;

	std::string name;
	gCacheName->getFullName(id, name);
	LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, id);
	if (session_id != LLUUID::null)
	{
		LLIMFloater::show(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::startCall(const LLUUID& id)
{
	if (id.isNull() || isCalling(id))
	{
		return;
	}

	std::string name;
	gCacheName->getFullName(id, name);
	LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, id);
	if (session_id != LLUUID::null)
	{
		// always open IM window when connecting to voice
		LLIMFloater::show(session_id);
		gIMMgr->startCall(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::startAdhocCall(const std::vector<LLUUID>& ids)
{
	if (ids.size() == 0)
	{
		return;
	}

	// convert vector into LLDynamicArray for addSession
	LLDynamicArray<LLUUID> id_array;
	for (std::vector<LLUUID>::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		id_array.push_back(*it);
	}

	// create the new ad hoc voice session
	const std::string title = LLTrans::getString("conference-title");
	LLUUID session_id = gIMMgr->addSession(title, IM_SESSION_CONFERENCE_START,
										   ids[0], id_array);
	if (session_id == LLUUID::null)
	{
		return;
	}

	// always open IM window when connecting to voice
	LLIMFloater::show(session_id);

	// start the call once the floater has fully initialized
	gAdhocAutoCall = LLIMModel::getInstance()->addSessionInitializedCallback(callbackAutoStartCall);

	make_ui_sound("UISndStartIM");
}

// static
bool LLAvatarActions::isCalling(const LLUUID &id)
{
	if (id.isNull())
	{
		return false;
	}

	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
	return (LLIMModel::getInstance()->findIMSession(session_id) != NULL);
}

// static
void LLAvatarActions::startConference(const std::vector<LLUUID>& ids)
{
	// *HACK: Copy into dynamic array
	LLDynamicArray<LLUUID> id_array;
	for (std::vector<LLUUID>::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		id_array.push_back(*it);
	}
	const std::string title = LLTrans::getString("conference-title");
	LLUUID session_id = gIMMgr->addSession(title, IM_SESSION_CONFERENCE_START, ids[0], id_array);
	if (session_id != LLUUID::null)
	{
		LLIMFloater::show(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::showProfile(const LLUUID& id)
{
	if (id.notNull())
	{
		LLSD params;
		params["id"] = id;
		params["open_tab_name"] = "panel_profile";

		//Show own profile
		if(gAgent.getID() == id)
		{
			LLSideTray::getInstance()->showPanel("panel_me_profile", params);
		}
		//Show other user profile
		else
		{
			LLSideTray::getInstance()->showPanel("panel_profile_view", params);
		}
	}
}

// static
void LLAvatarActions::pay(const LLUUID& id)
{
	LLNotification::Params params("BusyModePay");
	params.functor.function(boost::bind(&LLAvatarActions::handlePay, _1, _2, id));

	if (gAgent.getBusy())
	{
		// warn users of being in busy mode during a transaction
		LLNotifications::instance().add(params);
	}
	else
	{
		LLNotifications::instance().forceResponse(params, 1);
	}
}

// static
void LLAvatarActions::toggleBlock(const LLUUID& id)
{
	std::string name;

	gCacheName->getFullName(id, name);
	LLMute mute(id, name, LLMute::AGENT);

	if (LLMuteList::getInstance()->isMuted(mute.mID, mute.mName))
	{
		LLMuteList::getInstance()->remove(mute);
	}
	else
	{
		LLMuteList::getInstance()->add(mute);
	}
}

void LLAvatarActions::inviteToGroup(const LLUUID& id)
{
	LLFloaterGroupPicker* widget = LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(id));
	if (widget)
	{
		widget->removeNoneOption();
		widget->center();
		widget->setPowersMask(GP_MEMBER_INVITE);
		widget->setSelectGroupCallback(boost::bind(callback_invite_to_group, _1, id));
	}
}

//== private methods ========================================================================================

// static
bool LLAvatarActions::handleRemove(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	const LLSD& ids = notification["payload"]["ids"];
	for (LLSD::array_const_iterator itr = ids.beginArray(); itr != ids.endArray(); ++itr)
	{
		LLUUID id = itr->asUUID();
		const LLRelationship* ip = LLAvatarTracker::instance().getBuddyInfo(id);
		if (ip)
		{
			switch (option)
			{
			case 0: // YES
				if( ip->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
				{
					LLAvatarTracker::instance().empower(id, FALSE);
					LLAvatarTracker::instance().notifyObservers();
				}
				LLAvatarTracker::instance().terminateBuddy(id);
				LLAvatarTracker::instance().notifyObservers();
				break;

			case 1: // NO
			default:
				llinfos << "No removal performed." << llendl;
				break;
			}
		}
	}
	return false;
}

// static
bool LLAvatarActions::handlePay(const LLSD& notification, const LLSD& response, LLUUID avatar_id)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		gAgent.clearBusy();
	}

	LLFloaterPayUtil::payDirectly(&give_money, avatar_id, /*is_group=*/false);
	return false;
}

// static
void LLAvatarActions::callback_invite_to_group(LLUUID group_id, LLUUID id)
{
	std::vector<LLUUID> agent_ids;
	agent_ids.push_back(id);
	
	LLFloaterGroupInvite::showForGroup(group_id, &agent_ids);
}


// static
bool LLAvatarActions::callbackAddFriendWithMessage(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		requestFriendship(notification["payload"]["id"].asUUID(), 
		    notification["payload"]["name"].asString(),
		    response["message"].asString());
	}
	return false;
}

// static
bool LLAvatarActions::callbackAddFriend(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		// Servers older than 1.25 require the text of the message to be the
		// calling card folder ID for the offering user. JC
		LLUUID calling_card_folder_id = 
			gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		std::string message = calling_card_folder_id.asString();
		requestFriendship(notification["payload"]["id"].asUUID(), 
		    notification["payload"]["name"].asString(),
		    message);
	}
    return false;
}

// static
void LLAvatarActions::callbackAutoStartCall(const LLSD& data)
{
	// start the adhoc voice call now the IM panel has initialized
	LLUUID session_id = data["session_id"].asUUID();
	gIMMgr->startCall(session_id);

	// and deschedule this callback as its work is done now
	gAdhocAutoCall.disconnect();
}

// static
void LLAvatarActions::requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message)
{
	const LLUUID calling_card_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	send_improved_im(target_id,
					 target_name,
					 message,
					 IM_ONLINE,
					 IM_FRIENDSHIP_OFFERED,
					 calling_card_folder_id);
}

//static
bool LLAvatarActions::isFriend(const LLUUID& id)
{
	return ( NULL != LLAvatarTracker::instance().getBuddyInfo(id) );
}

// static
bool LLAvatarActions::isBlocked(const LLUUID& id)
{
	std::string name;
	gCacheName->getFullName(id, name);
	return LLMuteList::getInstance()->isMuted(id, name);
}
