/** 
 * @file fsfloaterimcontainer.cpp
 * @brief Multifloater containing active IM sessions in separate tab container tabs
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

// Original file: llimfloatercontainer.cpp


#include "llviewerprecompiledheaders.h"

#include "fsfloaterimcontainer.h"
#include "llfloaterreg.h"
#include "llimview.h"
#include "llavatariconctrl.h"
#include "llgroupiconctrl.h"
#include "llagent.h"
#include "lltransientfloatermgr.h"
#include "fsfloaternearbychat.h"
#include "fscontactsfloater.h"
#include "llfloater.h"
#include "llviewercontrol.h"
#include "fsfloaterim.h"
#include "llvoiceclient.h"

static const F32 VOICE_STATUS_UPDATE_INTERVAL = 1.0f;

//
// FSFloaterIMContainer
//
FSFloaterIMContainer::FSFloaterIMContainer(const LLSD& seed)
:	LLMultiFloater(seed),
	mActiveVoiceFloater(NULL),
	mCurrentVoiceState(VOICE_STATE_NONE),
	mForceVoiceStateUpdate(false)
{
	mAutoResize = FALSE;
	LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::IM, this);

	// Firstly add our self to IMSession observers, so we catch session events
	LLIMMgr::getInstance()->addSessionObserver(this);
}

FSFloaterIMContainer::~FSFloaterIMContainer()
{
	mNewMessageConnection.disconnect();
	LLTransientFloaterMgr::getInstance()->removeControlView(LLTransientFloaterMgr::IM, this);

	if (!LLSingleton<LLIMMgr>::destroyed())
	{
		LLIMMgr::getInstance()->removeSessionObserver(this);
	}
}

BOOL FSFloaterIMContainer::postBuild()
{
	mNewMessageConnection = LLIMModel::instance().mNewMsgSignal.connect(boost::bind(&FSFloaterIMContainer::onNewMessageReceived, this, _1));
	// Do not call base postBuild to not connect to mCloseSignal to not close all floaters via Close button
	// mTabContainer will be initialized in LLMultiFloater::addChild()

	mActiveVoiceUpdateTimer.setTimerExpirySec(VOICE_STATUS_UPDATE_INTERVAL);
	mActiveVoiceUpdateTimer.start();

	gSavedSettings.getControl("FSShowConversationVoiceStateIndicator")->getSignal()->connect(boost::bind(&FSFloaterIMContainer::onVoiceStateIndicatorChanged, this, _2));

	return TRUE;
}

void FSFloaterIMContainer::onOpen(const LLSD& key)
{
	LLMultiFloater::onOpen(key);

	// If we're using multitabs, and we open up for the first time
	// Add localchat by default if it's not already on the screen somewhere else. -AO	
	// But only if it hasnt been already so we can reopen it to the same tab -KC
	// Improved handling to leave most of the work to the LL tear-off code -Zi
	// This is mirrored from FSFloaterContacts::onOpen() and FSFloaterNearbyChat::onOpen()
	// respectively: If those floaters are hosted, they don't store their visibility state.
	// Instead, the visibility state of the hosting container is stored. (See Zi's changes to
	// LLFloater::storeVisibilityControl()) That means if contacts and/or nearby chat floater
	// are hosted and FSFloaterIMContainer was visible at logout, we will end up here during
	// next login and have to configure those floaters so their tear off state and icon is
	// correct. Configure contacts first and nearby chat last so nearby chat will be active
	// once FSFloaterIMContainer has opened. -AH

	FSFloaterContacts* floater_contacts = FSFloaterContacts::getInstance();
	if (!LLFloater::isVisible(floater_contacts) && (floater_contacts->getHost() != this))
	{
		if (gSavedSettings.getBOOL("ContactsTornOff"))
		{
			// first set the tear-off host to the conversations container
			floater_contacts->setHost(this);
			// clear the tear-off host right after, the "last host used" will still stick
			setHost(NULL);
			// reparent to floater view
			gFloaterView->addChild(floater_contacts);
		}
		else
		{
			LLMultiFloater::showFloater(floater_contacts);
		}
	}

	LLFloater* floater_chat = FSFloaterNearbyChat::getInstance();
	if (!LLFloater::isVisible(floater_chat) && (floater_chat->getHost() != this))
	{
		if (gSavedSettings.getBOOL("ChatHistoryTornOff"))
		{
			// first set the tear-off host to this floater
			floater_chat->setHost(this);
			// clear the tear-off host right after, the "last host used" will still stick
			floater_chat->setHost(NULL);
			// reparent to floater view
			gFloaterView->addChild(floater_chat);
		}
		else
		{
			LLMultiFloater::showFloater(floater_chat);
		}
	}

	LLFloater* active_floater = getActiveFloater();
	if (active_floater && !active_floater->hasFocus())
	{
		active_floater->setFocus(TRUE);
	}
}

void FSFloaterIMContainer::addFloater(LLFloater* floaterp, 
									BOOL select_added_floater, 
									LLTabContainer::eInsertionPoint insertion_point)
{
	if(!floaterp) return;

	// already here
	if (floaterp->getHost() == this)
	{
		openFloater(floaterp->getKey());
		return;
	}

	// Need to force an update on the voice state because torn off floater might get re-attached
	mForceVoiceStateUpdate = true;
	
	if (floaterp->getName() == "imcontacts" || floaterp->getName() == "nearby_chat")
	{
		S32 num_locked_tabs = mTabContainer->getNumLockedTabs();
		mTabContainer->unlockTabs();
		// add contacts window as first tab
		if (floaterp->getName() == "imcontacts")
		{
			LLMultiFloater::addFloater(floaterp, select_added_floater, LLTabContainer::START);
			gSavedSettings.setBOOL("ContactsTornOff", FALSE);
		}
		else
		{
			// add chat history as second tab if contact window is present, first tab otherwise
			if (getChildView("imcontacts"))
			{
				// assuming contacts window is first tab, select it
				mTabContainer->selectFirstTab();
				// and add ourselves after
				LLMultiFloater::addFloater(floaterp, select_added_floater, LLTabContainer::RIGHT_OF_CURRENT);
			}
			else
			{
				LLMultiFloater::addFloater(floaterp, select_added_floater, LLTabContainer::START);
			}
			gSavedSettings.setBOOL("ChatHistoryTornOff", FALSE);
		}
		// make sure first two tabs are now locked
		mTabContainer->lockTabs(num_locked_tabs + 1);
		
		floaterp->setCanClose(FALSE);
		return;
	}

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-17 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLUUID session_id = floaterp->getKey();
	if (session_id.isNull())
	{
		// Re-insert the nearby chat floater at the start
		insertion_point = LLTabContainer::START;
	}
// [/SL:KB]

	LLMultiFloater::addFloater(floaterp, select_added_floater, insertion_point);

	if (session_id.isNull())
	{
		// Don't allow the nearby chat tab to be drag-rearranged
		mTabContainer->lockTabs(1);
	}
	else
	{
		mSessions[session_id] = floaterp;
		floaterp->mCloseSignal.connect(boost::bind(&FSFloaterIMContainer::onCloseFloater, this, session_id));
	}
}

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-11 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
void FSFloaterIMContainer::removeFloater(LLFloater* floaterp)
{
	// <FS:ND>  old code from FS
	if (floaterp->getName() == "nearby_chat")
	{
		// only my friends floater now locked
		mTabContainer->lockTabs(mTabContainer->getNumLockedTabs() - 1);
		gSavedSettings.setBOOL("ChatHistoryTornOff", TRUE);
		floaterp->setCanClose(TRUE);
	}
	else if (floaterp->getName() == "imcontacts")
	{
		// only chat floater now locked
		mTabContainer->lockTabs(mTabContainer->getNumLockedTabs() - 1);
		gSavedSettings.setBOOL("ContactsTornOff", TRUE);
		floaterp->setCanClose(TRUE);
	}
	// </FS:ND>


	LLUUID idSession = floaterp->getKey();
	if (idSession.isNull())
	{
		mTabContainer->unlockTabs();
	}
	LLMultiFloater::removeFloater(floaterp);
}
// [/SL:KB]

bool FSFloaterIMContainer::hasFloater(LLFloater* floaterp)
{
	for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
	{
		if (dynamic_cast<LLFloater*>(mTabContainer->getPanelByIndex(i)) == floaterp)
		{
			return true;
		}
	}
	return false;
}

void FSFloaterIMContainer::onCloseFloater(LLUUID& id)
{
	mSessions.erase(id);
	setFocus(TRUE);
}

void FSFloaterIMContainer::onNewMessageReceived(const LLSD& data)
{
	LLUUID session_id = data["session_id"].asUUID();
	LLFloater* floaterp = get_ptr_in_map(mSessions, session_id);
	LLFloater* current_floater = LLMultiFloater::getActiveFloater();

	// KC: Don't flash tab on friend status changes per setting
	if (floaterp && current_floater && floaterp != current_floater
		&& (gSavedSettings.getBOOL("FSIMChatFlashOnFriendStatusChange") || !data.has("from_id") || data["from_id"].asUUID().notNull()))
	{
		if(LLMultiFloater::isFloaterFlashing(floaterp))
			LLMultiFloater::setFloaterFlashing(floaterp, FALSE);
		LLMultiFloater::setFloaterFlashing(floaterp, TRUE);
	}
}

FSFloaterIMContainer* FSFloaterIMContainer::findInstance()
{
	return LLFloaterReg::findTypedInstance<FSFloaterIMContainer>("fs_im_container");
}

FSFloaterIMContainer* FSFloaterIMContainer::getInstance()
{
	return LLFloaterReg::getTypedInstance<FSFloaterIMContainer>("fs_im_container");
}

void FSFloaterIMContainer::setMinimized(BOOL b)
{
	if (isMinimized() == b) return;
	
	LLMultiFloater::setMinimized(b);
	// Hide minimized floater (see EXT-5315)
	setVisible(!b);

	if (isMinimized()) return;

	if (getActiveFloater())
	{
		getActiveFloater()->setVisible(TRUE);
	}
}


//virtual
void FSFloaterIMContainer::sessionAdded(const LLUUID& session_id, const std::string& name, const LLUUID& other_participant_id, BOOL has_offline_msg)
{
	LLIMModel::LLIMSession* session = LLIMModel::getInstance()->findIMSession(session_id);
	if (!session) return;

	// no need to spawn chiclets for participants in P2P calls called through Avaline
	if (session->isP2P() && session->isOtherParticipantAvaline()) return;

	FSFloaterIM::onNewIMReceived(session_id);
}

//virtual
void FSFloaterIMContainer::sessionRemoved(const LLUUID& session_id)
{
	FSFloaterIM* iMfloater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
	if (iMfloater != NULL)
	{
		iMfloater->closeFloater();
	}
}

// static
void FSFloaterIMContainer::reloadEmptyFloaters()
{
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
		iter != inst_list.end(); ++iter)
	{
		FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
		if (floater && floater->getLastChatMessageIndex() == -1)
		{
			floater->reloadMessages(true);
		}
	}

	FSFloaterNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat");
	if (nearby_chat && nearby_chat->getMessageArchiveLength() == 0)
	{
		nearby_chat->reloadMessages(true);
	}
}

void FSFloaterIMContainer::onVoiceStateIndicatorChanged(const LLSD& data)
{
	if (!data.asBoolean())
	{
		if (mActiveVoiceFloater)
		{
			mTabContainer->setTabImage(mActiveVoiceFloater, "");
			mActiveVoiceFloater = NULL;
		}
		mCurrentVoiceState = VOICE_STATE_NONE;
	}
}

// virtual
void FSFloaterIMContainer::draw()
{
	static LLCachedControl<bool> fsShowConversationVoiceStateIndicator(gSavedSettings, "FSShowConversationVoiceStateIndicator");
	if (fsShowConversationVoiceStateIndicator && (mActiveVoiceUpdateTimer.hasExpired() || mForceVoiceStateUpdate))
	{
		LLFloater* current_voice_floater = getCurrentVoiceFloater();
		if (mActiveVoiceFloater != current_voice_floater)
		{
			if (mActiveVoiceFloater)
			{
				mTabContainer->setTabImage(mActiveVoiceFloater, "");
				mCurrentVoiceState = VOICE_STATE_NONE;
			}
		}

		if (current_voice_floater)
		{
			static LLUIColor voice_connected_color = LLUIColorTable::instance().getColor("VoiceConnectedColor", LLColor4::green);
			static LLUIColor voice_error_color = LLUIColorTable::instance().getColor("VoiceErrorColor", LLColor4::red);
			static LLUIColor voice_not_connected_color = LLUIColorTable::instance().getColor("VoiceNotConnectedColor", LLColor4::yellow);

			eVoiceState voice_state = VOICE_STATE_UNKNOWN;
			LLVoiceChannel* voice_channel = LLVoiceChannel::getCurrentVoiceChannel();
			if (voice_channel)
			{
				if (voice_channel->isActive())
				{
					voice_state = VOICE_STATE_CONNECTED;
				}
				else if (voice_channel->getState() == LLVoiceChannel::STATE_ERROR)
				{
					voice_state = VOICE_STATE_ERROR;
				}
				else
				{
					voice_state = VOICE_STATE_NOT_CONNECTED;
				}
			}

			if (voice_state != mCurrentVoiceState || mForceVoiceStateUpdate)
			{
				LLColor4 icon_color;
				switch (voice_state)
				{
					case VOICE_STATE_CONNECTED:
						icon_color = voice_connected_color.get();
						break;
					case VOICE_STATE_ERROR:
						icon_color = voice_error_color.get();
						break;
					case VOICE_STATE_NOT_CONNECTED:
						icon_color = voice_not_connected_color.get();
						break;
					default:
						icon_color = LLColor4::white;
						break;
				}
				mTabContainer->setTabImage(current_voice_floater, "Active_Voice_Tab", LLFontGL::RIGHT, icon_color, icon_color);
				mCurrentVoiceState = voice_state;
			}
		}
		mForceVoiceStateUpdate = false;
		mActiveVoiceFloater = current_voice_floater;
		mActiveVoiceUpdateTimer.setTimerExpirySec(VOICE_STATUS_UPDATE_INTERVAL);
	}

	LLMultiFloater::draw();
}

LLFloater* FSFloaterIMContainer::getCurrentVoiceFloater()
{
	if (!LLVoiceClient::instance().voiceEnabled())
	{
		return NULL;
	}

	if (LLVoiceChannelProximal::getInstance() == LLVoiceChannel::getCurrentVoiceChannel())
	{
		return FSFloaterNearbyChat::getInstance();
	}

	for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
	{
		FSFloaterIM* im_floater = dynamic_cast<FSFloaterIM*>(mTabContainer->getPanelByIndex(i));
		if (im_floater && im_floater->getVoiceChannel() == LLVoiceChannel::getCurrentVoiceChannel())
		{
			return im_floater;
		}
	}
	return NULL;
}

// EOF
