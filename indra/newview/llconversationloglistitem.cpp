/**
 * @file llconversationloglistitem.cpp
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

// llui
#include "lliconctrl.h"
#include "lltextbox.h"
#include "lltextutil.h"

// newview
#include "llavatariconctrl.h"
#include "llconversationlog.h"
#include "llconversationloglistitem.h"
#include "llgroupiconctrl.h"
#include "llinventoryicon.h"

LLConversationLogListItem::LLConversationLogListItem(const LLConversation* conversation)
:	LLPanel(),
	mConversation(conversation),
	mConversationName(NULL),
	mConversationDate(NULL)
{
	buildFromFile("panel_conversation_log_list_item.xml");

	LLIMFloater* floater = LLIMFloater::findInstance(mConversation->getSessionID());

	bool has_offline_ims = !mConversation->isVoice() && mConversation->hasOfflineMessages();
	bool ims_are_read = LLIMFloater::isVisible(floater) && floater->hasFocus();

	if (has_offline_ims && !ims_are_read)
	{
		mIMFloaterShowedConnection = LLIMFloater::setIMFloaterShowedCallback(boost::bind(&LLConversationLogListItem::onIMFloaterShown, this, _1));
	}
}

LLConversationLogListItem::~LLConversationLogListItem()
{
	mIMFloaterShowedConnection.disconnect();
}

BOOL LLConversationLogListItem::postBuild()
{
	initIcons();

	// set conversation name
	mConversationName = getChild<LLTextBox>("conversation_name");
	mConversationName->setValue(mConversation->getConversationName());

	// set conversation date and time
	mConversationDate = getChild<LLTextBox>("date_time");
	mConversationDate->setValue(mConversation->getTimestamp());

	getChild<LLButton>("delete_btn")->setClickedCallback(boost::bind(&LLConversationLogListItem::onRemoveBtnClicked, this));

	return TRUE;
}

void LLConversationLogListItem::initIcons()
{
	switch (mConversation->getConversationType())
	{
		case LLIMModel::LLIMSession::P2P_SESSION:
		case LLIMModel::LLIMSession::ADHOC_SESSION:
		{
			LLAvatarIconCtrl* avatar_icon = getChild<LLAvatarIconCtrl>("avatar_icon");
			avatar_icon->setVisible(TRUE);
			avatar_icon->setValue(mConversation->getParticipantID());
			break;
		}
		case LLIMModel::LLIMSession::GROUP_SESSION:
		{
			LLGroupIconCtrl* group_icon = getChild<LLGroupIconCtrl>("group_icon");
			group_icon->setVisible(TRUE);
			group_icon->setValue(mConversation->getSessionID());
			break;
		}
		default:
			break;
	}

	if (mConversation->isVoice())
	{
		getChild<LLIconCtrl>("voice_session_icon")->setVisible(TRUE);
	}
	else
	{
		if (mConversation->hasOfflineMessages())
		{
			getChild<LLIconCtrl>("unread_ims_icon")->setVisible(TRUE);
		}
	}
}

void LLConversationLogListItem::onMouseEnter(S32 x, S32 y, MASK mask)
{
	getChildView("hovered_icon")->setVisible(true);
	LLPanel::onMouseEnter(x, y, mask);
}

void LLConversationLogListItem::onMouseLeave(S32 x, S32 y, MASK mask)
{
	getChildView("hovered_icon")->setVisible(false);
	LLPanel::onMouseLeave(x, y, mask);
}

void LLConversationLogListItem::setValue(const LLSD& value)
{
	if (!value.isMap() || !value.has("selected"))
	{
		return;
	}

	getChildView("selected_icon")->setVisible(value["selected"]);
}

void LLConversationLogListItem::onIMFloaterShown(const LLUUID& session_id)
{
	if (mConversation->getSessionID() == session_id)
	{
		getChild<LLIconCtrl>("unread_ims_icon")->setVisible(FALSE);
	}
}

void LLConversationLogListItem::onRemoveBtnClicked()
{
	LLConversationLog::instance().removeConversation(*mConversation);
}

void LLConversationLogListItem::highlightNameDate(const std::string& highlited_text)
{
	LLStyle::Params params;
	LLTextUtil::textboxSetHighlightedVal(mConversationName, params, mConversation->getConversationName(), highlited_text);
	LLTextUtil::textboxSetHighlightedVal(mConversationDate, params, mConversation->getTimestamp(), highlited_text);
}
