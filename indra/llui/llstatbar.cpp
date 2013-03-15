/** 
 * @file llstatbar.cpp
 * @brief A little map of the world with network information
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llstatbar.h"

#include "llmath.h"
#include "llui.h"
#include "llgl.h"
#include "llfontgl.h"

#include "lluictrlfactory.h"
#include "lltracerecording.h"

///////////////////////////////////////////////////////////////////////////////////

LLStatBar::LLStatBar(const Params& p)
	: LLView(p),
	  mLabel(p.label),
	  mUnitLabel(p.unit_label),
	  mMinBar(p.bar_min),
	  mMaxBar(p.bar_max),
	  mCountFloatp(LLTrace::CountStatHandle<>::getInstance(p.stat)),
	  mCountIntp(LLTrace::CountStatHandle<S64>::getInstance(p.stat)),
	  mMeasurementFloatp(LLTrace::MeasurementStatHandle<>::getInstance(p.stat)),
	  mMeasurementIntp(LLTrace::MeasurementStatHandle<S64>::getInstance(p.stat)),
	  mTickSpacing(p.tick_spacing),
	  mLabelSpacing(p.label_spacing),
	  mPrecision(p.precision),
	  mUpdatesPerSec(p.update_rate),
	  mUnitScale(p.unit_scale),
	  mPerSec(p.show_per_sec),
	  mDisplayBar(p.show_bar),
	  mDisplayHistory(p.show_history),
	  mDisplayMean(p.show_mean),
	  mOrientation(p.orientation)
{}

BOOL LLStatBar::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mDisplayBar)
	{
		if (mDisplayHistory)
		{
			mDisplayBar = FALSE;
			mDisplayHistory = FALSE;
		}
		else
		{
			mDisplayHistory = TRUE;
		}
	}
	else
	{
		mDisplayBar = TRUE;
	}

	LLView* parent = getParent();
	parent->reshape(parent->getRect().getWidth(), parent->getRect().getHeight(), FALSE);

	return FALSE;
}

void LLStatBar::draw()
{
	F32 current = 0.f, 
		min = 0.f, 
		max = 0.f,
		mean = 0.f;

	LLTrace::PeriodicRecording& frame_recording = LLTrace::get_frame_recording();

	if (mCountFloatp)
	{
		LLTrace::Recording& last_frame_recording = frame_recording.getLastRecordingPeriod(); 

		if (mPerSec)
		{
			current = last_frame_recording.getPerSec(*mCountFloatp);
			min = frame_recording.getPeriodMinPerSec(*mCountFloatp);
			max = frame_recording.getPeriodMaxPerSec(*mCountFloatp);
			mean = frame_recording.getPeriodMeanPerSec(*mCountFloatp);
		}
		else
		{
			current = last_frame_recording.getSum(*mCountFloatp);
			min = frame_recording.getPeriodMin(*mCountFloatp);
			max = frame_recording.getPeriodMax(*mCountFloatp);
			mean = frame_recording.getPeriodMean(*mCountFloatp);
		}
	}
	else if (mCountIntp)
	{
		LLTrace::Recording& last_frame_recording = frame_recording.getLastRecordingPeriod(); 

		if (mPerSec)
		{
			current = last_frame_recording.getPerSec(*mCountIntp);
			min = frame_recording.getPeriodMinPerSec(*mCountIntp);
			max = frame_recording.getPeriodMaxPerSec(*mCountIntp);
			mean = frame_recording.getPeriodMeanPerSec(*mCountIntp);
		}
		else
		{
			current = last_frame_recording.getSum(*mCountIntp);
			min = frame_recording.getPeriodMin(*mCountIntp);
			max = frame_recording.getPeriodMax(*mCountIntp);
			mean = frame_recording.getPeriodMean(*mCountIntp);
		}
	}
	else if (mMeasurementFloatp)
	{
		LLTrace::Recording& recording = frame_recording.getTotalRecording();
		current = recording.getLastValue(*mMeasurementFloatp);
		min = recording.getMin(*mMeasurementFloatp);
		max = recording.getMax(*mMeasurementFloatp);
		mean = recording.getMean(*mMeasurementFloatp);
	}
	else if (mMeasurementIntp)
	{
		LLTrace::Recording& recording = frame_recording.getTotalRecording();
		current = recording.getLastValue(*mMeasurementIntp);
		min = recording.getMin(*mMeasurementIntp);
		max = recording.getMax(*mMeasurementIntp);
		mean = recording.getMean(*mMeasurementIntp);
	}

	current *= mUnitScale;
	min *= mUnitScale;
	max *= mUnitScale;
	mean *= mUnitScale;

	if ((mUpdatesPerSec == 0.f) || (mUpdateTimer.getElapsedTimeF32() > 1.f/mUpdatesPerSec) || (mValue == 0.f))
	{
		if (mDisplayMean)
		{
			mValue = mean;
		}
		else
		{
			mValue = current;
		}
		mUpdateTimer.reset();
	}

	S32 bar_top, bar_left, bar_right, bar_bottom;
	if (mOrientation == HORIZONTAL)
	{
		bar_top = getRect().getHeight() - 15;
		bar_left = 0;
		bar_right = getRect().getWidth() - 80;
		bar_bottom = 0;
	}
	else // VERTICAL
	{
		bar_top = getRect().getHeight() - 15; // 16 pixels from top.
		bar_left = 0;
		bar_right = getRect().getWidth();
		bar_bottom = 20;
	}
	const S32 tick_length = 4;
	const S32 tick_width = 1;

	F32 value_scale = (mOrientation == HORIZONTAL) 
					? (bar_top - bar_bottom)/(mMaxBar - mMinBar)
					: (bar_right - bar_left)/(mMaxBar - mMinBar);

	LLFontGL::getFontMonospace()->renderUTF8(mLabel, 0, 0, getRect().getHeight(), LLColor4(1.f, 1.f, 1.f, 1.f),
											 LLFontGL::LEFT, LLFontGL::TOP);

	std::string value_format;
	std::string value_str;
	if (!mUnitLabel.empty())
	{
		value_format = llformat( "%%.%df%%s", mPrecision);
		value_str = llformat( value_format.c_str(), mValue, mUnitLabel.c_str());
	}
	else
	{
		value_format = llformat( "%%.%df", mPrecision);
		value_str = llformat( value_format.c_str(), mValue);
	}

	// Draw the value.
	if (mOrientation == HORIZONTAL)
	{

	}
	else
	{
		LLFontGL::getFontMonospace()->renderUTF8(value_str, 0, bar_right, getRect().getHeight(), 
			LLColor4(1.f, 1.f, 1.f, 0.5f),
			LLFontGL::RIGHT, LLFontGL::TOP);
	}

	value_format = llformat( "%%.%df", mPrecision);
	if (mDisplayBar && (mCountFloatp || mCountIntp || mMeasurementFloatp || mMeasurementIntp))
	{
		std::string tick_label;

		// Draw the tick marks.
		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		for (F32 tick_value = mMinBar; tick_value <= mMaxBar; tick_value += mTickSpacing)
		{
			const S32 begin = llfloor((tick_value - mMinBar)*value_scale);
			const S32 end = begin + tick_width;
			if (mOrientation == HORIZONTAL)
			{
			}
			else
			{
				gl_rect_2d(begin, bar_top, end, bar_bottom - tick_length/2, LLColor4(1.f, 1.f, 1.f, 0.1f));
			}
		}

		// Draw the tick labels (and big ticks).
		for (F32 tick_value = mMinBar; tick_value <= mMaxBar; tick_value += mLabelSpacing)
		{
			const S32 begin = llfloor((tick_value - mMinBar)*value_scale);
			const S32 end = begin + tick_width;
			tick_label = llformat( value_format.c_str(), tick_value);

			// draw labels for the tick marks
			if (mOrientation == HORIZONTAL)
			{
			}
			else
			{
				gl_rect_2d(begin, bar_top, end, bar_bottom - tick_length, LLColor4(1.f, 1.f, 1.f, 0.25f));
				LLFontGL::getFontMonospace()->renderUTF8(tick_label, 0, begin - 1, bar_bottom - tick_length,
					LLColor4(1.f, 1.f, 1.f, 0.5f),
					LLFontGL::LEFT, LLFontGL::TOP);
			}
		}

		// draw background bar.
		gl_rect_2d(bar_left, bar_top, bar_right, bar_bottom, LLColor4(0.f, 0.f, 0.f, 0.25f));

		if (frame_recording.getNumPeriods() == 0)
		{
			// No data, don't draw anything...
			return;
		}

		// draw min and max
		S32 begin = (S32) ((min - mMinBar) * value_scale);

		if (begin < 0)
		{
			begin = 0;
			llwarns << "Min:" << min << llendl;
		}

		S32 end = (S32) ((max - mMinBar) * value_scale);
		if (mOrientation == HORIZONTAL)
		{
			gl_rect_2d(bar_left, end, bar_right, begin, LLColor4(1.f, 0.f, 0.f, 0.25f));
		}
		else // VERTICAL
		{
			gl_rect_2d(begin, bar_top, end, bar_bottom, LLColor4(1.f, 0.f, 0.f, 0.25f));
		}

		if (mDisplayHistory && (mCountFloatp || mCountIntp || mMeasurementFloatp || mMeasurementIntp))
		{
			const S32 num_values = frame_recording.getNumPeriods() - 1;
			S32 begin = 0;
			S32 end = 0;
			S32 i;
			for (i = 1; i <= num_values; i++)
			{
				if (mPerSec)
				{
					if (mCountFloatp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mCountFloatp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mCountFloatp)  - mMinBar) * value_scale) + 1;
					}
					else if (mCountIntp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mCountIntp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mCountIntp)  - mMinBar) * value_scale) + 1;
					}
					else if (mMeasurementFloatp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mMeasurementFloatp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mMeasurementFloatp)  - mMinBar) * value_scale) + 1;
					}
					else if (mMeasurementIntp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mMeasurementIntp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getPerSec(*mMeasurementIntp)  - mMinBar) * value_scale) + 1;
					}
				}
				else
				{
					if (mCountFloatp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mCountFloatp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mCountFloatp)  - mMinBar) * value_scale) + 1;
					}
					else if (mCountIntp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mCountIntp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mCountIntp)  - mMinBar) * value_scale) + 1;
					}
					else if (mMeasurementFloatp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mMeasurementFloatp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mMeasurementFloatp)  - mMinBar) * value_scale) + 1;
					}
					else if (mMeasurementIntp)
					{
						begin = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mMeasurementIntp)  - mMinBar) * value_scale);
						end = (S32)((frame_recording.getPrevRecordingPeriod(i).getSum(*mMeasurementIntp)  - mMinBar) * value_scale) + 1;
					}
				}
				if (mOrientation == HORIZONTAL)
				{
					gl_rect_2d(bar_right - i, end, bar_right - i - 1, begin, LLColor4(1.f, 0.f, 0.f, 1.f));
				}
				else
				{
					gl_rect_2d(begin, bar_bottom+i+1, end, bar_bottom+i, LLColor4(1.f, 0.f, 0.f, 1.f));
				}
			}
		}
		else
		{
			S32 begin = (S32) ((current - mMinBar) * value_scale) - 1;
			S32 end = (S32) ((current - mMinBar) * value_scale) + 1;
			// draw current
			if (mOrientation == HORIZONTAL)
			{
				gl_rect_2d(bar_left, end, bar_right, begin, LLColor4(1.f, 0.f, 0.f, 1.f));
			}
			else
			{
				gl_rect_2d(begin, bar_top, end, bar_bottom, LLColor4(1.f, 0.f, 0.f, 1.f));
			}
		}

		// draw mean bar
		{
			const S32 begin = (S32) ((mean - mMinBar) * value_scale) - 1;
			const S32 end = (S32) ((mean - mMinBar) * value_scale) + 1;
			if (mOrientation == HORIZONTAL)
			{
				gl_rect_2d(bar_left - 2, begin, bar_right + 2, end, LLColor4(0.f, 1.f, 0.f, 1.f));
			}
			else
			{
				gl_rect_2d(begin, bar_top + 2, end, bar_bottom - 2, LLColor4(0.f, 1.f, 0.f, 1.f));
			}
		}
	}
	
	LLView::draw();
}

void LLStatBar::setStat(const std::string& stat_name)
{
	mCountFloatp = LLTrace::CountStatHandle<>::getInstance(stat_name);
	mCountIntp = LLTrace::CountStatHandle<S64>::getInstance(stat_name);
	mMeasurementFloatp = LLTrace::MeasurementStatHandle<>::getInstance(stat_name);
	mMeasurementIntp = LLTrace::MeasurementStatHandle<S64>::getInstance(stat_name);
}


void LLStatBar::setRange(F32 bar_min, F32 bar_max, F32 tick_spacing, F32 label_spacing)
{
	mMinBar = bar_min;
	mMaxBar = bar_max;
	mTickSpacing = tick_spacing;
	mLabelSpacing = label_spacing;
}

LLRect LLStatBar::getRequiredRect()
{
	LLRect rect;

	if (mDisplayBar)
	{
		if (mDisplayHistory)
		{
			rect.mTop = 35 + LLTrace::get_frame_recording().getNumPeriods();
		}
		else
		{
			rect.mTop = 40;
		}
	}
	else
	{
		rect.mTop = 14;
	}
	return rect;
}
