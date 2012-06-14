/**
 * @file llimconversation.cpp
 * @brief LLIMConversation class implements the common behavior of LNearbyChatBar
 * @brief and LLIMFloater for hosting both in LLIMContainer
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#include "llimconversation.h"

#include "lldraghandle.h"
#include "llfloaterreg.h"
#include "llimfloater.h"
#include "llimfloatercontainer.h" // to replace separate IM Floaters with multifloater container
#include "lllayoutstack.h"
#include "llnearbychat.h"

const F32 REFRESH_INTERVAL = 0.2f;

LLIMConversation::LLIMConversation(const LLUUID& session_id)
  : LLTransientDockableFloater(NULL, true, session_id)
  ,	LLEventTimer(REFRESH_INTERVAL)
  ,  mIsP2PChat(false)
  ,  mExpandCollapseBtn(NULL)
  ,  mTearOffBtn(NULL)
  ,  mCloseBtn(NULL)
  ,  mSessionID(session_id)
  , mParticipantList(NULL)
{
	mCommitCallbackRegistrar.add("IMSession.Menu.Action",
			boost::bind(&LLIMConversation::onIMSessionMenuItemClicked,  this, _2));
//	mCommitCallbackRegistrar.add("IMSession.ExpCollapseBtn.Click",
//			boost::bind(&LLIMConversation::onSlide,  this));
//	mCommitCallbackRegistrar.add("IMSession.CloseBtn.Click",
//			boost::bind(&LLFloater::onClickClose, this));
	mCommitCallbackRegistrar.add("IMSession.TearOffBtn.Click",
			boost::bind(&LLIMConversation::onTearOffClicked, this));
	mEnableCallbackRegistrar.add("IMSession.Menu.CompactExpandedModes.CheckItem",
			boost::bind(&LLIMConversation::onIMCompactExpandedMenuItemCheck, this, _2));
	mEnableCallbackRegistrar.add("IMSession.Menu.ShowModes.CheckItem",
			boost::bind(&LLIMConversation::onIMShowModesMenuItemCheck,   this, _2));
	mEnableCallbackRegistrar.add("IMSession.Menu.ShowModes.Enable",
			boost::bind(&LLIMConversation::onIMShowModesMenuItemEnable,  this, _2));
}

LLIMConversation::~LLIMConversation()
{
	if (mParticipantList)
	{
		delete mParticipantList;
		mParticipantList = NULL;
	}
}

BOOL LLIMConversation::postBuild()
{
	mCloseBtn = getChild<LLButton>("close_btn");
	mCloseBtn->setCommitCallback(boost::bind(&LLFloater::onClickClose, this));

	mExpandCollapseBtn = getChild<LLButton>("expand_collapse_btn");
	mExpandCollapseBtn->setClickedCallback(boost::bind(&LLIMConversation::onSlide, this));

	mParticipantListPanel = getChild<LLLayoutPanel>("speakers_list_panel");

	mTearOffBtn = getChild<LLButton>("tear_off_btn");
	mTearOffBtn->setCommitCallback(boost::bind(&LLIMConversation::onTearOffClicked, this));

	if (!getTornOff())
	{
		setOpenPositioning(LLFloaterEnums::POSITIONING_RELATIVE);
	}

	buildParticipantList();

	if (isChatMultiTab())
	{
		return LLFloater::postBuild();
	}
	else
	{
		return LLDockableFloater::postBuild();
	}

}

BOOL LLIMConversation::tick()
{
	// This check is needed until LLFloaterReg::removeInstance() is synchronized with deleting the floater
	// via LLMortician::updateClass(), to avoid calling dead instances. See LLFloater::destroy().
	if (isDead()) return false;

	// Need to resort the participant list if it's in sort by recent speaker order.
	if (mParticipantList)
	{
		mParticipantList->update();
	}

	return false;
}

void LLIMConversation::buildParticipantList()
{	if (mIsNearbyChat)
	{
	}
	else
	{
		// for group and ad-hoc chat we need to include agent into list
		if(!mIsP2PChat && !mParticipantList && mSessionID.notNull())
		{
			LLSpeakerMgr* speaker_manager = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			mParticipantList = new LLParticipantList(speaker_manager, getChild<LLAvatarList>("speakers_list"), true, false);
		}
	}
}

void LLIMConversation::onSortMenuItemClicked(const LLSD& userdata)
{
	// TODO: Check this code when sort order menu will be added. (EM)
	if (!mParticipantList)
	{
		return;
	}

	std::string chosen_item = userdata.asString();

	if (chosen_item == "sort_name")
	{
		mParticipantList->setSortOrder(LLParticipantList::E_SORT_BY_NAME);
	}

}

void LLIMConversation::onIMSessionMenuItemClicked(const LLSD& userdata)
{
	std::string item = userdata.asString();

	if (item == "compact_view" || item == "expanded_view")
	{
		gSavedSettings.setBOOL("PlainTextChatHistory", item == "compact_view");
	}
	else
	{
		bool prev_value = gSavedSettings.getBOOL(item);
		gSavedSettings.setBOOL(item, !prev_value);
	}

	LLIMConversation::processChatHistoryStyleUpdate();
}


bool LLIMConversation::onIMCompactExpandedMenuItemCheck(const LLSD& userdata)
{
	std::string item = userdata.asString();
	bool is_plain_text_mode = gSavedSettings.getBOOL("PlainTextChatHistory");

	return is_plain_text_mode? item == "compact_view" : item == "expanded_view";
}


bool LLIMConversation::onIMShowModesMenuItemCheck(const LLSD& userdata)
{
	return gSavedSettings.getBOOL(userdata.asString());
}

// enable/disable states for the "show time" and "show names" items of the show-modes menu
bool LLIMConversation::onIMShowModesMenuItemEnable(const LLSD& userdata)
{
	std::string item = userdata.asString();
	bool plain_text = gSavedSettings.getBOOL("PlainTextChatHistory");
	bool is_not_names = (item != "IMShowNamesForP2PConv");
	return (plain_text && (is_not_names || mIsP2PChat));
}

void LLIMConversation::updateHeaderAndToolbar()
{
	bool is_hosted = getHost() != NULL;

	if (is_hosted)
	{
		for (S32 i = 0; i < BUTTON_COUNT; i++)
		{
			if (mButtons[i])
			{
				// Hide the standard header buttons in a docked IM floater.
				mButtons[i]->setVisible(false);
			}
		}
	}

	// Participant list should be visible only in torn off floaters.
	bool is_participant_list_visible =
			!is_hosted
			&& gSavedSettings.getBOOL("IMShowControlPanel")
			&& !mIsP2PChat
			&& !mIsNearbyChat; // *TODO: temporarily disabled for Nearby chat

	mParticipantListPanel->setVisible(is_participant_list_visible);

	// Display collapse image (<<) if the floater is hosted
	// or if it is torn off but has an open control panel.
	bool is_expanded = is_hosted || is_participant_list_visible;
	mExpandCollapseBtn->setImageOverlay(getString(is_expanded ? "collapse_icon" : "expand_icon"));

	// The button (>>) should be disabled for torn off P2P conversations.
	mExpandCollapseBtn->setEnabled(is_hosted || !mIsP2PChat && !mIsNearbyChat);

	if (mDragHandle)
	{
		// toggle floater's drag handle and title visibility
		mDragHandle->setVisible(!is_hosted);
	}

	mTearOffBtn->setImageOverlay(getString(is_hosted ? "tear_off_icon" : "return_icon"));

	mCloseBtn->setVisible(is_hosted);

	enableDisableCallBtn();
}

// static
void LLIMConversation::processChatHistoryStyleUpdate()
{
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("impanel");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
			iter != inst_list.end(); ++iter)
	{
		LLIMFloater* floater = dynamic_cast<LLIMFloater*>(*iter);
		if (floater)
		{
			floater->reloadMessages();
		}
	}

	LLNearbyChat* nearby_chat = LLNearbyChat::getInstance();
	if (nearby_chat)
	{
		nearby_chat->reloadMessages();
	}
}

void LLIMConversation::updateCallBtnState(bool callIsActive)
{
	getChild<LLButton>("voice_call_btn")->setImageOverlay(
			callIsActive? getString("call_btn_stop") : getString("call_btn_start"));
    enableDisableCallBtn();

}

void LLIMConversation::onSlide(LLIMConversation* self)
{
	LLIMFloaterContainer* host_floater = dynamic_cast<LLIMFloaterContainer*>(self->getHost());
	if (host_floater)
	{
		// Hide the messages pane if a floater is hosted in the Conversations
		host_floater->collapseMessagesPane(true);
	}
	else ///< floater is torn off
	{
		if (!self->mIsP2PChat)
		{
			bool expand = !self->mParticipantListPanel->getVisible();

			// Expand/collapse the IM control panel
			self->mParticipantListPanel->setVisible(expand);

			gSavedSettings.setBOOL("IMShowControlPanel", expand);

			self->mExpandCollapseBtn->setImageOverlay(self->getString(expand ? "collapse_icon" : "expand_icon"));
		}
	}
}

/*virtual*/
void LLIMConversation::onOpen(const LLSD& key)
{
	LLIMFloaterContainer* host_floater = dynamic_cast<LLIMFloaterContainer*>(getHost());
	if (host_floater)
	{
		// Show the messages pane when opening a floater hosted in the Conversations
		host_floater->collapseMessagesPane(false);
	}

	updateHeaderAndToolbar();
}

void LLIMConversation::onTearOffClicked()
{
	onClickTearOff(this);
	updateHeaderAndToolbar();
}

// static
bool LLIMConversation::isChatMultiTab()
{
	// Restart is required in order to change chat window type.
	static bool is_single_window = gSavedSettings.getS32("ChatWindow") == 1;
	return is_single_window;
}
