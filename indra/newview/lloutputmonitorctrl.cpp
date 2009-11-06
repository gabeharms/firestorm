/** 
 * @file lloutputmonitorctrl.cpp
 * @brief LLOutputMonitorCtrl base class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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
#include "lloutputmonitorctrl.h"

// library includes 
#include "llui.h"

// viewer includes
#include "llvoiceclient.h"
#include "llmutelist.h"
#include "llagent.h"

// default options set in output_monitor.xml
static LLDefaultChildRegistry::Register<LLOutputMonitorCtrl> r("output_monitor");

// The defaults will be initialized in the constructor.
//LLColor4	LLOutputMonitorCtrl::sColorMuted;
//LLColor4	LLOutputMonitorCtrl::sColorOverdriven;
//LLColor4	LLOutputMonitorCtrl::sColorNormal;
LLColor4	LLOutputMonitorCtrl::sColorBound;
//S32			LLOutputMonitorCtrl::sRectsNumber 		= 0;
//F32			LLOutputMonitorCtrl::sRectWidthRatio	= 0.f;
//F32			LLOutputMonitorCtrl::sRectHeightRatio	= 0.f;

LLOutputMonitorCtrl::Params::Params()
:	draw_border("draw_border"),
	image_mute("image_mute"),
	image_off("image_off"),
	image_on("image_on"),
	image_level_1("image_level_1"),
	image_level_2("image_level_2"),
	image_level_3("image_level_3"),
	auto_update("auto_update"),
	speaker_id("speaker_id")
{
	draw_border = true;
	name = "output_monitor";
	follows.flags(FOLLOWS_LEFT|FOLLOWS_TOP);
	mouse_opaque = false;
};

LLOutputMonitorCtrl::LLOutputMonitorCtrl(const LLOutputMonitorCtrl::Params& p)
:	LLView(p),
	mPower(0),
	mImageMute(p.image_mute),
	mImageOff(p.image_off),
	mImageOn(p.image_on),
	mImageLevel1(p.image_level_1),
	mImageLevel2(p.image_level_2),
	mImageLevel3(p.image_level_3),
	mAutoUpdate(p.auto_update),
	mSpeakerId(p.speaker_id),
	mIsAgentControl(false)
{
	//static LLUIColor output_monitor_muted_color = LLUIColorTable::instance().getColor("OutputMonitorMutedColor", LLColor4::orange);
	//static LLUIColor output_monitor_overdriven_color = LLUIColorTable::instance().getColor("OutputMonitorOverdrivenColor", LLColor4::red);
	//static LLUIColor output_monitor_normal_color = LLUIColorTable::instance().getColor("OutputMonitorNotmalColor", LLColor4::green);
	static LLUIColor output_monitor_bound_color = LLUIColorTable::instance().getColor("OutputMonitorBoundColor", LLColor4::white);
	//static LLUICachedControl<S32> output_monitor_rects_number("OutputMonitorRectanglesNumber", 20);
	//static LLUICachedControl<F32> output_monitor_rect_width_ratio("OutputMonitorRectangleWidthRatio", 0.5f);
	//static LLUICachedControl<F32> output_monitor_rect_height_ratio("OutputMonitorRectangleHeightRatio", 0.8f);

	// IAN BUG compare to existing pattern where these are members - some will change per-widget and need to be anyway
	// sent feedback to PE
	
	// *TODO: it looks suboptimal to load the defaults every time an output monitor is constructed.
	//sColorMuted			= output_monitor_muted_color;
	//sColorOverdriven	= output_monitor_overdriven_color;
	//sColorNormal		= output_monitor_normal_color;
	sColorBound			= output_monitor_bound_color;
	//sRectsNumber		= output_monitor_rects_number;
	//sRectWidthRatio		= output_monitor_rect_width_ratio;
	//sRectHeightRatio	= output_monitor_rect_height_ratio;

	mBorder = p.draw_border;

	//with checking mute state
	setSpeakerId(mSpeakerId);
}

LLOutputMonitorCtrl::~LLOutputMonitorCtrl()
{
	LLMuteList::getInstance()->removeObserver(this);
}

void LLOutputMonitorCtrl::setPower(F32 val)
{
	mPower = llmax(0.f, llmin(1.f, val));
}

void LLOutputMonitorCtrl::draw()
{
	// Copied from llmediaremotectrl.cpp
	// *TODO: Give the LLOutputMonitorCtrl an agent-id to monitor, then
	// call directly into gVoiceClient to ask if that agent-id is muted, is
	// speaking, and what power.  This avoids duplicating data, which can get
	// out of sync.
	const F32 LEVEL_0 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL / 3.f;
	const F32 LEVEL_1 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL * 2.f / 3.f;
	const F32 LEVEL_2 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL;

	if (getVisible() && mAutoUpdate && !mIsMuted && mSpeakerId.notNull())
	{
		setPower(gVoiceClient->getCurrentPower(mSpeakerId));
		if(mIsAgentControl)
		{
			setIsTalking(gVoiceClient->getUserPTTState());
		}
		else
		{
			setIsTalking(gVoiceClient->getIsSpeaking(mSpeakerId));
		}
	}

	LLPointer<LLUIImage> icon;
	if (mIsMuted)
	{
		icon = mImageMute;
	}
	else if (mPower == 0.f && !mIsTalking)
	{
		// only show off if PTT is not engaged
		icon = mImageOff;
	}
	else if (mPower < LEVEL_0)
	{
		// PTT is on, possibly with quiet background noise
		icon = mImageOn;
	}
	else if (mPower < LEVEL_1)
	{
		icon = mImageLevel1;
	}
	else if (mPower < LEVEL_2)
	{
		icon = mImageLevel2;
	}
	else
	{
		// overdriven
		icon = mImageLevel3;
	}

	if (icon)
	{
		icon->draw(0, 0);
	}

	//
	// Fill the monitor with a bunch of small rectangles.
	// The rectangles will be filled with gradient color,
	// beginning with sColorNormal and ending with sColorOverdriven.
	// 
	// *TODO: would using a (partially drawn) pixmap instead be faster?
	//
	const int monh		= getRect().getHeight();
	const int monw		= getRect().getWidth();
	//int maxrects		= sRectsNumber;
	//const int period	= llmax(1, monw / maxrects, 0, 0);                    // "1" - min value for the period
	//const int rectw		= llmax(1, llfloor(period * sRectWidthRatio), 0, 0);  // "1" - min value for the rect's width
	//const int recth		= llfloor(monh * sRectHeightRatio);

	//if(period == 1 && rectw == 1) //if we have so small control, then "maxrects = monitor's_width - 2*monitor_border's_width
	//	maxrects = monw-2;

	//const int nrects	= mIsMuted ? maxrects : llfloor(mPower * maxrects); // how many rects to draw?
	//const int rectbtm	= (monh - recth) / 2;
	//const int recttop	= rectbtm + recth;
	//
	//LLColor4 rect_color;
	//
	//for (int i=1, xpos = 0; i <= nrects; i++)
	//{
	//	// Calculate color to use for the current rectangle.
	//	if (mIsMuted)
	//	{
	//		rect_color = sColorMuted;
	//	}
	//	else
	//	{
	//		F32 frac = (mPower * i/nrects) / LLVoiceClient::OVERDRIVEN_POWER_LEVEL;
	//		// Use overdriven color if the power exceeds overdriven level.
	//		if (frac > 1.0f)
	//			frac = 1.0f;
	//		rect_color = lerp(sColorNormal, sColorOverdriven, frac);
	//	}

	//	// Draw rectangle filled with the color.
	//	gl_rect_2d(xpos, recttop, xpos+rectw, rectbtm, rect_color, TRUE);
	//	xpos += period;
	//}

	//
	// Draw bounding box.
	//
	if(mBorder)
		gl_rect_2d(0, monh, monw, 0, sColorBound, FALSE);
}

void LLOutputMonitorCtrl::setSpeakerId(const LLUUID& speaker_id)
{
	if (speaker_id.isNull() || speaker_id == mSpeakerId) return;

	mSpeakerId = speaker_id;

	//mute management
	if (mAutoUpdate)
	{
		if (speaker_id == gAgentID)
		{
			setIsMuted(false);
		}
		else
		{
			setIsMuted(LLMuteList::getInstance()->isMuted(mSpeakerId));
			LLMuteList::getInstance()->addObserver(this);
		}
	}
}

void LLOutputMonitorCtrl::onChange()
{
	setIsMuted(LLMuteList::getInstance()->isMuted(mSpeakerId));
}
