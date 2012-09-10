/** 
 * @file LLNearbyChatHandler.cpp
 * @brief Nearby chat chat managment
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

#include "llviewerprecompiledheaders.h"

#include "llagentdata.h" // for gAgentID
#include "llnearbychathandler.h"

#include "llchatitemscontainerctrl.h"
#include "llfirstuse.h"
#include "llfloaterscriptdebug.h"
#include "llhints.h"
#include "llnearbychat.h"
#include "llrecentpeople.h"

#include "llviewercontrol.h"

#include "llfloaterreg.h"//for LLFloaterReg::getTypedInstance
#include "llviewerwindow.h"//for screen channel position
#include "llnearbychat.h"
#include "llrootview.h"
#include "lllayoutstack.h"

//add LLNearbyChatHandler to LLNotificationsUI namespace
using namespace LLNotificationsUI;

static LLNearbyChatToastPanel* createToastPanel()
{
	LLNearbyChatToastPanel* item = LLNearbyChatToastPanel::createInstance();
	return item;
}


//-----------------------------------------------------------------------------------------------
//LLNearbyChatScreenChannel
//-----------------------------------------------------------------------------------------------	

class LLNearbyChatScreenChannel: public LLScreenChannelBase
{
	LOG_CLASS(LLNearbyChatScreenChannel);
public:
	typedef std::vector<LLHandle<LLToast> > toast_vec_t;
	typedef std::list<LLHandle<LLToast> > toast_list_t;

	LLNearbyChatScreenChannel(const Params& p)
		: LLScreenChannelBase(p)
	{
		mStopProcessing = false;

		LLControlVariable* ctrl = gSavedSettings.getControl("NearbyToastLifeTime").get();
		if (ctrl)
		{
			ctrl->getSignal()->connect(boost::bind(&LLNearbyChatScreenChannel::updateToastsLifetime, this));
		}

		ctrl = gSavedSettings.getControl("NearbyToastFadingTime").get();
		if (ctrl)
		{
			ctrl->getSignal()->connect(boost::bind(&LLNearbyChatScreenChannel::updateToastFadingTime, this));
		}
	}

	void addChat	(LLSD& chat);
	void arrangeToasts		();
	
	typedef boost::function<LLNearbyChatToastPanel* (void )> create_toast_panel_callback_t;
	void setCreatePanelCallback(create_toast_panel_callback_t value) { m_create_toast_panel_callback_t = value;}

	void onToastDestroyed	(LLToast* toast, bool app_quitting);
	void onToastFade		(LLToast* toast);

	void redrawToasts()
	{
		arrangeToasts();
	}

	// hide all toasts from screen, but not remove them from a channel
	// removes all toasts from a channel
	virtual void		removeToastsFromChannel() 
	{
		for(toast_vec_t::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
		{
			addToToastPool(it->get());
		}
		m_active_toasts.clear();
	};

	virtual void deleteAllChildren()
	{
		LL_DEBUGS("NearbyChat") << "Clearing toast pool" << llendl;
		m_toast_pool.clear();
		m_active_toasts.clear();
		LLScreenChannelBase::deleteAllChildren();
	}

protected:
	void	deactivateToast(LLToast* toast);
	void	addToToastPool(LLToast* toast)
	{
		if (!toast) return;
		LL_DEBUGS("NearbyChat") << "Pooling toast" << llendl;
		toast->setVisible(FALSE);
		toast->stopTimer();
		toast->setIsHidden(true);

		// Nearby chat toasts that are hidden, not destroyed. They are collected to the toast pool, so that
		// they can be used next time, this is done for performance. But if the toast lifetime was changed
		// (from preferences floater (STORY-36)) while it was shown (at this moment toast isn't in the pool yet)
		// changes don't take affect.
		// So toast's lifetime should be updated each time it's added to the pool. Otherwise viewer would have
		// to be restarted so that changes take effect.
		toast->setLifetime(gSavedSettings.getS32("NearbyToastLifeTime"));
		toast->setFadingTime(gSavedSettings.getS32("NearbyToastFadingTime"));
		m_toast_pool.push_back(toast->getHandle());
	}

	void	createOverflowToast(S32 bottom, F32 timer);

	void 	updateToastsLifetime();

	void	updateToastFadingTime();

	create_toast_panel_callback_t m_create_toast_panel_callback_t;

	bool	createPoolToast();
	
	toast_vec_t m_active_toasts;
	toast_list_t m_toast_pool;

	bool	mStopProcessing;
	bool	mChannelRect;
};



//-----------------------------------------------------------------------------------------------
// LLNearbyChatToast
//-----------------------------------------------------------------------------------------------

// We're deriving from LLToast to be able to override onClose()
// in order to handle closing nearby chat toasts properly.
class LLNearbyChatToast : public LLToast
{
	LOG_CLASS(LLNearbyChatToast);
public:
	LLNearbyChatToast(const LLToast::Params& p, LLNearbyChatScreenChannel* nc_channelp)
	:	LLToast(p),
	 	mNearbyChatScreenChannelp(nc_channelp)
	{
	}

	/*virtual*/ void onClose(bool app_quitting);

private:
	LLNearbyChatScreenChannel*	mNearbyChatScreenChannelp;
};

//-----------------------------------------------------------------------------------------------
// LLNearbyChatScreenChannel
//-----------------------------------------------------------------------------------------------

void LLNearbyChatScreenChannel::deactivateToast(LLToast* toast)
{
	toast_vec_t::iterator pos = std::find(m_active_toasts.begin(), m_active_toasts.end(), toast->getHandle());

	if (pos == m_active_toasts.end())
	{
		llassert(pos == m_active_toasts.end());
		return;
	}

	LL_DEBUGS("NearbyChat") << "Deactivating toast" << llendl;
	m_active_toasts.erase(pos);
}

void	LLNearbyChatScreenChannel::createOverflowToast(S32 bottom, F32 timer)
{
	//we don't need overflow toast in nearby chat
}

void LLNearbyChatScreenChannel::onToastDestroyed(LLToast* toast, bool app_quitting)
{	
	LL_DEBUGS("NearbyChat") << "Toast destroyed (app_quitting=" << app_quitting << ")" << llendl;

	if (app_quitting)
	{
		// Viewer is quitting.
		// Immediately stop processing chat messages (EXT-1419).
	mStopProcessing = true;
}
	else
	{
		// The toast is being closed by user (STORM-192).
		// Remove it from the list of active toasts to prevent
		// further references to the invalid pointer.
		deactivateToast(toast);
	}
}

void LLNearbyChatScreenChannel::onToastFade(LLToast* toast)
{	
	LL_DEBUGS("NearbyChat") << "Toast fading" << llendl;

	//fade mean we put toast to toast pool
	if(!toast)
		return;

	deactivateToast(toast);

	addToToastPool(toast);
	
	arrangeToasts();
}

void LLNearbyChatScreenChannel::updateToastsLifetime()
{
	S32 seconds = gSavedSettings.getS32("NearbyToastLifeTime");
	toast_list_t::iterator it;

	for(it = m_toast_pool.begin(); it != m_toast_pool.end(); ++it)
	{
		(*it).get()->setLifetime(seconds);
	}
}

void LLNearbyChatScreenChannel::updateToastFadingTime()
{
	S32 seconds = gSavedSettings.getS32("NearbyToastFadingTime");
	toast_list_t::iterator it;

	for(it = m_toast_pool.begin(); it != m_toast_pool.end(); ++it)
	{
		(*it).get()->setFadingTime(seconds);
	}
}

bool	LLNearbyChatScreenChannel::createPoolToast()
{
	LLNearbyChatToastPanel* panel= m_create_toast_panel_callback_t();
	if(!panel)
		return false;
	
	LLToast::Params p;
	p.panel = panel;
	p.lifetime_secs = gSavedSettings.getS32("NearbyToastLifeTime");
	p.fading_time_secs = gSavedSettings.getS32("NearbyToastFadingTime");

	LLToast* toast = new LLNearbyChatToast(p, this);
	
	
	toast->setOnFadeCallback(boost::bind(&LLNearbyChatScreenChannel::onToastFade, this, _1));

	// If the toast gets somehow prematurely destroyed, deactivate it to prevent crash (STORM-1352).
	toast->setOnToastDestroyedCallback(boost::bind(&LLNearbyChatScreenChannel::onToastDestroyed, this, _1, false));

	LL_DEBUGS("NearbyChat") << "Creating and pooling toast" << llendl;	
	m_toast_pool.push_back(toast->getHandle());
	return true;
}

void LLNearbyChatScreenChannel::addChat(LLSD& chat)
{
	//look in pool. if there is any message
	if(mStopProcessing)
		return;

	/*
    find last toast and check ID
	*/

	if(m_active_toasts.size())
	{
		LLUUID fromID = chat["from_id"].asUUID();		// agent id or object id
		std::string from = chat["from"].asString();
		LLToast* toast = m_active_toasts[0].get();
		if (toast)
		{
			LLNearbyChatToastPanel* panel = dynamic_cast<LLNearbyChatToastPanel*>(toast->getPanel());
  
			if(panel && panel->messageID() == fromID && panel->getFromName() == from && panel->canAddText())
			{
				panel->addMessage(chat);
				toast->reshapeToPanel();
				toast->startTimer();
	  
				arrangeToasts();
				return;
			}
		}
	}
	

	
	if(m_toast_pool.empty())
	{
		//"pool" is empty - create one more panel
		LL_DEBUGS("NearbyChat") << "Empty pool" << llendl;
		if(!createPoolToast())//created toast will go to pool. so next call will find it
			return;
		addChat(chat);
		return;
	}

	int chat_type = chat["chat_type"].asInteger();
	
	if( ((EChatType)chat_type == CHAT_TYPE_DEBUG_MSG))
	{
		if(gSavedSettings.getBOOL("ShowScriptErrors") == FALSE) 
			return;
		if(gSavedSettings.getS32("ShowScriptErrorsLocation")== 1)
			return;
	}
		

	//take 1st element from pool, (re)initialize it, put it in active toasts

	LL_DEBUGS("NearbyChat") << "Getting toast from pool" << llendl;
	LLToast* toast = m_toast_pool.back().get();

	m_toast_pool.pop_back();


	LLNearbyChatToastPanel* panel = dynamic_cast<LLNearbyChatToastPanel*>(toast->getPanel());
	if(!panel)
		return;
	panel->init(chat);

	toast->reshapeToPanel();
	toast->startTimer();
	
	m_active_toasts.push_back(toast->getHandle());

	arrangeToasts();
}

static bool sort_toasts_predicate(LLHandle<LLToast> first, LLHandle<LLToast> second)
{
	if (!first.get() || !second.get()) return false; // STORM-1352

	F32 v1 = first.get()->getTimeLeftToLive();
	F32 v2 = second.get()->getTimeLeftToLive();
	return v1 > v2;
}

void LLNearbyChatScreenChannel::arrangeToasts()
{
	if(mStopProcessing || isHovering())
		return;

	if (mFloaterSnapRegion == NULL)
	{
		mFloaterSnapRegion = gViewerWindow->getRootView()->getChildView("floater_snap_region");
	}
	
	if (!getParent())
	{
		// connect to floater snap region just to get resize events, we don't care about being a proper widget 
		mFloaterSnapRegion->addChild(this);
		setFollows(FOLLOWS_ALL);
	}

	LLRect	toast_rect;	
	updateRect();

	LLRect channel_rect;
	mFloaterSnapRegion->localRectToOtherView(mFloaterSnapRegion->getLocalRect(), &channel_rect, gFloaterView);
	channel_rect.mLeft += 10;
	channel_rect.mRight = channel_rect.mLeft + 300;

	S32 channel_bottom = channel_rect.mBottom;

	S32		bottom = channel_bottom + 80;
	S32		margin = gSavedSettings.getS32("ToastGap");

	//sort active toasts
	std::sort(m_active_toasts.begin(),m_active_toasts.end(),sort_toasts_predicate);

	//calc max visible item and hide other toasts.

	for(toast_vec_t::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
	{
		LLToast* toast = it->get();
		if (!toast)
		{
			llwarns << "NULL found in the active chat toasts list!" << llendl;
			continue;
		}

		S32 toast_top = bottom + toast->getRect().getHeight() + margin;

		if(toast_top > channel_rect.getHeight())
		{
			while(it!=m_active_toasts.end())
			{
				addToToastPool(it->get());
				it=m_active_toasts.erase(it);
			}
			break;
		}

		toast_rect = toast->getRect();
		toast_rect.setLeftTopAndSize(channel_rect.mLeft , bottom + toast_rect.getHeight(), toast_rect.getWidth() ,toast_rect.getHeight());

		toast->setRect(toast_rect);
		bottom += toast_rect.getHeight() - toast->getTopPad() + margin;
	}
	
	// use reverse order to provide correct z-order and avoid toast blinking
	
	for(toast_vec_t::reverse_iterator it = m_active_toasts.rbegin(); it != m_active_toasts.rend(); ++it)
	{
		LLToast* toast = it->get();
		if (toast)
	{
		toast->setIsHidden(false);
		toast->setVisible(TRUE);
		}
	}

}



//-----------------------------------------------------------------------------------------------
//LLNearbyChatHandler
//-----------------------------------------------------------------------------------------------
boost::scoped_ptr<LLEventPump> LLNearbyChatHandler::sChatWatcher(new LLEventStream("LLChat"));

LLNearbyChatHandler::LLNearbyChatHandler()
{
	// Getting a Channel for our notifications
	LLNearbyChatScreenChannel::Params p;
	p.id = LLUUID(gSavedSettings.getString("NearByChatChannelUUID"));
	LLNearbyChatScreenChannel* channel = new LLNearbyChatScreenChannel(p);
	
	LLNearbyChatScreenChannel::create_toast_panel_callback_t callback = createToastPanel;

	channel->setCreatePanelCallback(callback);

	LLChannelManager::getInstance()->addChannel(channel);

	mChannel = channel->getHandle();
}

LLNearbyChatHandler::~LLNearbyChatHandler()
{
}


void LLNearbyChatHandler::initChannel()
{
	//LLRect snap_rect = gFloaterView->getSnapRect();
	//mChannel->init(snap_rect.mLeft, snap_rect.mLeft + 200);
}



void LLNearbyChatHandler::processChat(const LLChat& chat_msg,
									  const LLSD &args)
{
	if(chat_msg.mMuted == TRUE)
		return;

	if(chat_msg.mText.empty())
		return;//don't process empty messages

	LLNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat");

	// Build notification data 
	LLSD chat;
	chat["message"] = chat_msg.mText;
	chat["from"] = chat_msg.mFromName;
	chat["from_id"] = chat_msg.mFromID;
	chat["time"] = chat_msg.mTime;
	chat["source"] = (S32)chat_msg.mSourceType;
	chat["chat_type"] = (S32)chat_msg.mChatType;
	chat["chat_style"] = (S32)chat_msg.mChatStyle;
	// Pass sender info so that it can be rendered properly (STORM-1021).
	chat["sender_slurl"] = LLViewerChat::getSenderSLURL(chat_msg, args);

	if (chat_msg.mChatType == CHAT_TYPE_DIRECT &&
		chat_msg.mText.length() > 0 &&
		chat_msg.mText[0] == '@')
	{
		// Send event on to LLEventStream and exit
		sChatWatcher->post(chat);
		return;
	}

	// don't show toast and add message to chat history on receive debug message
	// with disabled setting showing script errors or enabled setting to show script
	// errors in separate window.
	if (chat_msg.mChatType == CHAT_TYPE_DEBUG_MSG)
	{
		if(gSavedSettings.getBOOL("ShowScriptErrors") == FALSE)
			return;

		// don't process debug messages from not owned objects, see EXT-7762
		if (gAgentID != chat_msg.mOwnerID)
		{
			return;
		}

		if (gSavedSettings.getS32("ShowScriptErrorsLocation")== 1)// show error in window //("ScriptErrorsAsChat"))
		{

			LLColor4 txt_color;

			LLViewerChat::getChatColor(chat_msg,txt_color);

			LLFloaterScriptDebug::addScriptLine(chat_msg.mText,
												chat_msg.mFromName,
												txt_color,
												chat_msg.mFromID);
			return;
		}
	}

	nearby_chat->addMessage(chat_msg, true, args);

	if(chat_msg.mSourceType == CHAT_SOURCE_AGENT 
		&& chat_msg.mFromID.notNull() 
		&& chat_msg.mFromID != gAgentID)
	{
 		LLFirstUse::otherAvatarChatFirst();

 		// Add sender to the recent people list.
 		LLRecentPeople::instance().add(chat_msg.mFromID);

	}

	// Send event on to LLEventStream
	sChatWatcher->post(chat);

	if( nearby_chat->isInVisibleChain()
		|| ( chat_msg.mSourceType == CHAT_SOURCE_AGENT
			&& gSavedSettings.getBOOL("UseChatBubbles") )
		|| mChannel.isDead()
		|| !mChannel.get()->getShowToasts() ) // to prevent toasts in Busy mode
		return;//no need in toast if chat is visible or if bubble chat is enabled

	// arrange a channel on a screen
	if(!mChannel.get()->getVisible())
	{
		initChannel();
	}

	/*
	//comment all this due to EXT-4432
	..may clean up after some time...

	//only messages from AGENTS
	if(CHAT_SOURCE_OBJECT == chat_msg.mSourceType)
	{
		if(chat_msg.mChatType == CHAT_TYPE_DEBUG_MSG)
			return;//ok for now we don't skip messeges from object, so skip only debug messages
	}
	*/

	LLNearbyChatScreenChannel* channel = dynamic_cast<LLNearbyChatScreenChannel*>(mChannel.get());

	if(channel)
	{
		// Handle IRC styled messages.
		std::string toast_msg;
		if (chat_msg.mChatStyle == CHAT_STYLE_IRC)
		{
			if (!chat_msg.mFromName.empty())
			{
				toast_msg += chat_msg.mFromName;
			}
			toast_msg += chat_msg.mText.substr(3);
		}
		else
		{
			toast_msg = chat_msg.mText;
		}

		// Add a nearby chat toast.
		LLUUID id;
		id.generate();
		chat["id"] = id;
		std::string r_color_name = "White";
		F32 r_color_alpha = 1.0f; 
		LLViewerChat::getChatColor( chat_msg, r_color_name, r_color_alpha);
		
		chat["text_color"] = r_color_name;
		chat["color_alpha"] = r_color_alpha;
		chat["font_size"] = (S32)LLViewerChat::getChatFontSize() ;
		chat["message"] = toast_msg;
		channel->addChat(chat);	
	}
}


//-----------------------------------------------------------------------------------------------
// LLNearbyChatToast
//-----------------------------------------------------------------------------------------------

// virtual
void LLNearbyChatToast::onClose(bool app_quitting)
{
	mNearbyChatScreenChannelp->onToastDestroyed(this, app_quitting);
}

// EOF
