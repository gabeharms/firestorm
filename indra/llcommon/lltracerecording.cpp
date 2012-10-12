/** 
 * @file lltracesampler.cpp
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "linden_common.h"

#include "lltrace.h"
#include "lltracerecording.h"
#include "lltracethreadrecorder.h"
#include "llthread.h"

namespace LLTrace
{

///////////////////////////////////////////////////////////////////////
// Recording
///////////////////////////////////////////////////////////////////////

Recording::Recording() 
:	mElapsedSeconds(0),
	mRates(new AccumulatorBuffer<RateAccumulator<F32> >()),
	mMeasurements(new AccumulatorBuffer<MeasurementAccumulator<F32> >()),
	mStackTimers(new AccumulatorBuffer<TimerAccumulator>())
{}

Recording::~Recording()
{}

void Recording::update()
{
	if (isStarted())
{
		LLTrace::get_thread_recorder()->update(this);
		mElapsedSeconds = 0.0;
		mSamplingTimer.reset();
	}
}

void Recording::handleReset()
{
	mRates.write()->reset();
	mMeasurements.write()->reset();
	mStackTimers.write()->reset();

	mElapsedSeconds = 0.0;
	mSamplingTimer.reset();
}

void Recording::handleStart()
{
	mSamplingTimer.reset();
	LLTrace::get_thread_recorder()->activate(this);
}

void Recording::handleStop()
{
	mElapsedSeconds += mSamplingTimer.getElapsedTimeF64();
	LLTrace::get_thread_recorder()->deactivate(this);
}

void Recording::handleSplitTo(Recording& other)
{
	stop();
	other.restart();
}


void Recording::makePrimary()
{
	mRates.write()->makePrimary();
	mMeasurements.write()->makePrimary();
	mStackTimers.write()->makePrimary();
}

bool Recording::isPrimary() const
{
	return mRates->isPrimary();
}

void Recording::mergeRecording( const Recording& other )
{
	mRates.write()->addSamples(*other.mRates);
	mMeasurements.write()->addSamples(*other.mMeasurements);
	mStackTimers.write()->addSamples(*other.mStackTimers);
}

///////////////////////////////////////////////////////////////////////
// Recording
///////////////////////////////////////////////////////////////////////

PeriodicRecording::PeriodicRecording( S32 num_periods ) 
:	mNumPeriods(num_periods),
	mCurPeriod(0),
	mTotalValid(false),
	mRecordingPeriods( new Recording[num_periods])
{
	llassert(mNumPeriods > 0);
}

PeriodicRecording::~PeriodicRecording()
{
	delete[] mRecordingPeriods;
}


void PeriodicRecording::nextPeriod()
{
	EPlayState play_state = getPlayState();
	getCurRecordingPeriod().stop();
	mCurPeriod = (mCurPeriod + 1) % mNumPeriods;
	switch(play_state)
	{
	case STOPPED:
		break;
	case PAUSED:
		getCurRecordingPeriod().pause();
		break;
	case STARTED:
		getCurRecordingPeriod().start();
		break;
	}
	// new period, need to recalculate total
	mTotalValid = false;
}

Recording& PeriodicRecording::getTotalRecording()
{
	if (!mTotalValid)
	{
		mTotalRecording.reset();
		for (S32 i = (mCurPeriod + 1) % mNumPeriods; i < mCurPeriod; i++)
		{
			mTotalRecording.mergeRecording(mRecordingPeriods[i]);
		}
	}
	mTotalValid = true;
	return mTotalRecording;
}

void PeriodicRecording::handleStart()
{
	getCurRecordingPeriod().handleStart();
}

void PeriodicRecording::handleStop()
{
	getCurRecordingPeriod().handleStop();
}

void PeriodicRecording::handleReset()
{
	getCurRecordingPeriod().handleReset();
}

void PeriodicRecording::handleSplitTo( PeriodicRecording& other )
{
	getCurRecordingPeriod().handleSplitTo(other.getCurRecordingPeriod());
}

///////////////////////////////////////////////////////////////////////
// ExtendableRecording
///////////////////////////////////////////////////////////////////////

void ExtendableRecording::extend()
{
	mAcceptedRecording.mergeRecording(mPotentialRecording);
	mPotentialRecording.reset();
}

void ExtendableRecording::handleStart()
{
	mPotentialRecording.handleStart();
}

void ExtendableRecording::handleStop()
{
	mPotentialRecording.handleStop();
}

void ExtendableRecording::handleReset()
{
	mAcceptedRecording.handleReset();
	mPotentialRecording.handleReset();
}

void ExtendableRecording::handleSplitTo( ExtendableRecording& other )
{
	mPotentialRecording.handleSplitTo(other.mPotentialRecording);
}

PeriodicRecording& get_frame_recording()
{
	static PeriodicRecording sRecording(64);
	sRecording.start();
	return sRecording;
}

}

void LLVCRControlsMixinCommon::start()
{
	switch (mPlayState)
	{
	case STOPPED:
		handleReset();
		handleStart();
		break;
	case PAUSED:
		handleStart();
		break;
	case STARTED:
		handleReset();
		break;
	}
	mPlayState = STARTED;
}

void LLVCRControlsMixinCommon::stop()
{
	switch (mPlayState)
	{
	case STOPPED:
		break;
	case PAUSED:
		handleStop();
		break;
	case STARTED:
		handleStop();
		break;
	}
	mPlayState = STOPPED;
}

void LLVCRControlsMixinCommon::pause()
{
	switch (mPlayState)
	{
	case STOPPED:
		break;
	case PAUSED:
		break;
	case STARTED:
		handleStop();
		break;
	}
	mPlayState = PAUSED;
}

void LLVCRControlsMixinCommon::resume()
{
	switch (mPlayState)
	{
	case STOPPED:
		handleStart();
		break;
	case PAUSED:
		handleStart();
		break;
	case STARTED:
		break;
	}
	mPlayState = STARTED;
}

void LLVCRControlsMixinCommon::restart()
{
	switch (mPlayState)
	{
	case STOPPED:
		handleReset();
		handleStart();
		break;
	case PAUSED:
		handleReset();
		handleStart();
		break;
	case STARTED:
		handleReset();
		break;
	}
	mPlayState = STARTED;
}

void LLVCRControlsMixinCommon::reset()
{
	handleReset();
}
