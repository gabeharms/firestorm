/** 
 * @file llpanelavatar.cpp
 * @brief LLPanelAvatar and related class implementations
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llpanelimcontrolpanel.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llavatariconctrl.h"
#include "llbutton.h"
#include "llgroupactions.h"
#include "llavatarlist.h"
#include "llparticipantlist.h"
#include "llimview.h"
#include "llvoicechannel.h"

void LLPanelChatControlPanel::onCallButtonClicked()
{
	gIMMgr->startCall(mSessionId);
}

void LLPanelChatControlPanel::onEndCallButtonClicked()
{
	gIMMgr->endCall(mSessionId);
}

void LLPanelChatControlPanel::onOpenVoiceControlsClicked()
{
	// TODO: implement Voice Control Panel opening
}

void LLPanelChatControlPanel::onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state)
{
	bool is_call_started = ( new_state >= LLVoiceChannel::STATE_CALL_STARTED );
	childSetVisible("end_call_btn", is_call_started);
	childSetVisible("voice_ctrls_btn", is_call_started);
	childSetVisible("call_btn", ! is_call_started);
}

BOOL LLPanelChatControlPanel::postBuild()
{
	childSetAction("call_btn", boost::bind(&LLPanelChatControlPanel::onCallButtonClicked, this));
	childSetAction("end_call_btn", boost::bind(&LLPanelChatControlPanel::onEndCallButtonClicked, this));
	childSetAction("voice_ctrls_btn", boost::bind(&LLPanelChatControlPanel::onOpenVoiceControlsClicked, this));

	return TRUE;
}

void LLPanelChatControlPanel::draw()
{
	// hide/show start call and end call buttons
	bool voice_enabled = LLVoiceClient::voiceEnabled();

	LLIMModel::LLIMSession* session = LLIMModel::getInstance()->findIMSession(mSessionId);
	if (!session) return;

	bool session_initialized = session->mSessionInitialized;
	bool callback_enabled = session->mCallBackEnabled;
	LLViewerRegion* region = gAgent.getRegion();

	BOOL enable_connect = (region && region->getCapability("ChatSessionRequest") != "")
		&& session_initialized
		&& voice_enabled
		&& callback_enabled;
	childSetEnabled("call_btn", enable_connect);

	LLPanel::draw();
}

void LLPanelChatControlPanel::setSessionId(const LLUUID& session_id)
{
	//Method is called twice for AdHoc and Group chat. Second time when server init reply received
	mSessionId = session_id;
	LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionId);
	if(voice_channel)
		voice_channel->setStateChangedCallback(boost::bind(&LLPanelChatControlPanel::onVoiceChannelStateChanged, this, _1, _2));
}

LLPanelIMControlPanel::LLPanelIMControlPanel()
{
}

LLPanelIMControlPanel::~LLPanelIMControlPanel()
{
}

BOOL LLPanelIMControlPanel::postBuild()
{
	childSetAction("view_profile_btn", boost::bind(&LLPanelIMControlPanel::onViewProfileButtonClicked, this));
	childSetAction("add_friend_btn", boost::bind(&LLPanelIMControlPanel::onAddFriendButtonClicked, this));

	childSetAction("share_btn", boost::bind(&LLPanelIMControlPanel::onShareButtonClicked, this));
	childSetAction("teleport_btn", boost::bind(&LLPanelIMControlPanel::onTeleportButtonClicked, this));
	childSetAction("pay_btn", boost::bind(&LLPanelIMControlPanel::onPayButtonClicked, this));
	childSetEnabled("add_friend_btn", !LLAvatarActions::isFriend(getChild<LLAvatarIconCtrl>("avatar_icon")->getAvatarId()));

	
	
	return LLPanelChatControlPanel::postBuild();
}

void LLPanelIMControlPanel::onTeleportButtonClicked()
{
	LLAvatarActions::offerTeleport(mAvatarID);
}
void LLPanelIMControlPanel::onPayButtonClicked()
{
	LLAvatarActions::pay(mAvatarID);
}

void LLPanelIMControlPanel::onViewProfileButtonClicked()
{
	LLAvatarActions::showProfile(mAvatarID);
}

void LLPanelIMControlPanel::onAddFriendButtonClicked()
{
	LLAvatarIconCtrl* avatar_icon = getChild<LLAvatarIconCtrl>("avatar_icon");
	std::string full_name = avatar_icon->getFirstName() + " " + avatar_icon->getLastName();
	LLAvatarActions::requestFriendshipDialog(mAvatarID, full_name);
}

void LLPanelIMControlPanel::onShareButtonClicked()
{
	// *TODO: Implement
}

void LLPanelIMControlPanel::setSessionId(const LLUUID& session_id)
{
	LLPanelChatControlPanel::setSessionId(session_id);

	LLIMModel& im_model = LLIMModel::instance();

	mAvatarID = im_model.getOtherParticipantID(session_id);

	// Disable "Add friend" button for friends.
	childSetEnabled("add_friend_btn", !LLAvatarActions::isFriend(mAvatarID));

	getChild<LLAvatarIconCtrl>("avatar_icon")->setValue(mAvatarID);

	// Fetch the currect name
	gCacheName->get(mAvatarID, FALSE, boost::bind(&LLPanelIMControlPanel::nameUpdatedCallback, this, _1, _2, _3, _4));

	// Disable profile button if participant is not realy SL avatar
	LLIMModel::LLIMSession* im_session =
		im_model.findIMSession(session_id);
	if( im_session && !im_session->mOtherParticipantIsAvatar )
		childSetEnabled("view_profile_btn", FALSE);
}

void LLPanelIMControlPanel::nameUpdatedCallback(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group)
{
	if ( id == mAvatarID )
	{
		std::string avatar_name;
		avatar_name.assign(first);
		avatar_name.append(" ");
		avatar_name.append(last);
		getChild<LLTextBox>("avatar_name")->setValue(avatar_name);
	}
}

LLPanelGroupControlPanel::LLPanelGroupControlPanel(const LLUUID& session_id)
{
	mSpeakerManager = LLIMModel::getInstance()->getSpeakerManager(session_id);
}

BOOL LLPanelGroupControlPanel::postBuild()
{
	childSetAction("group_info_btn", boost::bind(&LLPanelGroupControlPanel::onGroupInfoButtonClicked, this));

	mAvatarList = getChild<LLAvatarList>("speakers_list");
	mParticipantList = new LLParticipantList(mSpeakerManager, mAvatarList);

	return LLPanelChatControlPanel::postBuild();
}

LLPanelGroupControlPanel::~LLPanelGroupControlPanel()
{
	delete mParticipantList;
	mParticipantList = NULL;
}

// virtual
void LLPanelGroupControlPanel::draw()
{
	mSpeakerManager->update(true);
	LLPanelChatControlPanel::draw();
}

void LLPanelGroupControlPanel::onGroupInfoButtonClicked()
{
	LLGroupActions::show(mGroupID);
}

void LLPanelGroupControlPanel::onSortMenuItemClicked(const LLSD& userdata)
{
	// TODO: Check this code when when sort order menu will be added. (EM)
	if (false && !mParticipantList)
		return;

	std::string chosen_item = userdata.asString();

	if (chosen_item == "sort_name")
	{
		mParticipantList->setSortOrder(LLParticipantList::E_SORT_BY_NAME);
	}

}

void LLPanelGroupControlPanel::onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state)
{
	LLPanelChatControlPanel::onVoiceChannelStateChanged(old_state, new_state);
	mAvatarList->setSpeakingIndicatorsVisible(new_state >= LLVoiceChannel::STATE_CALL_STARTED);
}

void LLPanelGroupControlPanel::setSessionId(const LLUUID& session_id)
{
	LLPanelChatControlPanel::setSessionId(session_id);

	mGroupID = LLIMModel::getInstance()->getOtherParticipantID(session_id);
}


LLPanelAdHocControlPanel::LLPanelAdHocControlPanel(const LLUUID& session_id):LLPanelGroupControlPanel(session_id)
{
}

BOOL LLPanelAdHocControlPanel::postBuild()
{
	mAvatarList = getChild<LLAvatarList>("speakers_list");
	mParticipantList = new LLParticipantList(mSpeakerManager, mAvatarList);

	return LLPanelChatControlPanel::postBuild();
}

