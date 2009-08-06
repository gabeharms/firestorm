/** 
 * @file llchannelmanager.cpp
 * @brief This class rules screen notification channels.
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

#include "llchannelmanager.h"

#include "llappviewer.h"
#include "llviewercontrol.h"

#include <algorithm>

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLChannelManager::LLChannelManager()
{
	LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&LLChannelManager::onLoginCompleted, this));
	mChannelList.clear();
	mStartUpChannel = NULL;
}

//--------------------------------------------------------------------------
LLChannelManager::~LLChannelManager()
{
	//All channels are being deleted by Parent View
}

//--------------------------------------------------------------------------
void LLChannelManager::onLoginCompleted()
{
	S32 hidden_notifications = 0;

	for(std::vector<ChannelElem>::iterator it = mChannelList.begin(); it !=  mChannelList.end(); ++it)
	{
		//(*it).channel->showToasts();
		hidden_notifications +=(*it).channel->getNumberOfHiddenToasts();
	}

	if(!hidden_notifications)
	{
		LLScreenChannel::setStartUpToastShown();
		return;
	}
	
	LLChannelManager::Params p;
	p.id = LLUUID(STARTUP_CHANNEL_ID);
	p.channel_right_bound = getRootView()->getRect().mRight - gSavedSettings.getS32("NotificationChannelRightMargin"); 
	p.channel_width = gSavedSettings.getS32("NotifyBoxWidth");
	mStartUpChannel = createChannel(p);

	if(!mStartUpChannel)
		return;

	static_cast<LLUICtrl*>(mStartUpChannel)->setCommitCallback(boost::bind(&LLChannelManager::enableShowToasts, this));
	mStartUpChannel->setNumberOfHiddenToasts(hidden_notifications);
	mStartUpChannel->createOverflowToast(gSavedSettings.getS32("ChannelBottomPanelMargin"), gSavedSettings.getS32("StartUpToastTime"));
}

//--------------------------------------------------------------------------
void LLChannelManager::enableShowToasts()
{
	LLScreenChannel::setStartUpToastShown();
	delete mStartUpChannel;
	mStartUpChannel = NULL;
}

//--------------------------------------------------------------------------
LLScreenChannel* LLChannelManager::createChannel(LLChannelManager::Params& p)
{
	LLScreenChannel* new_channel = NULL;

	if(!p.chiclet)
	{
		new_channel = getChannelByID(p.id);
	}
	else
	{
		new_channel = getChannelByChiclet(p.chiclet);
	}

	if(new_channel)
		return new_channel;

	new_channel = new LLScreenChannel(); 
	getRootView()->addChild(new_channel);
	new_channel->init(p.channel_right_bound - p.channel_width, p.channel_right_bound);
	new_channel->setToastAlignment(p.align);

	ChannelElem new_elem;
	new_elem.id = p.id;
	new_elem.chiclet = p.chiclet;
	new_elem.channel = new_channel;
	
	mChannelList.push_back(new_elem); //TODO: remove chiclet from ScreenChannel?

	return new_channel;
}

//--------------------------------------------------------------------------
LLScreenChannel* LLChannelManager::getChannelByID(const LLUUID id)
{
	std::vector<ChannelElem>::iterator it = find(mChannelList.begin(), mChannelList.end(), id); 
	if(it != mChannelList.end())
	{
		return (*it).channel;
	}

	return NULL;
}

//--------------------------------------------------------------------------
LLScreenChannel* LLChannelManager::getChannelByChiclet(const LLChiclet* chiclet)
{
	std::vector<ChannelElem>::iterator it = find(mChannelList.begin(), mChannelList.end(), chiclet); 
	if(it != mChannelList.end())
	{
		return (*it).channel;
	}

	return NULL;
}

//--------------------------------------------------------------------------
void LLChannelManager::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	for(std::vector<ChannelElem>::iterator it = mChannelList.begin(); it !=  mChannelList.end(); ++it)
	{
		if((*it).channel->getToastAlignment() == NA_CENTRE)
		{
			LLRect channel_rect = (*it).channel->getRect();
			S32 screen_width = getRootView()->getRect().getWidth();
			channel_rect.setLeftTopAndSize(screen_width/2, channel_rect.mTop, channel_rect.getWidth(), channel_rect.getHeight());
			(*it).channel->setRect(channel_rect);
			(*it).channel->showToasts();
		}
	}
}

//--------------------------------------------------------------------------

LLScreenChannel* LLChannelManager::getStartUpChannel()
{
	return mStartUpChannel;
}

//--------------------------------------------------------------------------







