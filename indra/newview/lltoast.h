/** 
 * @file lltoast.h
 * @brief This class implements a placeholder for any notification panel.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLTOAST_H
#define LL_LLTOAST_H


#include "llpanel.h"
#include "llfloater.h"
#include "lltimer.h"
#include "llnotifications.h"

#include "llviewercontrol.h"

#define MOUSE_LEAVE false
#define MOUSE_ENTER true

namespace LLNotificationsUI
{

/**
 * Represents toast pop-up.
 * This is a parent view for all toast panels.
 */
class LLToast : public LLFloater
{
public:
	typedef boost::function<void (LLToast* toast)> toast_callback_t;
	typedef boost::signals2::signal<void (LLToast* toast)> toast_signal_t;

	struct Params :	public LLInitParam::Block<Params, LLFloater::Params>
	{
		LLPanel*			panel;
		LLUUID				id;	 //notification or message ID
		LLNotificationPtr	notification;
		F32					timer_period;
		toast_callback_t	on_toast_destroy;
		toast_callback_t	on_mouse_enter;
		bool				can_fade;
		bool				can_be_stored;
		bool				enable_hide_btn;
		bool				is_modal;
		bool				is_tip;

		Params() :	can_fade(true),
					can_be_stored(true),
					is_modal(false),
					is_tip(false),
					enable_hide_btn(true),
					panel(NULL),
					timer_period(gSavedSettings.getS32("NotificationToastTime"))

		{};
	};

	LLToast(LLToast::Params p);
	virtual ~LLToast();
	BOOL postBuild();

	// Toast handlers
	virtual BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	virtual void onMouseEnter(S32 x, S32 y, MASK mask);
	virtual void onMouseLeave(S32 x, S32 y, MASK mask);

	// Operating with toasts
	// insert a panel to a toast
	void insertPanel(LLPanel* panel);
	// get toast's panel
	LLPanel* getPanel() { return mPanel; }
	// discard notification
	void discardNotification();
	// enable/disable Toast's Hide button
	void setHideButtonEnabled(bool enabled);
	// initialize and start Toast's timer
	void setAndStartTimer(F32 period);
	// 
	void resetTimer() { mTimer.start(); }
	//
	void stopTimer() { mTimer.stop(); }
	//
	void close() { die(); }
	//
	virtual void draw();
	//
	virtual void setVisible(BOOL show);
	//
	virtual void hide();



	// get/set Toast's flags or states
	// get information whether the notification corresponding to the toast is responded or not
	bool getIsNotificationUnResponded();
	//
	bool isViewed() { return mIsViewed; }
	//
	void setCanFade(bool can_fade);
	//
	void setCanBeStored(bool can_be_stored) { mCanBeStored = can_be_stored; }
	//
	bool getCanBeStored() { return mCanBeStored; }
	//
	void setModal(bool modal);


	// Registers callbacks for events
	toast_signal_t mOnFade;
	toast_signal_t mOnMousEnter;
	toast_signal_t mOnToastDestroy;
	boost::signals2::connection setOnFadeCallback(toast_callback_t cb) { return mOnFade.connect(cb); }
	boost::signals2::connection setOnMouseEnterCallback(toast_callback_t cb) { return mOnMousEnter.connect(cb); }
	boost::signals2::connection setOnToastDestroyCallback(toast_callback_t cb) { return mOnToastDestroy.connect(cb); }

	typedef boost::function<void (LLToast* toast, bool mouse_enter)> toast_hover_check_callback_t;
	typedef boost::signals2::signal<void (LLToast* toast, bool mouse_enter)> toast_hover_check_signal_t;
	toast_hover_check_signal_t mOnToastHover;	
	boost::signals2::connection setOnToastHoverCallback(toast_hover_check_callback_t cb) { return mOnToastHover.connect(cb); }


private:

	// check timer
	bool	timerHasExpired();
	// on timer finished function
	void	tick();

	LLUUID				mID;
	LLNotificationPtr	mNotification;

	LLTimer		mTimer;
	F32			mTimerValue;

	LLPanel*	mPanel;
	LLButton*	mHideBtn;

	LLColor4	mBgColor;
	bool		mIsViewed;
	bool		mIsTipNotification;
	bool		mCanFade;
	bool		mIsModal;
	bool		mCanBeStored;
	bool		mHideBtnEnabled;
	bool		mHideBtnPressed;
};

}
#endif
