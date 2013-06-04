/** 
 * @file fsfloaterim.cpp
 * @brief FSFloaterIM class definition
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

// Original file: llimfloater.cpp

#include "llviewerprecompiledheaders.h"

#include "fsfloaterim.h"

#include "llnotificationsutil.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llchannelmanager.h"
#include "llchiclet.h"
#include "llchicletbar.h"
#include "llfloaterabout.h"		// for sysinfo button -Zi
#include "llfloaterreg.h"
#include "fsfloaterimcontainer.h" // to replace separate IM Floaters with multifloater container
#include "llinventoryfunctions.h"
#include "lllayoutstack.h"
#include "lllineeditor.h"
#include "lllogchat.h"
#include "fspanelimcontrolpanel.h"
#include "llscreenchannel.h"
#include "llsyswellwindow.h"
#include "lltrans.h"
#include "fschathistory.h"
#include "llnotifications.h"
#include "llviewerwindow.h"
#include "llvoicechannel.h"
#include "lltransientfloatermgr.h"
#include "llinventorymodel.h"
#include "llrootview.h"
#include "llspeakers.h"
#include "llviewerchat.h"
#include "llautoreplace.h"
// [RLVa:KB] - Checked: 2010-04-09 (RLVa-1.2.0e)
#include "rlvhandler.h"
// [/RLVa:KB]

//AO: For moving callbacks from control panel into this class
#include "llavataractions.h"
#include "llgroupactions.h"
#include "llvoicechannel.h"
//TL: for support group chat prefix
#include "fsdata.h"
#include "llversioninfo.h"
#include "llcheckboxctrl.h"

#include "llnotificationtemplate.h"		// <FS:Zi> Viewer version popup
#include "fscommon.h"

FSFloaterIM::FSFloaterIM(const LLUUID& session_id)
  : LLTransientDockableFloater(NULL, true, session_id),
	mControlPanel(NULL),
	mSessionID(session_id),
	mLastMessageIndex(-1),
	mDialog(IM_NOTHING_SPECIAL),
	mChatHistory(NULL),
	mInputEditor(NULL),
	mSavedTitle(),
	mTypingStart(),
	mShouldSendTypingState(false),
	mMeTyping(false),
	mOtherTyping(false),
	mTypingTimer(),
	mTypingTimeoutTimer(),
	mPositioned(false),
	mSessionInitialized(false)
{
	LLIMModel::LLIMSession* im_session = LLIMModel::getInstance()->findIMSession(mSessionID);
	if (im_session)
	{
		mSessionInitialized = im_session->mSessionInitialized;
		
		mDialog = im_session->mType;
		switch(mDialog){
		case IM_NOTHING_SPECIAL:
		case IM_SESSION_P2P_INVITE:
			mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelIMControl, this);
			break;
		case IM_SESSION_CONFERENCE_START:
			mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelAdHocControl, this);
			break;
		case IM_SESSION_GROUP_START:
			mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelGroupControl, this);
			break;
		case IM_SESSION_INVITE:		
			if (gAgent.isInGroup(mSessionID))
			{
				mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelGroupControl, this);
			}
			else
			{
				mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelAdHocControl, this);
			}
			break;
		default: break;
		}
	}
	setOverlapsScreenChannel(true);

	LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::IM, this);

	setDocked(true);
}

// virtual
BOOL FSFloaterIM::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash)
{
	mInputEditor->setFocus(TRUE);
	onTabInto();
	if(focus_flash)
	{
		gFocusMgr.triggerFocusFlash();
	}
	return TRUE;
}

void FSFloaterIM::onFocusLost()
{
	LLIMModel::getInstance()->resetActiveSessionID();
	
	LLChicletBar::getInstance()->getChicletPanel()->setChicletToggleState(mSessionID, false);
}

void FSFloaterIM::onFocusReceived()
{
	LLIMModel::getInstance()->setActiveSessionID(mSessionID);

	LLChicletBar::getInstance()->getChicletPanel()->setChicletToggleState(mSessionID, true);

	if (getVisible())
	{
		LLIMModel::instance().sendNoUnreadMessages(mSessionID);
	}

	// <FS:Ansariel> Give focus to input textbox
	mInputEditor->setFocus(TRUE);
}

// virtual
void FSFloaterIM::onClose(bool app_quitting)
{
	setTyping(false);

	// The source of much argument and design thrashing
	// Should the window hide or the session close when the X is clicked?
	//
	// Last change:
	// EXT-3516 X Button should end IM session, _ button should hide
	
	
	// AO: Make sure observers are removed on close
	mVoiceChannelStateChangeConnection.disconnect();
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
	}
	
	//<FS:ND> FIRE-6077 et al: Always clean up observers when the floater dies
	LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
	//</FS:ND> FIRE-6077 et al
	
	gIMMgr->leaveSession(mSessionID);
}

/* static */
void FSFloaterIM::newIMCallback(const LLSD& data){
	
	if (data["num_unread"].asInteger() > 0 || data["from_id"].asUUID().isNull())
	{
		LLUUID session_id = data["session_id"].asUUID();

		FSFloaterIM* floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
		if (floater == NULL) return;

        // update if visible, otherwise will be updated when opened
		if (floater->getVisible())
		{
			floater->updateMessages();
		}
	}
}

void FSFloaterIM::onVisibilityChange(const LLSD& new_visibility)
{
	bool visible = new_visibility.asBoolean();

	LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

	if (visible && voice_channel &&
		voice_channel->getState() == LLVoiceChannel::STATE_CONNECTED)
	{
		LLFloaterReg::showInstance("voice_call", mSessionID);
	}
	else
	{
		LLFloaterReg::hideInstance("voice_call", mSessionID);
	}
}

void FSFloaterIM::onSendMsg( LLUICtrl* ctrl, void* userdata )
{
	FSFloaterIM* self = (FSFloaterIM*) userdata;
	self->sendMsg();
	self->setTyping(false);
}

void FSFloaterIM::sendMsg()
{
	if (!gAgent.isGodlike() 
		&& (mDialog == IM_NOTHING_SPECIAL)
		&& mOtherParticipantUUID.isNull())
	{
		llinfos << "Cannot send IM to everyone unless you're a god." << llendl;
		return;
	}

	if (mInputEditor)
	{
		LLWString text = mInputEditor->getConvertedText();
		if(!text.empty())
		{
			// Convert to UTF8 for transport
			std::string utf8_text = wstring_to_utf8str(text);

			// Convert OOC and MU* style poses
			utf8_text = applyAutoCloseOoc(utf8_text);
			utf8_text = applyMuPose(utf8_text);

			// <FS:Techwolf Lupindo> Support group chat prefix
			static LLCachedControl<bool> chat_prefix(gSavedSettings, "FSSupportGroupChatPrefix2");
			if (chat_prefix && FSData::getInstance()->isSupportGroup(mSessionID))
			{

				// <FS:PP> FIRE-7075: Skin indicator
				static LLCachedControl<std::string> FSInternalSkinCurrent(gSavedSettings, "FSInternalSkinCurrent");
				std::string skinIndicator(FSInternalSkinCurrent);
				LLStringUtil::toLower(skinIndicator);
				if (skinIndicator == "starlight cui")
				{
					skinIndicator = "sc"; // Separate "s" (StarLight) from "sc" (StarLight CUI)
				}
				else
				{
					skinIndicator = skinIndicator.substr(0, 1); // "FS 4.4.1f os", "FS 4.4.1v", "FS 4.4.1a", "FS 4.4.1s os", "FS 4.4.1m os" etc.
				}
				// </FS:PP>

				if (utf8_text.find("/me ") == 0 || utf8_text.find("/me'") == 0)
				{
					utf8_text.insert(4,("(FS " + LLVersionInfo::getShortVersion() + skinIndicator +
#ifdef OPENSIM
					" os" +
#endif
					") "));
				}
				else
				{
					utf8_text.insert(0,("(FS " + LLVersionInfo::getShortVersion() + skinIndicator +
#ifdef OPENSIM
					" os" +
#endif
					") "));
				}
			}

			// <FS:Techwolf Lupindo> Allow user to send system info.
			if(mDialog == IM_NOTHING_SPECIAL && utf8_text.find("/sysinfo") == 0)
			{
				LLSD system_info = FSData::getSystemInfo();
				utf8_text = system_info["Part1"].asString() + system_info["Part2"].asString();
			}
			// </FS:Techwolf Lupindo> 

			// Truncate for transport
			//<FS:TS> FIRE-787: break up too long chat lines into multiple messages
			//utf8_text = utf8str_truncate(utf8_text, MAX_MSG_BUF_SIZE - 1);
			//</FS:TS> FIRE-787
			
// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0c) | Modified: RLVa-1.3.0c
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SENDIMTO)) )
			{
				LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
				RLV_ASSERT(pIMSession);

				bool fRlvFilter = !pIMSession;
				if (pIMSession)
				{
					switch (pIMSession->mSessionType)
					{
						case LLIMModel::LLIMSession::P2P_SESSION:	// One-on-one IM
							fRlvFilter = !gRlvHandler.canSendIM(mOtherParticipantUUID);
							break;
						case LLIMModel::LLIMSession::GROUP_SESSION:	// Group chat
							fRlvFilter = !gRlvHandler.canSendIM(mSessionID);
							break;
						case LLIMModel::LLIMSession::ADHOC_SESSION:	// Conference chat: allow if all participants can be sent an IM
							{
								if (!pIMSession->mSpeakers)
								{
									fRlvFilter = true;
									break;
								}

								LLSpeakerMgr::speaker_list_t speakers;
								pIMSession->mSpeakers->getSpeakerList(&speakers, TRUE);
								for (LLSpeakerMgr::speaker_list_t::const_iterator itSpeaker = speakers.begin(); 
										itSpeaker != speakers.end(); ++itSpeaker)
								{
									const LLSpeaker* pSpeaker = *itSpeaker;
									if ( (gAgent.getID() != pSpeaker->mID) && (!gRlvHandler.canSendIM(pSpeaker->mID)) )
									{
										fRlvFilter = true;
										break;
									}
								}
							}
							break;
						default:
							fRlvFilter = true;
							break;
					}
				}

				if (fRlvFilter)
					utf8_text = RlvStrings::getString(RLV_STRING_BLOCKED_SENDIM);
			}
// [/RLVa:KB]

			if (mSessionInitialized)
			{
				LLIMModel::sendMessage(utf8_text, mSessionID,
					mOtherParticipantUUID,mDialog);
			}
			else
			{
				//queue up the message to send once the session is initialized
				mQueuedMsgsForInit.append(utf8_text);
			}

			mInputEditor->setText(LLStringUtil::null);

			updateMessages();
		}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-02 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
		else if (gSavedSettings.getBOOL("CloseIMOnEmptyReturn"))
		{
			// Close if we're the child of a floater
			closeFloater();
		}
// [/SL:KB]
	}
}



FSFloaterIM::~FSFloaterIM()
{
	llinfos << "~FSFloaterIM, instance exists is: " << ((LLTransientFloaterMgr::getInstance()) == NULL) << llendl; 
	LLTransientFloaterMgr::getInstance()->removeControlView(LLTransientFloaterMgr::IM, (LLView*)this);
	mVoiceChannelStateChangeConnection.disconnect();
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
	}
	
	LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
	if ((pIMSession) && (pIMSession->mSessionType == LLIMModel::LLIMSession::P2P_SESSION))
	{
		llinfos << "AO: Cleaning up stray particularFriendObservers" << llendl;
		LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
	}
}

// <AO> Callbacks previously in llcontrol_panel, moved to this floater.

void FSFloaterIM::onViewProfileButtonClicked()
{
	llinfos << "FSFloaterIM::onViewProfileButtonClicked" << llendl;
	LLAvatarActions::showProfile(mOtherParticipantUUID);
}
void FSFloaterIM::onAddFriendButtonClicked()
{
	llinfos << "FSFloaterIM::onAddFriendButtonClicked" << llendl;
	//[FIX FIRE-2009: SJ] Offering friendship gives wrong status message. full_name was emtpy on call but was also obsolete
	//                    
	//LLAvatarIconCtrl* avatar_icon = getChild<LLAvatarIconCtrl>("avatar_icon");
	//std::string full_name = avatar_icon->getFullName();
	//LLAvatarActions::requestFriendshipDialog(mOtherParticipantUUID, full_name);
	LLAvatarActions::requestFriendshipDialog(mOtherParticipantUUID);
}
void FSFloaterIM::onShareButtonClicked()
{
	llinfos << "FSFloaterIM::onShareButtonClicked" << llendl;
	LLAvatarActions::share(mOtherParticipantUUID);
}
void FSFloaterIM::onTeleportButtonClicked()
{
	llinfos << "FSFloaterIM::onTeleportButtonClicked" << llendl;
	LLAvatarActions::offerTeleport(mOtherParticipantUUID);
}
void FSFloaterIM::onPayButtonClicked()
{
	llinfos << "FSFloaterIM::onPayButtonClicked" << llendl;
	LLAvatarActions::pay(mOtherParticipantUUID);
}
void FSFloaterIM::onGroupInfoButtonClicked()
{
	llinfos << "FSFloaterIM::onGroupInfoButtonClicked" << llendl;
	LLGroupActions::show(mSessionID);
}
void FSFloaterIM::onCallButtonClicked()
{
	llinfos << "FSFloaterIM::onCallButtonClicked" << llendl;
	gIMMgr->startCall(mSessionID);
}
void FSFloaterIM::onEndCallButtonClicked()
{
	llinfos << "FSFloaterIM::onEndCallButtonClicked" << llendl;
	gIMMgr->endCall(mSessionID);
}
void FSFloaterIM::onOpenVoiceControlsClicked()
{
	llinfos << "FSFloaterIM::onOpenVoiceControlsClicked" << llendl;
	LLFloaterReg::showInstance("fs_voice_controls");
}
void FSFloaterIM::onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state)
{
	llinfos << "FSFloaterIM::onVoiceChannelStateChanged" << llendl;
	updateButtons(new_state >= LLVoiceChannel::STATE_CALL_STARTED);
}
void FSFloaterIM::onHistoryButtonClicked()
{
	gViewerWindow->getWindow()->openFile(LLLogChat::makeLogFileName(LLIMModel::instance().getHistoryFileName(mSessionID)));
}

// support sysinfo button -Zi
void FSFloaterIM::onSysinfoButtonClicked()
{
	LLSD system_info = FSData::getSystemInfo();
	LLSD args;
	args["SYSINFO"] = system_info["Part1"].asString() + system_info["Part2"].asString();
	args["Part1"] = system_info["Part1"];
	args["Part2"] = system_info["Part2"];
	LLNotificationsUtil::add("SendSysinfoToIM",args,LLSD(),boost::bind(&FSFloaterIM::onSendSysinfo,this,_1,_2));
}

BOOL FSFloaterIM::onSendSysinfo(const LLSD& notification, const LLSD& response)
{
	S32 option=LLNotificationsUtil::getSelectedOption(notification,response);

	if(option==0)
	{
		std::string part1 = notification["substitutions"]["Part1"];
		std::string part2 = notification["substitutions"]["Part2"];
		if (mSessionInitialized)
		{
			LLIMModel::sendMessage(part1, mSessionID,mOtherParticipantUUID,mDialog);
			LLIMModel::sendMessage(part2, mSessionID,mOtherParticipantUUID,mDialog);
		}
		else
		{
			//queue up the message to send once the session is initialized
			mQueuedMsgsForInit.append(part1);
			mQueuedMsgsForInit.append(part2);
		}
		return TRUE;
	}
	return FALSE;
}

void FSFloaterIM::onSysinfoButtonVisibilityChanged(const LLSD& yes)
{
	mSysinfoButton->setVisible(yes.asBoolean() /* && mIsSupportIM */);
}
// support sysinfo button -Zi

void FSFloaterIM::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	// llinfos << "FSFloaterIM::onChange" << llendl;
	if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}
	
	updateCallButton();
}

void FSFloaterIM::updateCallButton()
{
	// llinfos << "FSFloaterIM::updateCallButton" << llendl;
	// hide/show call button
	bool voice_enabled = LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);
	
	if (!session) 
	{
		getChild<LLButton>("call_btn")->setEnabled(false);
		return;
	}
	
	//bool session_initialized = session->mSessionInitialized;
	bool callback_enabled = session->mCallBackEnabled;

	//[Possible FIX-FIRE-2012] GROUP and Ad-Hoc don't have session initialized --> removing that from the condition to enable_connect
	//BOOL enable_connect = session_initialized
	//&& voice_enabled
	//&& callback_enabled;
	BOOL enable_connect = voice_enabled
	&& callback_enabled;
	//if (voice_enabled) 
	//{
	//	llinfos << "FSFloaterIM::updateCallButton - voice enabled" << llendl;
	//}
	//if (session_initialized) 
	//{
	//	llinfos << "FSFloaterIM::updateCallButton - session_initialized" << llendl;
	//}
	//if (callback_enabled) 
	//{
	//	llinfos << "FSFloaterIM::updateCallButton - callback_enabled" << llendl;
	//}

	getChild<LLButton>("call_btn")->setEnabled(enable_connect);
}

void FSFloaterIM::updateButtons(bool is_call_started)
{
	llinfos << "FSFloaterIM::updateButtons" << llendl;
	getChild<LLLayoutStack>("ls_control_panel")->reshape(240,20,true);
	getChildView("end_call_btn_panel")->setVisible( is_call_started);
	getChildView("voice_ctrls_btn_panel")->setVisible( is_call_started);
	getChildView("call_btn_panel")->setVisible( ! is_call_started);
	updateCallButton();
	
	// AO: force resize the widget because llpanels don't resize properly on vis change.
	llinfos << "force resize the widget" << llendl;
	LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
	switch (pIMSession->mSessionType)
	{
		case LLIMModel::LLIMSession::P2P_SESSION:	// One-on-one IM
		{
			getChild<LLLayoutStack>("ls_control_panel")->reshape(230,20,true);
			break;
		}
		case LLIMModel::LLIMSession::GROUP_SESSION:	// Group chat
		{
			getChild<LLLayoutStack>("ls_control_panel")->reshape(170,20,true);
			break;
		}
		case LLIMModel::LLIMSession::ADHOC_SESSION:	// Conference chat
		{
			getChild<LLLayoutStack>("ls_control_panel")->reshape(150,20,true);
			break;
		}
		default:
			break;
	}
	
}

void FSFloaterIM::changed(U32 mask)
{
	llinfos << "FSFloaterIM::changed(U32 mask)" << llendl;
	getChild<LLButton>("call_btn")->setEnabled(!LLAvatarActions::isFriend(mOtherParticipantUUID));
	
	// Disable "Teleport" button if friend is offline
	if(LLAvatarActions::isFriend(mOtherParticipantUUID))
	{
		getChild<LLButton>("teleport_btn")->setEnabled(LLAvatarTracker::instance().isBuddyOnline(mOtherParticipantUUID));
	}
}

// </AO> Callbacks for llimcontrol panel, merged into this floater

//virtual
BOOL FSFloaterIM::postBuild()
{
	const LLUUID& other_party_id = LLIMModel::getInstance()->getOtherParticipantID(mSessionID);
	if (other_party_id.notNull())
	{
		mOtherParticipantUUID = other_party_id;
	}

	mControlPanel->setSessionId(mSessionID);
	
	// AO: always hide the control panel to start.
	llinfos << "mControlPanel->getParent()" << mControlPanel->getParent() << llendl;
	mControlPanel->getParent()->setVisible(false); 
	
	//mControlPanel->getParent()->setVisible(gSavedSettings.getBOOL("IMShowControlPanel"));

	llinfos << "buttons setup in IM start" << llendl;

	LLButton* slide_left = getChild<LLButton>("slide_left_btn");
	slide_left->setVisible(mControlPanel->getParent()->getVisible());
	slide_left->setClickedCallback(boost::bind(&FSFloaterIM::onSlide, this));

	LLButton* slide_right = getChild<LLButton>("slide_right_btn");
	slide_right->setVisible(!mControlPanel->getParent()->getVisible());
	slide_right->setClickedCallback(boost::bind(&FSFloaterIM::onSlide, this));
	
	LLButton* view_profile  = getChild<LLButton>("view_profile_btn");
	view_profile->setClickedCallback(boost::bind(&FSFloaterIM::onViewProfileButtonClicked, this));
	
	LLButton* group_profile = getChild<LLButton>("group_info_btn");
	group_profile->setClickedCallback(boost::bind(&FSFloaterIM::onGroupInfoButtonClicked, this));
	
	LLButton* call = getChild<LLButton>("call_btn");
	call->setClickedCallback(boost::bind(&FSFloaterIM::onCallButtonClicked, this));
	
	LLButton* endcall = getChild<LLButton>("end_call_btn");
	endcall->setClickedCallback(boost::bind(&FSFloaterIM::onEndCallButtonClicked, this));
	
	LLButton* voicectrl = getChild<LLButton>("voice_ctrls_btn");
	voicectrl->setClickedCallback(boost::bind(&FSFloaterIM::onOpenVoiceControlsClicked, this));
	
	LLButton* share = getChild<LLButton>("share_btn");
	share->setClickedCallback(boost::bind(&FSFloaterIM::onShareButtonClicked, this));
	
	LLButton* tp = getChild<LLButton>("teleport_btn");
	tp->setClickedCallback(boost::bind(&FSFloaterIM::onTeleportButtonClicked, this));
	
	LLButton* pay = getChild<LLButton>("pay_btn");
	pay->setClickedCallback(boost::bind(&FSFloaterIM::onPayButtonClicked, this));
	
	LLButton* add_friend = getChild<LLButton>("add_friend_btn");
	add_friend->setClickedCallback(boost::bind(&FSFloaterIM::onAddFriendButtonClicked, this));

	LLButton* im_history = getChild<LLButton>("im_history_btn");
	im_history->setClickedCallback(boost::bind(&FSFloaterIM::onHistoryButtonClicked, this));

	// support sysinfo button -Zi
	mSysinfoButton=getChild<LLButton>("send_sysinfo_btn");
	onSysinfoButtonVisibilityChanged(FALSE);

	// extra icon controls -AO
	LLButton* transl = getChild<LLButton>("translate_btn");
//TT
		llinfos << "transl" << (transl == NULL) << llendl;
	if (transl != NULL)
	transl->setVisible(true);
	
	// type-specfic controls
	LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
	if (pIMSession)
	{
		switch (pIMSession->mSessionType)
		{
			case LLIMModel::LLIMSession::P2P_SESSION:	// One-on-one IM
			{
				llinfos << "LLIMModel::LLIMSession::P2P_SESSION" << llendl;
				getChild<LLLayoutPanel>("slide_panel")->setVisible(false);
				getChild<LLLayoutPanel>("gprofile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
				getChild<LLLayoutStack>("ls_control_panel")->reshape(200,20,true);
				
				llinfos << "AO: adding FSFloaterIM removing/adding particularfriendobserver" << llendl;
				LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
				LLAvatarTracker::instance().addParticularFriendObserver(mOtherParticipantUUID, this);
				
				// Disable "Add friend" button for friends.
				llinfos << "add_friend_btn check start" << llendl;
				getChild<LLButton>("add_friend_btn")->setEnabled(!LLAvatarActions::isFriend(mOtherParticipantUUID));
				
				// Disable "Teleport" button if friend is offline
				if(LLAvatarActions::isFriend(mOtherParticipantUUID))
				{
					llinfos << "LLAvatarActions::isFriend - tp button" << llendl;
					getChild<LLButton>("teleport_btn")->setEnabled(LLAvatarTracker::instance().isBuddyOnline(mOtherParticipantUUID));
				}

				// support sysinfo button -Zi
				mSysinfoButton->setClickedCallback(boost::bind(&FSFloaterIM::onSysinfoButtonClicked, this));
				// this needs to be extended to fsdata awareness, once we have it. -Zi
				// mIsSupportIM=fsdata(partnerUUID).isSupport(); // pseudocode something like this
				onSysinfoButtonVisibilityChanged(gSavedSettings.getBOOL("SysinfoButtonInIM"));
				gSavedSettings.getControl("SysinfoButtonInIM")->getCommitSignal()->connect(boost::bind(&FSFloaterIM::onSysinfoButtonVisibilityChanged,this,_2));
				// support sysinfo button -Zi

				break;
			}
			case LLIMModel::LLIMSession::GROUP_SESSION:	// Group chat
			{
				llinfos << "LLIMModel::LLIMSession::GROUP_SESSION start" << llendl;
				getChild<LLLayoutPanel>("profile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("friend_panel")->setVisible(false);
				getChild<LLLayoutPanel>("tp_panel")->setVisible(false);
				getChild<LLLayoutPanel>("share_panel")->setVisible(false);
				getChild<LLLayoutPanel>("pay_panel")->setVisible(false);
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
				getChild<LLLayoutStack>("ls_control_panel")->reshape(140,20,true);
				
				llinfos << "LLIMModel::LLIMSession::GROUP_SESSION end" << llendl;
				break;
			}
			case LLIMModel::LLIMSession::ADHOC_SESSION:	// Conference chat
			{
	llinfos << "LLIMModel::LLIMSession::ADHOC_SESSION  start" << llendl;
				getChild<LLLayoutPanel>("profile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("gprofile_panel")->setVisible(false);
				getChild<LLLayoutPanel>("friend_panel")->setVisible(false);
				getChild<LLLayoutPanel>("tp_panel")->setVisible(false);
				getChild<LLLayoutPanel>("share_panel")->setVisible(false);
				getChild<LLLayoutPanel>("pay_panel")->setVisible(false);
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
				getChild<LLLayoutStack>("ls_control_panel")->reshape(120,20,true);
	llinfos << "LLIMModel::LLIMSession::ADHOC_SESSION end" << llendl;
				break;
			}
			default:
	llinfos << "default buttons start" << llendl;
				getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
				getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);		
	llinfos << "default buttons end" << llendl;
				break;
		}
	}
	LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);
	if(voice_channel)
	{
	llinfos << "voice_channel start" << llendl;
		mVoiceChannelStateChangeConnection = voice_channel->setStateChangedCallback(boost::bind(&FSFloaterIM::onVoiceChannelStateChanged, this, _1, _2));
		
		//call (either p2p, group or ad-hoc) can be already in started state
		updateButtons(voice_channel->getState() >= LLVoiceChannel::STATE_CALL_STARTED);
	llinfos << "voice_channel end" << llendl;
	}
	LLVoiceClient::getInstance()->addObserver((LLVoiceClientStatusObserver*)this);
	
	// </AO>
	

	mInputEditor = getChild<LLLineEditor>("chat_editor");
	//<FS:TS> FIRE-5770: input text buffer is too small
	mInputEditor->setMaxTextLength(3000);
	//</FS:TS> FIRE-5770
	// enable line history support for instant message bar
	mInputEditor->setEnableLineHistory(TRUE);
	// *TODO Establish LineEditor with autoreplace callback
	mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2));

	LLFontGL* font = LLViewerChat::getChatFont();
	mInputEditor->setFont(font);	
	
	mInputEditor->setFocusReceivedCallback( boost::bind(onInputEditorFocusReceived, _1, this) );
	mInputEditor->setFocusLostCallback( boost::bind(onInputEditorFocusLost, _1, this) );
	mInputEditor->setKeystrokeCallback( onInputEditorKeystroke, this );
	mInputEditor->setCommitOnFocusLost( FALSE );
	mInputEditor->setRevertOnEsc( FALSE );
	mInputEditor->setReplaceNewlinesWithSpaces( FALSE );
	mInputEditor->setPassDelete( TRUE );

	childSetCommitCallback("chat_editor", onSendMsg, this);
	
	mChatHistory = getChild<FSChatHistory>("chat_history");

	LLCheckBoxCtrl* FSPrefixBox = getChild<LLCheckBoxCtrl>("FSSupportGroupChatPrefix_toggle");

	BOOL isFSSupportGroup=FSData::getInstance()->isSupportGroup(mSessionID);
	FSPrefixBox->setVisible(isFSSupportGroup);

	// <FS:Zi> Viewer version popup
	if(isFSSupportGroup)
	{
		// check if the dialog was set to ignore
		LLNotificationTemplatePtr templatep=LLNotifications::instance().getTemplate("FirstJoinSupportGroup");
		if(!templatep.get()->mForm->getIgnored())
		{
			// if not, give the user a choice, whether to enable the version prefix or not
			LLSD args;
			LLNotificationsUtil::add("FirstJoinSupportGroup",args,LLSD(),boost::bind(&FSFloaterIM::enableViewerVersionCallback,this,_1,_2));
		}
	}
	// </FS:Zi> Viewer version popup

	setDocked(true);

	mTypingStart = LLTrans::getString("IM_typing_start_string");

	// Disable input editor if session cannot accept text
	LLIMModel::LLIMSession* im_session =
		LLIMModel::instance().findIMSession(mSessionID);
	if( im_session && !im_session->mTextIMPossible )
	{
		mInputEditor->setEnabled(FALSE);
		mInputEditor->setLabel(LLTrans::getString("IM_unavailable_text_label"));
	}

	if ( im_session && im_session->isP2PSessionType())
	{
		// look up display name for window title
		LLAvatarNameCache::get(im_session->mOtherParticipantID,
							   boost::bind(&FSFloaterIM::onAvatarNameCache,
										   this, _1, _2));
	}
	else
	{
		std::string session_name(LLIMModel::instance().getName(mSessionID));
		updateSessionName(session_name, session_name);
	}
	
	//*TODO if session is not initialized yet, add some sort of a warning message like "starting session...blablabla"
	//see LLFloaterIMPanel for how it is done (IB)

	if(isChatMultiTab())
	{
		return LLFloater::postBuild();
	}
	else
	{
		return LLDockableFloater::postBuild();
	}
}

void FSFloaterIM::updateSessionName(const std::string& ui_title,
									const std::string& ui_label)
{
	// <FS:Ansariel> FIRE-7874: Name is missing on tab if announcing incoming IMs is enabled and sender's name is not in name cache
	mSavedTitle = ui_title;

	mInputEditor->setLabel(LLTrans::getString("IM_to_label") + " " + ui_label);
	setTitle(ui_title);	
}

void FSFloaterIM::onAvatarNameCache(const LLUUID& agent_id,
									const LLAvatarName& av_name)
{
	// <FS:Ansariel> FIRE-8658: Let the user decide how the name should be displayed
	// Use display name only for labels, as the extended name will be in the
	// floater title
	//std::string ui_title = av_name.getCompleteName();
	//updateSessionName(ui_title, av_name.mDisplayName);
	//mTypingStart.setArg("[NAME]", ui_title);

	std::string name = av_name.getCompleteName();
	if (LLAvatarNameCache::useDisplayNames())
	{
		switch (gSavedSettings.getS32("FSIMTabNameFormat"))
		{
			// Display name
			case 0:
				name = av_name.mDisplayName;
				break;
			// Username
			case 1:
				name = av_name.mUsername;
				break;
			// Display name (username)
			case 2:
				// Do nothing - we already set the complete name as default
				break;
			// Username (display name)
			case 3:
				if (av_name.mIsDisplayNameDefault)
				{
					name = av_name.mUsername;
				}
				else
				{
					name = av_name.mUsername + " (" + av_name.mDisplayName + ")";
				}
				break;
			default:
				// Do nothing - we already set the complete name as default
				break;
		}
	}

	updateSessionName(name, name);
	mTypingStart.setArg("[NAME]", name);
	llinfos << "Setting IM tab name to '" << name << "'" << llendl;
	// </FS:Ansariel>
}

// virtual
void FSFloaterIM::draw()
{
	if ( mMeTyping )
	{
		// Time out if user hasn't typed for a while.
		if ( mTypingTimeoutTimer.getElapsedTimeF32() > LLAgent::TYPING_TIMEOUT_SECS )
		{
			setTyping(false);
		}
	}

	LLTransientDockableFloater::draw();
}


// static
void* FSFloaterIM::createPanelIMControl(void* userdata)
{
	FSFloaterIM *self = (FSFloaterIM*)userdata;
	self->mControlPanel = new FSPanelIMControlPanel();
	self->mControlPanel->setXMLFilename("panel_fs_im_control_panel.xml");
	return self->mControlPanel;
}


// static
void* FSFloaterIM::createPanelGroupControl(void* userdata)
{
	FSFloaterIM *self = (FSFloaterIM*)userdata;
	self->mControlPanel = new FSPanelGroupControlPanel(self->mSessionID);
	self->mControlPanel->setXMLFilename("panel_fs_group_control_panel.xml");
	return self->mControlPanel;
}

// static
void* FSFloaterIM::createPanelAdHocControl(void* userdata)
{
	FSFloaterIM *self = (FSFloaterIM*)userdata;
	self->mControlPanel = new FSPanelAdHocControlPanel(self->mSessionID);
	self->mControlPanel->setXMLFilename("panel_fs_adhoc_control_panel.xml");
	return self->mControlPanel;
}

void FSFloaterIM::onSlide()
{
	mControlPanel->getParent()->setVisible(!mControlPanel->getParent()->getVisible());

	gSavedSettings.setBOOL("IMShowControlPanel", mControlPanel->getParent()->getVisible());

	getChild<LLButton>("slide_left_btn")->setVisible(mControlPanel->getParent()->getVisible());
	getChild<LLButton>("slide_right_btn")->setVisible(!mControlPanel->getParent()->getVisible());
}

//static
FSFloaterIM* FSFloaterIM::show(const LLUUID& session_id)
{
	closeHiddenIMToasts();

	if (!gIMMgr->hasSession(session_id)) return NULL;

	if(!isChatMultiTab())
	{
		//hide all
		LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
		for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
			 iter != inst_list.end(); ++iter)
		{
			FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
			if (floater && floater->isDocked())
			{
				floater->setVisible(false);
			}
		}
	}

	bool exist = findInstance(session_id);

	FSFloaterIM* floater = getInstance(session_id);
	if (!floater) return NULL;

	if(isChatMultiTab())
	{
		FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();

		// do not add existed floaters to avoid adding torn off instances
		if (!exist)
		{
			//		LLTabContainer::eInsertionPoint i_pt = user_initiated ? LLTabContainer::RIGHT_OF_CURRENT : LLTabContainer::END;
			// TODO: mantipov: use LLTabContainer::RIGHT_OF_CURRENT if it exists
			LLTabContainer::eInsertionPoint i_pt = LLTabContainer::END;
			
			if (floater_container)
			{
				floater_container->addFloater(floater, TRUE, i_pt);
			}
		}

		floater->openFloater(floater->getKey());
	}
	else
	{
		// Docking may move chat window, hide it before moving, or user will see how window "jumps"
		floater->setVisible(false);

		if (floater->getDockControl() == NULL)
		{
			LLChiclet* chiclet =
					LLChicletBar::getInstance()->getChicletPanel()->findChiclet<LLChiclet>(
							session_id);
			if (chiclet == NULL)
			{
				llerror("Dock chiclet for FSFloaterIM doesn't exists", 0);
			}
			else
			{
				LLChicletBar::getInstance()->getChicletPanel()->scrollToChiclet(chiclet);
			}

			// <FS:Ansariel> Group notices, IMs and chiclets position
			//floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
			//		LLDockControl::BOTTOM));
			if (gSavedSettings.getBOOL("InternalShowGroupNoticesTopRight"))
			{
				floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
						LLDockControl::BOTTOM));
			}
			else
			{
				floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
						LLDockControl::TOP));
			}
			// </FS:Ansariel> Group notices, IMs and chiclets position
		}

		// window is positioned, now we can show it.
		floater->setVisible(TRUE);
	}

	return floater;
}

void FSFloaterIM::setDocked(bool docked, bool pop_on_undock)
{
	// update notification channel state
	LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
		(LLNotificationsUI::LLChannelManager::getInstance()->
											findChannelByID(LLUUID(gSavedSettings.getString("NotificationChannelUUID"))));
	
	if(!isChatMultiTab())
	{
		LLTransientDockableFloater::setDocked(docked, pop_on_undock);
	}

	// update notification channel state
	if(channel)
	{
		channel->updateShowToastsState();
		channel->redrawToasts();
	}
}

void FSFloaterIM::setVisible(BOOL visible)
{
	LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
		(LLNotificationsUI::LLChannelManager::getInstance()->
											findChannelByID(LLUUID(gSavedSettings.getString("NotificationChannelUUID"))));
	LLTransientDockableFloater::setVisible(visible);

	// update notification channel state
	if(channel)
	{
		channel->updateShowToastsState();
		channel->redrawToasts();
	}

	BOOL is_minimized = visible && isChatMultiTab()
		? FSFloaterIMContainer::getInstance()->isMinimized()
		: !visible;

	if (!is_minimized && mChatHistory && mInputEditor)
	{
		//only if floater was construced and initialized from xml
		updateMessages();
		FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();
		
		//prevent stealing focus when opening a background IM tab (EXT-5387, checking focus for EXT-6781)
		// If this is docked, is the selected tab, and the im container has focus, put focus in the input ctrl -KC
		bool is_active = im_container->getActiveFloater() == this && im_container->hasFocus();
		if (!isChatMultiTab() || is_active || hasFocus())
		{
			mInputEditor->setFocus(TRUE);
		}
	}

	if(!visible)
	{
		LLIMChiclet* chiclet = LLChicletBar::getInstance()->getChicletPanel()->findChiclet<LLIMChiclet>(mSessionID);
		if(chiclet)
		{
			chiclet->setToggleState(false);
		}
	}
}

BOOL FSFloaterIM::getVisible()
{
	if(isChatMultiTab())
	{
		FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();
		
		// Treat inactive floater as invisible.
		bool is_active = im_container->getActiveFloater() == this;
	
		//torn off floater is always inactive
		if (!is_active && getHost() != im_container)
		{
			return LLTransientDockableFloater::getVisible();
		}

		// getVisible() returns TRUE when Tabbed IM window is minimized.
		return is_active && !im_container->isMinimized() && im_container->getVisible();
	}
	else
	{
		return LLTransientDockableFloater::getVisible();
	}
}

//static
bool FSFloaterIM::toggle(const LLUUID& session_id)
{
	if(!isChatMultiTab())
	{
		FSFloaterIM* floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
		if (floater && floater->getVisible() && floater->hasFocus())
		{
			// clicking on chiclet to close floater just hides it to maintain existing
			// scroll/text entry state
			floater->setVisible(false);
			return false;
		}
		else if(floater && (!floater->isDocked() || (floater->getVisible() && !floater->hasFocus())))
		{
			floater->setVisible(TRUE);
			floater->setFocus(TRUE);
			return true;
		}
	}

	// ensure the list of messages is updated when floater is made visible
	show(session_id);
	return true;
}

//static
FSFloaterIM* FSFloaterIM::findInstance(const LLUUID& session_id)
{
	return LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
}

FSFloaterIM* FSFloaterIM::getInstance(const LLUUID& session_id)
{
	return LLFloaterReg::getTypedInstance<FSFloaterIM>("fs_impanel", session_id);
}

void FSFloaterIM::sessionInitReplyReceived(const LLUUID& im_session_id)
{
	mSessionInitialized = true;

	//will be different only for an ad-hoc im session
	if (mSessionID != im_session_id)
	{
		mSessionID = im_session_id;
		setKey(im_session_id);
		mControlPanel->setSessionId(im_session_id);
	}

	// updating "Call" button from group control panel here to enable it without placing into draw() (EXT-4796)
	if(gAgent.isInGroup(im_session_id))
	{
		mControlPanel->updateCallButton();
	}
	
	//*TODO here we should remove "starting session..." warning message if we added it in postBuild() (IB)


	//need to send delayed messaged collected while waiting for session initialization
	if (!mQueuedMsgsForInit.size()) return;
	LLSD::array_iterator iter;
	for ( iter = mQueuedMsgsForInit.beginArray();
		iter != mQueuedMsgsForInit.endArray();
		++iter)
	{
		LLIMModel::sendMessage(iter->asString(), mSessionID,
			mOtherParticipantUUID, mDialog);
	}
}

void FSFloaterIM::updateMessages()
{
	bool use_plain_text_chat_history = gSavedSettings.getBOOL("PlainTextChatHistory");
	//<FS:HG> FS-1734 seperate name and text styles for moderator
	//bool bold_mods_chat = gSavedSettings.getBOOL("FSBoldGroupMods");
	bool highlight_mods_chat = gSavedSettings.getBOOL("FSHighlightGroupMods");
	bool hide_timestamps_nearby_chat = gSavedSettings.getBOOL("FSHideTimestampsIM");

	std::list<LLSD> messages;

	// we shouldn't reset unread message counters if IM floater doesn't have focus
	if (hasFocus())
	{
		LLIMModel::instance().getMessages(mSessionID, messages, mLastMessageIndex+1);
	}
	else
	{
		LLIMModel::instance().getMessagesSilently(mSessionID, messages, mLastMessageIndex+1);
	}

	if (messages.size())
	{
		LLSD chat_args;
		chat_args["use_plain_text_chat_history"] = use_plain_text_chat_history;
		chat_args["hide_timestamps_nearby_chat"] = hide_timestamps_nearby_chat;
		
		LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
		RLV_ASSERT(pIMSession);

		std::ostringstream message;
		std::list<LLSD>::const_reverse_iterator iter = messages.rbegin();
		std::list<LLSD>::const_reverse_iterator iter_end = messages.rend();
		for (; iter != iter_end; ++iter)
		{
			LLSD msg = *iter;

			std::string time = msg["time"].asString();
			LLUUID from_id = msg["from_id"].asUUID();
			std::string from = msg["from"].asString();
			std::string message = msg["message"].asString();
			bool is_history = msg["is_history"].asBoolean();

			LLChat chat;
			chat.mFromID = from_id;
			chat.mSessionID = mSessionID;
			chat.mFromName = from;
			chat.mTimeStr = time;
			chat.mChatStyle = is_history ? CHAT_STYLE_HISTORY : chat.mChatStyle;			
			
			// Bold group moderators' chat -KC
			//<FS:HG> FS-1734 seperate name and text styles for moderator
			//if (!is_history && bold_mods_chat && pIMSession && pIMSession->mSpeakers)
			if (!is_history && highlight_mods_chat && pIMSession && pIMSession->mSpeakers)
			{
				LLPointer<LLSpeaker> speakerp = pIMSession->mSpeakers->findSpeaker(from_id);
				if (speakerp && speakerp->mIsModerator)
				{
					chat.mChatStyle = CHAT_STYLE_MODERATOR;
				}
			}

			// process offer notification
			if (msg.has("notification_id"))
			{
				chat.mNotifId = msg["notification_id"].asUUID();
				// if notification exists - embed it
				if (LLNotificationsUtil::find(chat.mNotifId) != NULL)
				{
					// remove embedded notification from channel
					LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
							(LLNotificationsUI::LLChannelManager::getInstance()->
																findChannelByID(LLUUID(gSavedSettings.getString("NotificationChannelUUID"))));
					if (getVisible())
					{
						// toast will be automatically closed since it is not storable toast
						channel->hideToast(chat.mNotifId);
					}
				}
				// if notification doesn't exist - try to use next message which should be log entry
				else
				{
					continue;
				}
			}
			//process text message
			else
			{
				chat.mText = message;
			}
			
			mChatHistory->appendMessage(chat, chat_args);
			mLastMessageIndex = msg["index"].asInteger();

			// if it is a notification - next message is a notification history log, so skip it
			if (chat.mNotifId.notNull() && LLNotificationsUtil::find(chat.mNotifId) != NULL)
			{
				if (++iter == iter_end)
				{
					break;
				}
				else
				{
					mLastMessageIndex++;
				}
			}
		}
	}
}

void FSFloaterIM::reloadMessages()
{
	mChatHistory->clear();
	mLastMessageIndex = -1;
	updateMessages();
}

// static
void FSFloaterIM::onInputEditorFocusReceived( LLFocusableElement* caller, void* userdata )
{
	FSFloaterIM* self= (FSFloaterIM*) userdata;

	// Allow enabling the FSFloaterIM input editor only if session can accept text
	LLIMModel::LLIMSession* im_session =
		LLIMModel::instance().findIMSession(self->mSessionID);
	//TODO: While disabled lllineeditor can receive focus we need to check if it is enabled (EK)
	if( im_session && im_session->mTextIMPossible && self->mInputEditor->getEnabled())
	{
		//in disconnected state IM input editor should be disabled
		self->mInputEditor->setEnabled(!gDisconnected);
	}
}

// static
void FSFloaterIM::onInputEditorFocusLost(LLFocusableElement* caller, void* userdata)
{
	FSFloaterIM* self = (FSFloaterIM*) userdata;
	self->setTyping(false);
}

// static
void FSFloaterIM::onInputEditorKeystroke(LLLineEditor* caller, void* userdata)
{
	FSFloaterIM* self = (FSFloaterIM*)userdata;
	std::string text = self->mInputEditor->getText();
	if (!text.empty())
	{
		self->setTyping(true);
	}
	else
	{
		// Deleting all text counts as stopping typing.
		self->setTyping(false);
	}
}

void FSFloaterIM::setTyping(bool typing)
{
	if ( typing )
	{
		// Started or proceeded typing, reset the typing timeout timer
		mTypingTimeoutTimer.reset();
	}

	if ( mMeTyping != typing )
	{
		// Typing state is changed
		mMeTyping = typing;
		// So, should send current state
		mShouldSendTypingState = true;
		// In case typing is started, send state after some delay
		mTypingTimer.reset();
	}

	// Don't want to send typing indicators to multiple people, potentially too
	// much network traffic. Only send in person-to-person IMs.
	if ( mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL )
	{
		if ( mMeTyping )
		{
			if ( mTypingTimer.getElapsedTimeF32() > 1.f )
			{
				// Still typing, send 'start typing' notification
				LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, TRUE);
				mShouldSendTypingState = false;
			}
		}
		else
		{
			// Send 'stop typing' notification immediately
			LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, FALSE);
			mShouldSendTypingState = false;
		}
	}

	LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
	if (speaker_mgr)
		speaker_mgr->setSpeakerTyping(gAgent.getID(), FALSE);

}

void FSFloaterIM::processIMTyping(const LLIMInfo* im_info, BOOL typing)
{
	if ( typing )
	{
		// other user started typing
		addTypingIndicator(im_info);
	}
	else
	{
		// other user stopped typing
		removeTypingIndicator(im_info);
	}
}

void FSFloaterIM::processAgentListUpdates(const LLSD& body)
{
	if ( !body.isMap() ) return;

	if ( body.has("agent_updates") && body["agent_updates"].isMap() )
	{
		LLSD agent_data = body["agent_updates"].get(gAgentID.asString());
		if (agent_data.isMap() && agent_data.has("info"))
		{
			LLSD agent_info = agent_data["info"];

			if (agent_info.has("mutes"))
			{
				BOOL moderator_muted_text = agent_info["mutes"]["text"].asBoolean(); 
				mInputEditor->setEnabled(!moderator_muted_text);
				std::string label;
				if (moderator_muted_text)
					label = LLTrans::getString("IM_muted_text_label");
				else
					label = LLTrans::getString("IM_to_label") + " " + LLIMModel::instance().getName(mSessionID);
				mInputEditor->setLabel(label);

				if (moderator_muted_text)
					LLNotificationsUtil::add("TextChatIsMutedByModerator");
			}
		}
	}
}

void FSFloaterIM::updateChatHistoryStyle()
{
	mChatHistory->clear();
	mLastMessageIndex = -1;
	updateMessages();
}

void FSFloaterIM::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
	LLFontGL* font = LLViewerChat::getChatFont();
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
		 iter != inst_list.end(); ++iter)
	{
		FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
		if (floater)
		{
			floater->updateChatHistoryStyle();
			floater->mInputEditor->setFont(font);
		}
	}

}

void FSFloaterIM::processSessionUpdate(const LLSD& session_update)
{
	// *TODO : verify following code when moderated mode will be implemented
	if ( false && session_update.has("moderated_mode") &&
		 session_update["moderated_mode"].has("voice") )
	{
		BOOL voice_moderated = session_update["moderated_mode"]["voice"];
		const std::string session_label = LLIMModel::instance().getName(mSessionID);

		if (voice_moderated)
		{
			setTitle(session_label + std::string(" ") + LLTrans::getString("IM_moderated_chat_label"));
		}
		else
		{
			setTitle(session_label);
		}

		// *TODO : uncomment this when/if LLPanelActiveSpeakers panel will be added
		//update the speakers dropdown too
		//mSpeakerPanel->setVoiceModerationCtrlMode(voice_moderated);
	}
}

BOOL FSFloaterIM::handleDragAndDrop(S32 x, S32 y, MASK mask,
						   BOOL drop, EDragAndDropType cargo_type,
						   void *cargo_data, EAcceptance *accept,
						   std::string& tooltip_msg)
{

	if (mDialog == IM_NOTHING_SPECIAL)
	{
		LLToolDragAndDrop::handleGiveDragAndDrop(mOtherParticipantUUID, mSessionID, drop,
												 cargo_type, cargo_data, accept);
	}

	// handle case for dropping calling cards (and folders of calling cards) onto invitation panel for invites
	else if (isInviteAllowed())
	{
		*accept = ACCEPT_NO;

		if (cargo_type == DAD_CALLINGCARD)
		{
			if (dropCallingCard((LLInventoryItem*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
		else if (cargo_type == DAD_CATEGORY)
		{
			if (dropCategory((LLInventoryCategory*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
	}
	return TRUE;
}

BOOL FSFloaterIM::dropCallingCard(LLInventoryItem* item, BOOL drop)
{
	BOOL rv = isInviteAllowed();
	if(rv && item && item->getCreatorUUID().notNull())
	{
		if(drop)
		{
			uuid_vec_t ids;
			ids.push_back(item->getCreatorUUID());
			inviteToSession(ids);
		}
	}
	else
	{
		// set to false if creator uuid is null.
		rv = FALSE;
	}
	return rv;
}

BOOL FSFloaterIM::dropCategory(LLInventoryCategory* category, BOOL drop)
{
	BOOL rv = isInviteAllowed();
	if(rv && category)
	{
		LLInventoryModel::cat_array_t cats;
		LLInventoryModel::item_array_t items;
		LLUniqueBuddyCollector buddies;
		gInventory.collectDescendentsIf(category->getUUID(),
										cats,
										items,
										LLInventoryModel::EXCLUDE_TRASH,
										buddies);
		S32 count = items.count();
		if(count == 0)
		{
			rv = FALSE;
		}
		else if(drop)
		{
			uuid_vec_t ids;
			ids.reserve(count);
			for(S32 i = 0; i < count; ++i)
			{
				ids.push_back(items.get(i)->getCreatorUUID());
			}
			inviteToSession(ids);
		}
	}
	return rv;
}

BOOL FSFloaterIM::isInviteAllowed() const
{

	return ( (IM_SESSION_CONFERENCE_START == mDialog)
			 || (IM_SESSION_INVITE == mDialog) );
}

class LLSessionInviteResponder : public LLHTTPClient::Responder
{
public:
	LLSessionInviteResponder(const LLUUID& session_id)
	{
		mSessionID = session_id;
	}

	void errorWithContent(U32 statusNum, const std::string& reason, const LLSD& content)
	{
		llwarns << "Error inviting all agents to session [status:" 
				<< statusNum << "]: " << content << llendl;
		//throw something back to the viewer here?
	}

private:
	LLUUID mSessionID;
};

BOOL FSFloaterIM::inviteToSession(const uuid_vec_t& ids)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		return FALSE;
	}

	S32 count = ids.size();

	if( isInviteAllowed() && (count > 0) )
	{
		llinfos << "FSFloaterIM::inviteToSession() - inviting participants" << llendl;

		std::string url = region->getCapability("ChatSessionRequest");

		LLSD data;

		data["params"] = LLSD::emptyArray();
		for (int i = 0; i < count; i++)
		{
			data["params"].append(ids[i]);
		}

		data["method"] = "invite";
		data["session-id"] = mSessionID;
		LLHTTPClient::post(
			url,
			data,
			new LLSessionInviteResponder(
					mSessionID));
	}
	else
	{
		llinfos << "FSFloaterIM::inviteToSession -"
				<< " no need to invite agents for "
				<< mDialog << llendl;
		// successful add, because everyone that needed to get added
		// was added.
	}

	return TRUE;
}

void FSFloaterIM::addTypingIndicator(const LLIMInfo* im_info)
{
	// We may have lost a "stop-typing" packet, don't add it twice
	if ( im_info && !mOtherTyping )
	{
		mOtherTyping = true;

		// Save and set new title
		mSavedTitle = getTitle();
		setTitle (mTypingStart);

		// Update speaker
		LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
		if ( speaker_mgr )
		{
			speaker_mgr->setSpeakerTyping(im_info->mFromID, TRUE);
		}
	}
}

void FSFloaterIM::removeTypingIndicator(const LLIMInfo* im_info)
{
	if ( mOtherTyping )
	{
		mOtherTyping = false;

		// Revert the title to saved one
		setTitle(mSavedTitle);

		if ( im_info )
		{
			// Update speaker
			LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if ( speaker_mgr )
			{
				speaker_mgr->setSpeakerTyping(im_info->mFromID, FALSE);
			}
		}

	}
}

// static
void FSFloaterIM::closeHiddenIMToasts()
{
	class IMToastMatcher: public LLNotificationsUI::LLScreenChannel::Matcher
	{
	public:
		bool matches(const LLNotificationPtr notification) const
		{
			// "notifytoast" type of notifications is reserved for IM notifications
			return "notifytoast" == notification->getType();
		}
	};

	LLNotificationsUI::LLScreenChannel* channel = LLNotificationsUI::LLChannelManager::getNotificationScreenChannel();
	if (channel != NULL)
	{
		channel->closeHiddenToasts(IMToastMatcher());
	}
}
// static
void FSFloaterIM::confirmLeaveCallCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	const LLSD& payload = notification["payload"];
	LLUUID session_id = payload["session_id"];

	LLFloater* im_floater = LLFloaterReg::findInstance("fs_impanel", session_id);
	if (option == 0 && im_floater != NULL)
	{
		im_floater->closeFloater();
	}

	return;
}

// static
bool FSFloaterIM::isChatMultiTab()
{
	// Restart is required in order to change chat window type.
	static bool is_single_window = gSavedSettings.getS32("ChatWindow") == 1;
	return is_single_window;
}

// static
void FSFloaterIM::initIMFloater()
{
	// This is called on viewer start up
	// init chat window type before user changed it in preferences
	isChatMultiTab();
}

//static
void FSFloaterIM::sRemoveTypingIndicator(const LLSD& data)
{
	LLUUID session_id = data["session_id"];
	if (session_id.isNull()) return;

	LLUUID from_id = data["from_id"];
	if (gAgentID == from_id || LLUUID::null == from_id) return;

	FSFloaterIM* floater = FSFloaterIM::findInstance(session_id);
	if (!floater) return;

	if (IM_NOTHING_SPECIAL != floater->mDialog) return;

	floater->removeTypingIndicator();
}

void FSFloaterIM::onIMChicletCreated( const LLUUID& session_id )
{

	if (isChatMultiTab())
	{
		FSFloaterIMContainer* im_box = FSFloaterIMContainer::getInstance();
		if (!im_box) return;

		if (FSFloaterIM::findInstance(session_id)) return;

		FSFloaterIM* new_tab = FSFloaterIM::getInstance(session_id);

		im_box->addFloater(new_tab, FALSE, LLTabContainer::END);
	}

}

void	FSFloaterIM::onClickCloseBtn()
{

	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
				mSessionID);

	if (session == NULL)
	{
		llwarns << "Empty session." << llendl;
		return;
	}

	bool is_call_with_chat = session->isGroupSessionType()
			|| session->isAdHocSessionType() || session->isP2PSessionType();

	LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

	if (is_call_with_chat && voice_channel != NULL && voice_channel->isActive())
	{
		LLSD payload;
		payload["session_id"] = mSessionID;
		LLNotificationsUtil::add("ConfirmLeaveCall", LLSD(), payload, confirmLeaveCallCallback);
		return;
	}

	LLFloater::onClickCloseBtn();
}

// <FS:Zi> Viewer version popup
BOOL FSFloaterIM::enableViewerVersionCallback(const LLSD& notification,const LLSD& response)
{
	S32 option=LLNotificationsUtil::getSelectedOption(notification,response);

	BOOL result=FALSE;
	if(option==0)		// "yes"
	{
		result=TRUE;
	}

	gSavedSettings.setBOOL("FSSupportGroupChatPrefix2",result);
	return result;
}
// </FS:Zi>

// <FS:Ansariel> FIRE-3248: Disable add friend button on IM floater if friendship request accepted
void FSFloaterIM::setEnableAddFriendButton(BOOL enabled)
{
	getChild<LLButton>("add_friend_btn")->setEnabled(enabled);
}
// </FS:Ansariel>
