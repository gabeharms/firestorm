/** 
 * @file LLNearbyChatHandler.cpp
 * @brief Nearby chat notification managment
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

#include "llnearbychathandler.h"

#include "llchatitemscontainerctrl.h"
#include "llnearbychat.h"
#include "llrecentpeople.h"

#include "llviewercontrol.h"

#include "llfloaterreg.h"//for LLFloaterReg::getTypedInstance
#include "llviewerwindow.h"//for screen channel position

//add LLNearbyChatHandler to LLNotificationsUI namespace
using namespace LLNotificationsUI;

//-----------------------------------------------------------------------------------------------
//LLNearbyChatScreenChannel
//-----------------------------------------------------------------------------------------------	
LLToastPanelBase* createToastPanel()
{
	LLNearbyChatToastPanel* item = LLNearbyChatToastPanel::createInstance();
	static S32 chat_item_width = 304;
	item->setWidth(chat_item_width);
	return item;
}


class LLNearbyChatScreenChannel: public LLScreenChannelBase
{
public:
	LLNearbyChatScreenChannel(const LLUUID& id):LLScreenChannelBase(id) { mStopProcessing = false;};

	void init				(S32 channel_left, S32 channel_right);

	void addNotification	(LLSD& notification);
	void arrangeToasts		();
	void showToastsBottom	();
	
	typedef boost::function<LLToastPanelBase* (void )> create_toast_panel_callback_t;
	void setCreatePanelCallback(create_toast_panel_callback_t value) { m_create_toast_panel_callback_t = value;}

	void onToastDestroyed	(LLToast* toast);
	void onToastFade		(LLToast* toast);

	void reshape			(S32 width, S32 height, BOOL called_from_parent);

	void redrawToasts()
	{
		arrangeToasts();
	}

	// hide all toasts from screen, but not remove them from a channel
	virtual void		hideToastsFromScreen() 
	{
	};
	// removes all toasts from a channel
	virtual void		removeToastsFromChannel() 
	{
		for(std::vector<LLToast*>::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
		{
			LLToast* toast = (*it);
			toast->setVisible(FALSE);
			toast->stopTimer();
			m_toast_pool.push_back(toast);

		}
		m_active_toasts.clear();
	};

	virtual void deleteAllChildren()
	{
		m_toast_pool.clear();
		m_active_toasts.clear();
		LLScreenChannelBase::deleteAllChildren();
	}

protected:
	void	createOverflowToast(S32 bottom, F32 timer);

	create_toast_panel_callback_t m_create_toast_panel_callback_t;

	bool	createPoolToast();
	
	std::vector<LLToast*> m_active_toasts;
	std::list<LLToast*> m_toast_pool;

	bool	mStopProcessing;
};

void LLNearbyChatScreenChannel::init(S32 channel_left, S32 channel_right)
{
	S32 channel_top = gViewerWindow->getWorldViewRect().getHeight();
	S32 channel_bottom = gViewerWindow->getWorldViewRect().mBottom;
	setRect(LLRect(channel_left, channel_top, channel_right, channel_bottom));
	setVisible(TRUE);
}


void	LLNearbyChatScreenChannel::createOverflowToast(S32 bottom, F32 timer)
{
	//we don't need overflow toast in nearby chat
}

void LLNearbyChatScreenChannel::onToastDestroyed(LLToast* toast)
{	
	mStopProcessing = true;
}

void LLNearbyChatScreenChannel::onToastFade(LLToast* toast)
{	
	//fade mean we put toast to toast pool
	if(!toast)
		return;
	m_toast_pool.push_back(toast);

	std::vector<LLToast*>::iterator pos = std::find(m_active_toasts.begin(),m_active_toasts.end(),toast);
	if(pos!=m_active_toasts.end())
		m_active_toasts.erase(pos);
	
	arrangeToasts();
}


bool	LLNearbyChatScreenChannel::createPoolToast()
{
	LLToastPanelBase* panel= m_create_toast_panel_callback_t();
	if(!panel)
		return false;
	
	LLToast::Params p;
	p.panel = panel;
	p.lifetime_secs = gSavedSettings.getS32("NearbyToastLifeTime");
	p.fading_time_secs = gSavedSettings.getS32("NearbyToastFadingTime");

	LLToast* toast = new LLToast(p); 
	
	
	toast->setOnFadeCallback(boost::bind(&LLNearbyChatScreenChannel::onToastFade, this, _1));
	toast->setOnToastDestroyedCallback(boost::bind(&LLNearbyChatScreenChannel::onToastDestroyed, this, _1));
	
	m_toast_pool.push_back(toast);
	return true;
}

void LLNearbyChatScreenChannel::addNotification(LLSD& notification)
{
	//look in pool. if there is any message
	if(mStopProcessing)
		return;
	
	if(m_toast_pool.empty())
	{
		//"pool" is empty - create one more panel
		if(!createPoolToast())//created toast will go to pool. so next call will find it
			return;
		addNotification(notification);
		return;
	}

	int chat_type = notification["chat_type"].asInteger();
	
	if( ((EChatType)chat_type == CHAT_TYPE_DEBUG_MSG))
	{
		if(gSavedSettings.getBOOL("ShowScriptErrors") == FALSE) 
			return;
		if(gSavedSettings.getS32("ShowScriptErrorsLocation")== 1)
			return;
	}
		

	//take 1st element from pool, (re)initialize it, put it in active toasts

	LLToast* toast = m_toast_pool.back();

	m_toast_pool.pop_back();


	LLToastPanelBase* panel = dynamic_cast<LLToastPanelBase*>(toast->getPanel());
	if(!panel)
		return;
	panel->init(notification);

	toast->reshapeToPanel();
	toast->resetTimer();
	
	m_active_toasts.insert(m_active_toasts.begin(),toast);

	arrangeToasts();
}

void LLNearbyChatScreenChannel::arrangeToasts()
{
	if(m_active_toasts.size() == 0 || mIsHovering)
		return;

	hideToastsFromScreen();

	showToastsBottom();					
}

void LLNearbyChatScreenChannel::showToastsBottom()
{
	if(mStopProcessing)
		return;

	LLRect	toast_rect;	
	S32		bottom = getRect().mBottom;
	S32		margin = gSavedSettings.getS32("ToastGap");

	for(std::vector<LLToast*>::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
	{
		LLToast* toast = (*it);
		S32 toast_top = bottom + toast->getRect().getHeight() + margin;

		if(toast_top > gFloaterView->getRect().getHeight())
		{
			while(it!=m_active_toasts.end())
			{
				toast->setVisible(FALSE);
				toast->stopTimer();
				m_toast_pool.push_back(toast);
				it=m_active_toasts.erase(it);
			}
			break;
		}
		else
		{
			toast_rect = toast->getRect();
			toast_rect.setLeftTopAndSize(getRect().mLeft , toast_top, toast_rect.getWidth() ,toast_rect.getHeight());
		
			toast->setRect(toast_rect);
			toast->setIsHidden(false);
			toast->setVisible(TRUE);
			
			bottom = toast->getRect().mTop;
		}		
	}
}

void LLNearbyChatScreenChannel::reshape			(S32 width, S32 height, BOOL called_from_parent)
{
	LLScreenChannelBase::reshape(width, height, called_from_parent);
	arrangeToasts();
}


//-----------------------------------------------------------------------------------------------
//LLNearbyChatHandler
//-----------------------------------------------------------------------------------------------
LLNearbyChatHandler::LLNearbyChatHandler(e_notification_type type, const LLSD& id)
{
	mType = type;

	// Getting a Channel for our notifications
	LLNearbyChatScreenChannel* channel = new LLNearbyChatScreenChannel(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
	
	LLNearbyChatScreenChannel::create_toast_panel_callback_t callback = createToastPanel;

	channel->setCreatePanelCallback(callback);

	mChannel = LLChannelManager::getInstance()->addChannel(channel);
	mChannel->setOverflowFormatString("You have %d unread nearby chat messages");
}

LLNearbyChatHandler::~LLNearbyChatHandler()
{
}


void LLNearbyChatHandler::initChannel()
{
	LLNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
	S32 channel_right_bound = nearby_chat->getRect().mRight;
	S32 channel_width = nearby_chat->getRect().mRight; 
	mChannel->init(channel_right_bound - channel_width, channel_right_bound);
}

void LLNearbyChatHandler::processChat(const LLChat& chat_msg)
{
	if(chat_msg.mMuted == TRUE)
		return;
	if(chat_msg.mSourceType == CHAT_SOURCE_AGENT && chat_msg.mFromID.notNull())
         LLRecentPeople::instance().add(chat_msg.mFromID);

	if(chat_msg.mText.empty())
		return;//don't process empty messages
	
	LLNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
	nearby_chat->addMessage(chat_msg);
	if(nearby_chat->getVisible())
		return;//no need in toast if chat is visible
	
	// arrange a channel on a screen
	if(!mChannel->getVisible())
	{
		initChannel();
	}

	//only messages from AGENTS
	if(CHAT_SOURCE_OBJECT == chat_msg.mSourceType)
	{
		return;//dn't show toast for messages from objects
	}

	LLUUID id;
	id.generate();

	LLNearbyChatScreenChannel* channel = dynamic_cast<LLNearbyChatScreenChannel*>(mChannel);
	

	if(channel)
	{
		LLSD notification;
		notification["id"] = id;
		notification["message"] = chat_msg.mText;
		notification["from"] = chat_msg.mFromName;
		notification["from_id"] = chat_msg.mFromID;
		notification["time"] = chat_msg.mTime;
		notification["source"] = (S32)chat_msg.mSourceType;
		notification["chat_type"] = (S32)chat_msg.mChatType;

		channel->addNotification(notification);	
	}
	
}

void LLNearbyChatHandler::onDeleteToast(LLToast* toast)
{
}



