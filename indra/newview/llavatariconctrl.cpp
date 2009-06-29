/** 
 * @file llavatariconctrl.cpp
 * @brief LLAvatarIconCtrl class implementation
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

#include "llagent.h"
#include "llavatarconstants.h"
#include "llavatariconctrl.h"
#include "llcallingcard.h" // for LLAvatarTracker
#include "llfriendactions.h"
#include "llimview.h"
#include "llmenugl.h"
#include "lluictrlfactory.h"

#include "llcachename.h"

#define MENU_ITEM_VIEW_PROFILE 0
#define MENU_ITEM_SEND_IM 1

static LLDefaultWidgetRegistry::Register<LLAvatarIconCtrl> r("avatar_icon");

LLAvatarIconCtrl::LLAvatarIconCtrl(const LLAvatarIconCtrl::Params& p)
:	LLIconCtrl(p),
	mDrawTooltip(p.draw_tooltip)
{
	LLRect rect = p.rect;

	static LLUICachedControl<S32> llavatariconctrl_symbol_hpad("UIAvatariconctrlSymbolHPad", 2);
	static LLUICachedControl<S32> llavatariconctrl_symbol_vpad("UIAvatariconctrlSymbolVPad", 2);
	static LLUICachedControl<S32> llavatariconctrl_symbol_size("UIAvatariconctrlSymbolSize", 5);
	static LLUICachedControl<std::string> llavatariconctrl_symbol_pos("UIAvatariconctrlSymbolPosition", "BottomRight");

	// BottomRight is the default position
	S32 left = rect.getWidth() - llavatariconctrl_symbol_size - llavatariconctrl_symbol_hpad;
	S32 bottom = llavatariconctrl_symbol_vpad;

	if ("BottomLeft" == (std::string)llavatariconctrl_symbol_pos)
	{
		left = llavatariconctrl_symbol_hpad;
		bottom = llavatariconctrl_symbol_vpad;
	}
	else if ("TopLeft" == (std::string)llavatariconctrl_symbol_pos)
	{
		left = llavatariconctrl_symbol_hpad;
		bottom = rect.getHeight() - llavatariconctrl_symbol_size - llavatariconctrl_symbol_vpad;
	}
	else if ("TopRight" == (std::string)llavatariconctrl_symbol_pos)
	{
		left = rect.getWidth() - llavatariconctrl_symbol_size - llavatariconctrl_symbol_hpad;
		bottom = rect.getHeight() - llavatariconctrl_symbol_size - llavatariconctrl_symbol_vpad;
	}

	rect.setOriginAndSize(left, bottom, llavatariconctrl_symbol_size, llavatariconctrl_symbol_size);

	LLIconCtrl::Params icparams;
	icparams.name ("Status Symbol");
	icparams.follows.flags (FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	icparams.rect (rect);
	mStatusSymbol = LLUICtrlFactory::create<LLIconCtrl> (icparams);
	mStatusSymbol->setValue("circle.tga");
	mStatusSymbol->setColor(LLColor4::grey);

	addChild(mStatusSymbol);
	
	if (p.avatar_id.isProvided())
	{
		LLSD value(p.avatar_id);
		setValue(value);
	}
	else
	{
		LLIconCtrl::setValue("default_profile_picture.j2c");
	}


	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;

	registrar.add("AvatarIcon.Action", boost::bind(&LLAvatarIconCtrl::onAvatarIconContextMenuItemClicked, this, _2));

	LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_avatar_icon.xml", gMenuHolder);

	mPopupMenuHandle = menu->getHandle();
}

LLAvatarIconCtrl::~LLAvatarIconCtrl()
{
	if (mAvatarId.notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarId, this);
		// Name callbacks will be automatically disconnected since LLUICtrl is trackable
	}

	LLView::deleteViewByHandle(mPopupMenuHandle);
}

//virtual
void LLAvatarIconCtrl::setValue(const LLSD& value)
{
	if (value.isUUID())
	{
		if (mAvatarId.notNull())
		{
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarId, this);
		}

		if (mAvatarId != value.asUUID())
		{
			LLAvatarPropertiesProcessor::getInstance()->addObserver(value.asUUID(), this);
			LLAvatarPropertiesProcessor::getInstance()->sendDataRequest(value.asUUID(),APT_PROPERTIES);
			mAvatarId = value.asUUID();
		}
	}
	else
	{
		LLIconCtrl::setValue(value);
	}

	gCacheName->get(mAvatarId, FALSE, boost::bind(&LLAvatarIconCtrl::nameUpdatedCallback, this, _1, _2, _3, _4));
}

//virtual
void LLAvatarIconCtrl::processProperties(void* data, EAvatarProcessorType type)
{
	if (APT_PROPERTIES == type)
	{
		LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
		if (avatar_data)
		{
			if (avatar_data->avatar_id != mAvatarId)
			{
				return;
			}

			// Update the avatar
			if (avatar_data->image_id.notNull())
			{
				LLIconCtrl::setValue(avatar_data->image_id);
			}
			else
			{
				LLIconCtrl::setValue("default_profile_picture.j2c");
			}

			// Update color of status symbol and tool tip
			if (avatar_data->flags & AVATAR_ONLINE)
			{
				mStatusSymbol->setColor(LLColor4::green);
				if (mDrawTooltip)
				{
					setToolTip((LLStringExplicit)"Online");
				}
			}
			else
			{
				mStatusSymbol->setColor(LLColor4::grey);
				if (mDrawTooltip)
				{
					setToolTip((LLStringExplicit)"Offline");
				}
			}
		}
	}
}

BOOL LLAvatarIconCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();

	if(menu)
	{
		bool is_friend = LLAvatarTracker::instance().getBuddyInfo(mAvatarId) != NULL;

		menu->setItemEnabled("Add Friend", !is_friend);
		menu->setItemEnabled("Remove Friend", is_friend);

		menu->buildDrawLabels();
		menu->updateParent(LLMenuGL::sMenuContainer);
		LLMenuGL::showPopup(this, menu, x, y);
	}

	return TRUE;
}

void LLAvatarIconCtrl::nameUpdatedCallback(
	const LLUUID& id,
	const std::string& first,
	const std::string& last,
	BOOL is_group)
{
	if (id == mAvatarId)
	{
		mFirstName = first;
		mLastName = last;
	}
}

void LLAvatarIconCtrl::onAvatarIconContextMenuItemClicked(const LLSD& userdata)
{
	std::string level = userdata.asString();
	LLUUID id = getAvatarId();

	if (level == "profile")
	{
		LLFriendActions::showProfile(id);
	}
	else if (level == "im")
	{
		std::string name;
		name.assign(getFirstName());
		name.append(" ");
		name.append(getLastName());

		gIMMgr->addSession(name, IM_NOTHING_SPECIAL, id);
	}
	else if (level == "add")
	{
		std::string name;
		name.assign(getFirstName());
		name.append(" ");
		name.append(getLastName());

		LLFriendActions::requestFriendshipDialog(id, name);
	}
	else if (level == "remove")
	{
		LLFriendActions::removeFriendDialog(id);
	}
}
