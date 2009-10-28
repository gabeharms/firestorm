/** 
 * @file llscreenchannel.cpp
 * @brief Class implements a channel on a screen in which appropriate toasts may appear.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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


#include "llviewerprecompiledheaders.h" // must be first include

#include "lliconctrl.h"
#include "lltextbox.h"
#include "llscreenchannel.h"

#include "lltoastpanel.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llfloaterreg.h"
#include "lltrans.h"

#include "lldockablefloater.h"
#include "llimpanel.h"
#include "llsyswellwindow.h"
#include "llimfloater.h"

#include <algorithm>

using namespace LLNotificationsUI;

bool LLScreenChannel::mWasStartUpToastShown = false;

//--------------------------------------------------------------------------
//////////////////////
// LLScreenChannelBase
//////////////////////
LLScreenChannelBase::LLScreenChannelBase(const LLUUID& id) :
												mOverflowToastPanel(NULL) 
												,mToastAlignment(NA_BOTTOM)
												,mCanStoreToasts(true)
												,mHiddenToastsNum(0)
												,mOverflowToastHidden(false)
												,mIsHovering(false)
												,mControlHovering(false)
												,mShowToasts(true)
{	
	mID = id;
	mOverflowFormatString = LLTrans::getString("OverflowInfoChannelString");
	mWorldViewRectConnection = gViewerWindow->setOnWorldViewRectUpdated(boost::bind(&LLScreenChannelBase::updatePositionAndSize, this, _1, _2));
	setMouseOpaque( false );
	setVisible(FALSE);
}
LLScreenChannelBase::~LLScreenChannelBase()
{
	mWorldViewRectConnection.disconnect();
}
void LLScreenChannelBase::updatePositionAndSize(LLRect old_world_rect, LLRect new_world_rect)
{
	S32 top_delta = old_world_rect.mTop - new_world_rect.mTop;
	S32 right_delta = old_world_rect.mRight - new_world_rect.mRight;

	LLRect this_rect = getRect();

	this_rect.mTop -= top_delta;
	switch(mChannelAlignment)
	{
	case CA_LEFT :
		break;
	case CA_CENTRE :
		this_rect.setCenterAndSize(new_world_rect.getWidth() / 2, new_world_rect.getHeight() / 2, this_rect.getWidth(), this_rect.getHeight());
		break;
	case CA_RIGHT :
		this_rect.mLeft -= right_delta;
		this_rect.mRight -= right_delta;
	}
	setRect(this_rect);
	redrawToasts();
	
}

void LLScreenChannelBase::init(S32 channel_left, S32 channel_right)
{
	S32 channel_top = gViewerWindow->getWorldViewRect().getHeight();
	S32 channel_bottom = gViewerWindow->getWorldViewRect().mBottom + gSavedSettings.getS32("ChannelBottomPanelMargin");
	setRect(LLRect(channel_left, channel_top, channel_right, channel_bottom));
	setVisible(TRUE);
}

//--------------------------------------------------------------------------
//////////////////////
// LLScreenChannel
//////////////////////
//--------------------------------------------------------------------------
LLScreenChannel::LLScreenChannel(LLUUID& id):	LLScreenChannelBase(id)
{	
}

//--------------------------------------------------------------------------
void LLScreenChannel::init(S32 channel_left, S32 channel_right)
{
	LLScreenChannelBase::init(channel_left, channel_right);
}

//--------------------------------------------------------------------------
LLScreenChannel::~LLScreenChannel() 
{
	
}

//--------------------------------------------------------------------------
void LLScreenChannel::updatePositionAndSize(LLRect old_world_rect, LLRect new_world_rect)
{
	LLScreenChannelBase::updatePositionAndSize(old_world_rect, new_world_rect);
}

//--------------------------------------------------------------------------
void LLScreenChannel::addToast(const LLToast::Params& p)
{
	bool store_toast = false, show_toast = false;

	mDisplayToastsAlways ? show_toast = true : show_toast = mWasStartUpToastShown && (mShowToasts || p.force_show);
	store_toast = !show_toast && p.can_be_stored && mCanStoreToasts;

	if(!show_toast && !store_toast)
	{
		mRejectToastSignal(p.notif_id);
		return;
	}

	ToastElem new_toast_elem(p);

	mOverflowToastHidden = false;
	
	new_toast_elem.toast->setOnFadeCallback(boost::bind(&LLScreenChannel::onToastFade, this, _1));
	new_toast_elem.toast->setOnToastDestroyedCallback(boost::bind(&LLScreenChannel::onToastDestroyed, this, _1));
	if(mControlHovering)
	{
		new_toast_elem.toast->setOnToastHoverCallback(boost::bind(&LLScreenChannel::onToastHover, this, _1, _2));
	}
	
	if(show_toast)
	{
		mToastList.push_back(new_toast_elem);
		redrawToasts();
	}	
	else // store_toast
	{
		mHiddenToastsNum++;
		storeToast(new_toast_elem);
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::onToastDestroyed(LLToast* toast)
{	
	std::vector<ToastElem>::iterator it = find(mToastList.begin(), mToastList.end(), static_cast<LLPanel*>(toast));
		
	if(it != mToastList.end())
	{
		mToastList.erase(it);
	}
}


//--------------------------------------------------------------------------
void LLScreenChannel::onToastFade(LLToast* toast)
{	
	std::vector<ToastElem>::iterator it = find(mToastList.begin(), mToastList.end(), static_cast<LLPanel*>(toast));
		
	if(it != mToastList.end())
	{
		bool delete_toast = !mCanStoreToasts || !toast->getCanBeStored();
		if(delete_toast)
		{
			mToastList.erase(it);
			deleteToast(toast);
		}
		else
		{
			storeToast((*it));
			mToastList.erase(it);
		}	

		redrawToasts();
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::deleteToast(LLToast* toast)
{
	// send signal to observers about destroying of a toast
	toast->mOnDeleteToastSignal(toast);
	
	// update channel's Hovering state
	// turning hovering off manually because onMouseLeave won't happen if a toast was closed using a keyboard
	if(toast->hasFocus())
		setHovering(false);

	// close the toast
	toast->closeFloater();
}

//--------------------------------------------------------------------------

void LLScreenChannel::storeToast(ToastElem& toast_elem)
{
	// do not store clones
	std::vector<ToastElem>::iterator it = find(mStoredToastList.begin(), mStoredToastList.end(), toast_elem.id);
	if( it != mStoredToastList.end() )
		return;

	toast_elem.toast->stopTimer();
	mStoredToastList.push_back(toast_elem);
	mOnStoreToast(toast_elem.toast->getPanel(), toast_elem.id);
}

//--------------------------------------------------------------------------
void LLScreenChannel::loadStoredToastsToChannel()
{
	std::vector<ToastElem>::iterator it;

	if(mStoredToastList.size() == 0)
		return;
	
	mOverflowToastHidden = false;

	for(it = mStoredToastList.begin(); it != mStoredToastList.end(); ++it)
	{
		(*it).toast->setIsHidden(false);
		(*it).toast->resetTimer();
		mToastList.push_back((*it));
	}

	mStoredToastList.clear();
	redrawToasts();
}

//--------------------------------------------------------------------------
void LLScreenChannel::loadStoredToastByNotificationIDToChannel(LLUUID id)
{
	std::vector<ToastElem>::iterator it = find(mStoredToastList.begin(), mStoredToastList.end(), id);

	if( it == mStoredToastList.end() )
		return;

	mOverflowToastHidden = false;

	LLToast* toast = (*it).toast;
	toast->setIsHidden(false);
	toast->resetTimer();
	mToastList.push_back((*it));
	mStoredToastList.erase(it);

	redrawToasts();
}

//--------------------------------------------------------------------------
void LLScreenChannel::removeStoredToastByNotificationID(LLUUID id)
{
	// *TODO: may be remove this function
	std::vector<ToastElem>::iterator it = find(mStoredToastList.begin(), mStoredToastList.end(), id);

	if( it == mStoredToastList.end() )
		return;

	LLToast* toast = (*it).toast;
	mStoredToastList.erase(it);
	mRejectToastSignal(toast->getNotificationID());
}

//--------------------------------------------------------------------------
void LLScreenChannel::killToastByNotificationID(LLUUID id)
{
	// searching among toasts on a screen
	std::vector<ToastElem>::iterator it = find(mToastList.begin(), mToastList.end(), id);
	
	if( it != mToastList.end())
	{
		LLToast* toast = (*it).toast;
		// if it is a notification toast and notification is UnResponded - then respond on it
		// else - simply destroy a toast
		//
		// NOTE:	if a notification is unresponded this function will be called twice for the same toast.
		//			At first, the notification will be discarded, at second (it will be caused by discarding),
		//			the toast will be destroyed.
		if(toast->isNotificationValid())
		{
			mRejectToastSignal(toast->getNotificationID());
		}
		else
		{
			mToastList.erase(it);
			deleteToast(toast);
			redrawToasts();
		}
		return;
	}

	// searching among stored toasts
	it = find(mStoredToastList.begin(), mStoredToastList.end(), id);

	if( it != mStoredToastList.end() )
	{
		LLToast* toast = (*it).toast;
		mStoredToastList.erase(it);
		// send signal to a listener to let him perform some action on toast rejecting
		mRejectToastSignal(toast->getNotificationID());
		deleteToast(toast);
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::modifyToastByNotificationID(LLUUID id, LLPanel* panel)
{
	std::vector<ToastElem>::iterator it = find(mToastList.begin(), mToastList.end(), id);
	
	if( it != mToastList.end() && panel)
	{
		LLToast* toast = (*it).toast;
		LLPanel* old_panel = toast->getPanel();
		toast->removeChild(old_panel);
		delete old_panel;
		toast->insertPanel(panel);
		toast->resetTimer();
		redrawToasts();
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::redrawToasts()
{
	if(mToastList.size() == 0 || mIsHovering)
		return;

	hideToastsFromScreen();

	switch(mToastAlignment)
	{
	case NA_TOP : 
		showToastsTop();
		break;

	case NA_CENTRE :
		showToastsCentre();
		break;

	case NA_BOTTOM :
		showToastsBottom();					
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::showToastsBottom()
{
	LLRect	toast_rect;	
	S32		bottom = getRect().mBottom - gFloaterView->getRect().mBottom;
	S32		toast_margin = 0;
	std::vector<ToastElem>::reverse_iterator it;

	for(it = mToastList.rbegin(); it != mToastList.rend(); ++it)
	{
		if(it != mToastList.rbegin())
		{
			bottom = (*(it-1)).toast->getRect().mTop;
			toast_margin = gSavedSettings.getS32("ToastGap");
		}

		toast_rect = (*it).toast->getRect();
		toast_rect.setOriginAndSize(getRect().mLeft, bottom + toast_margin, toast_rect.getWidth() ,toast_rect.getHeight());
		(*it).toast->setRect(toast_rect);

		bool stop_showing_toasts = (*it).toast->getRect().mTop > getRect().mTop;

		if(!stop_showing_toasts)
		{
			if( it != mToastList.rend()-1)
			{
				stop_showing_toasts = ((*it).toast->getRect().mTop + gSavedSettings.getS32("OverflowToastHeight") + gSavedSettings.getS32("ToastGap")) > getRect().mTop;
			}
		} 

		if(stop_showing_toasts)
			break;

		(*it).toast->setVisible(TRUE);	
	}

	if(it != mToastList.rend() && !mOverflowToastHidden)
	{
		mHiddenToastsNum = 0;
		for(; it != mToastList.rend(); it++)
		{
			(*it).toast->stopTimer();
			mHiddenToastsNum++;
		}
		createOverflowToast(bottom, gSavedSettings.getS32("NotificationTipToastLifeTime"));
	}	
}

//--------------------------------------------------------------------------
void LLScreenChannel::showToastsCentre()
{
	LLRect	toast_rect;	
	S32		bottom = (getRect().mTop - getRect().mBottom)/2 + mToastList[0].toast->getRect().getHeight()/2;
	std::vector<ToastElem>::reverse_iterator it;

	for(it = mToastList.rbegin(); it != mToastList.rend(); ++it)
	{
		toast_rect = (*it).toast->getRect();
		toast_rect.setLeftTopAndSize(getRect().mLeft - toast_rect.getWidth() / 2, bottom + toast_rect.getHeight() / 2 + gSavedSettings.getS32("ToastGap"), toast_rect.getWidth() ,toast_rect.getHeight());
		(*it).toast->setRect(toast_rect);

		(*it).toast->setVisible(TRUE);	
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::showToastsTop()
{
}

//--------------------------------------------------------------------------
void LLScreenChannel::createOverflowToast(S32 bottom, F32 timer)
{
	LLRect toast_rect;
	LLToast::Params p;
	p.lifetime_secs = timer;
	mOverflowToastPanel = new LLToast(p);

	if(!mOverflowToastPanel)
		return;

	mOverflowToastPanel->setOnFadeCallback(boost::bind(&LLScreenChannel::closeOverflowToastPanel, this));

	LLTextBox* text_box = mOverflowToastPanel->getChild<LLTextBox>("toast_text");
	LLIconCtrl* icon = mOverflowToastPanel->getChild<LLIconCtrl>("icon");
	std::string	text = llformat(mOverflowFormatString.c_str(),mHiddenToastsNum);
	if(mHiddenToastsNum == 1)
	{
		text += ".";
	}
	else
	{
		text += "s.";
	}

	toast_rect = mOverflowToastPanel->getRect();
	mOverflowToastPanel->reshape(getRect().getWidth(), toast_rect.getHeight(), true);
	toast_rect.setLeftTopAndSize(getRect().mLeft, bottom + toast_rect.getHeight()+gSavedSettings.getS32("ToastGap"), getRect().getWidth(), toast_rect.getHeight());	
	mOverflowToastPanel->setRect(toast_rect);

	text_box->setValue(text);
	text_box->setVisible(TRUE);
	icon->setVisible(TRUE);

	mOverflowToastPanel->setVisible(TRUE);
}

//--------------------------------------------------------------------------
void LLScreenChannel::onOverflowToastHide()
{
	mOverflowToastHidden = true;

	// remove all hidden toasts from channel and save interactive notifications
	for(std::vector<ToastElem>::iterator it = mToastList.begin(); it != mToastList.end();)
	{
		if(!(*it).toast->getVisible())
		{
			if((*it).toast->getCanBeStored())
			{
				storeToast((*it));
			}
			else
			{
				deleteToast((*it).toast);
			}

			it = mToastList.erase(it);
		}
		else
		{
			++it;
		}
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::closeOverflowToastPanel()
{
	if(mOverflowToastPanel != NULL)
	{
		mOverflowToastPanel->closeFloater();
		mOverflowToastPanel = NULL;
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::createStartUpToast(S32 notif_num, F32 timer)
{
	LLRect toast_rect;
	LLToast::Params p;
	p.lifetime_secs = timer;
	p.enable_hide_btn = false;
	mStartUpToastPanel = new LLToast(p);

	if(!mStartUpToastPanel)
		return;

	mStartUpToastPanel->setOnFadeCallback(boost::bind(&LLScreenChannel::onStartUpToastHide, this));

	LLTextBox* text_box = mStartUpToastPanel->getChild<LLTextBox>("toast_text");
	LLIconCtrl* icon = mStartUpToastPanel->getChild<LLIconCtrl>("icon");

	std::string mStartUpFormatString;

	if(notif_num == 1)
	{
		mStartUpFormatString = LLTrans::getString("StartUpNotification");
	}
	else
	{
		mStartUpFormatString = LLTrans::getString("StartUpNotifications");
	}
	

	std::string	text = llformat(mStartUpFormatString.c_str(), notif_num);

	toast_rect = mStartUpToastPanel->getRect();
	mStartUpToastPanel->reshape(getRect().getWidth(), toast_rect.getHeight(), true);
	toast_rect.setLeftTopAndSize(0, toast_rect.getHeight()+gSavedSettings.getS32("ToastGap"), getRect().getWidth(), toast_rect.getHeight());	
	mStartUpToastPanel->setRect(toast_rect);

	text_box->setValue(text);
	text_box->setVisible(TRUE);
	icon->setVisible(TRUE);

	addChild(mStartUpToastPanel);
	
	mStartUpToastPanel->setVisible(TRUE);
}

//--------------------------------------------------------------------------
void LLScreenChannel::updateStartUpString(S32 num)
{
	// *TODO: update string if notifications are arriving while the StartUp toast is on a screen
}

//--------------------------------------------------------------------------
void LLScreenChannel::onStartUpToastHide()
{
	onCommit();
}

//--------------------------------------------------------------------------
void LLScreenChannel::closeStartUpToast()
{
	if(mStartUpToastPanel != NULL)
	{
		mStartUpToastPanel->setVisible(FALSE);
		mStartUpToastPanel = NULL;
	}
}

//--------------------------------------------------------------------------
void LLScreenChannel::hideToastsFromScreen()
{
	closeOverflowToastPanel();
	for(std::vector<ToastElem>::iterator it = mToastList.begin(); it != mToastList.end(); it++)
		(*it).toast->setVisible(FALSE);
}

//--------------------------------------------------------------------------
void LLScreenChannel::removeToastsFromChannel()
{
	hideToastsFromScreen();
	for(std::vector<ToastElem>::iterator it = mToastList.begin(); it != mToastList.end(); it++)
	{
		deleteToast((*it).toast);
	}
	mToastList.clear();
}

//--------------------------------------------------------------------------
void LLScreenChannel::removeAndStoreAllStorableToasts()
{
	if(mToastList.size() == 0)
		return;

	hideToastsFromScreen();
	for(std::vector<ToastElem>::iterator it = mToastList.begin(); it != mToastList.end();)
	{
		if((*it).toast->getCanBeStored())
		{
			storeToast(*(it));
			it = mToastList.erase(it);
		}
		else
		{
			++it;
		}
	}
	redrawToasts();
}

//--------------------------------------------------------------------------
void LLScreenChannel::removeToastsBySessionID(LLUUID id)
{
	if(mToastList.size() == 0)
		return;

	hideToastsFromScreen();
	for(std::vector<ToastElem>::iterator it = mToastList.begin(); it != mToastList.end();)
	{
		if((*it).toast->getSessionID() == id)
		{
			deleteToast((*it).toast);
			it = mToastList.erase(it);
		}
		else
		{
			++it;
		}
	}
	redrawToasts();
}

//--------------------------------------------------------------------------
void LLScreenChannel::onToastHover(LLToast* toast, bool mouse_enter)
{
	// because of LLViewerWindow::updateUI() that ALWAYS calls onMouseEnter BEFORE onMouseLeave
	// we must check this to prevent incorrect setting for hovering in a channel
	std::map<LLToast*, bool>::iterator it_first, it_second;
	S32 stack_size = mToastEventStack.size();
	mIsHovering = mouse_enter;

	switch(stack_size)
	{
	case 0:
		mToastEventStack.insert(std::pair<LLToast*, bool>(toast, mouse_enter));
		break;
	case 1:
		it_first = mToastEventStack.begin();
		if((*it_first).second && !mouse_enter && ((*it_first).first != toast) )
		{
			mToastEventStack.clear();
			mIsHovering = true;
		}
		else
		{
			mToastEventStack.clear();
			mToastEventStack.insert(std::pair<LLToast*, bool>(toast, mouse_enter));
		}
		break;
	default:
		LL_ERRS ("LLScreenChannel::onToastHover: stack size error " ) << stack_size << llendl;
	}

	if(!mIsHovering)
		redrawToasts();
}

//--------------------------------------------------------------------------
void LLScreenChannel::updateShowToastsState()
{
	LLFloater* floater = LLDockableFloater::getInstanceHandle().get();

	if(!floater)
	{
		setShowToasts(true);
		return;
	}

	// for IM floaters showed in a docked state - prohibit showing of ani toast
	if(dynamic_cast<LLIMFloater*>(floater))
	{
		setShowToasts(!(floater->getVisible() && floater->isDocked()));
		if (!getShowToasts())
		{
			removeAndStoreAllStorableToasts();
		}
	}

	// for Message Well floater showed in a docked state - adjust channel's height
	if(dynamic_cast<LLSysWellWindow*>(floater))
	{
		S32 channel_bottom = gViewerWindow->getWorldViewRect().mBottom + gSavedSettings.getS32("ChannelBottomPanelMargin");;
		LLRect this_rect = getRect();
		if(floater->getVisible() && floater->isDocked())
		{
			channel_bottom += (floater->getRect().getHeight() + gSavedSettings.getS32("ToastGap"));
		}

		if(channel_bottom != this_rect.mBottom)
		{
			setRect(LLRect(this_rect.mLeft, this_rect.mTop, this_rect.mRight, channel_bottom));
		}
	}
}

//--------------------------------------------------------------------------

