/** 
 * @file lltracerecording.h
 * @brief Sampling object for collecting runtime statistics originating from lltrace.
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

#ifndef LL_LLTRACERECORDING_H
#define LL_LLTRACERECORDING_H

#include "stdtypes.h"
#include "llpreprocessor.h"

#include "llpointer.h"
#include "lltimer.h"
#include "lltraceaccumulators.h"

class LLStopWatchControlsMixinCommon
{
public:
	virtual ~LLStopWatchControlsMixinCommon() {}

	enum EPlayState
	{
		STOPPED,
		PAUSED,
		STARTED
	};

	void start();
	void stop();
	void pause();
	void resume();
	void restart();
	void reset();

	bool isStarted() const { return mPlayState == STARTED; }
	bool isPaused() const  { return mPlayState == PAUSED; }
	bool isStopped() const { return mPlayState == STOPPED; }
	EPlayState getPlayState() const { return mPlayState; }
	// force play state to specific value by calling appropriate handle* methods
	void setPlayState(EPlayState state);

protected:
	LLStopWatchControlsMixinCommon()
	:	mPlayState(STOPPED)
	{}

private:
	// trigger active behavior (without reset)
	virtual void handleStart() = 0;
	// stop active behavior
	virtual void handleStop() = 0;
	// clear accumulated state, can be called while started
	virtual void handleReset() = 0;

	EPlayState mPlayState;
};

template<typename DERIVED>
class LLStopWatchControlsMixin
:	public LLStopWatchControlsMixinCommon
{
public:
	typedef LLStopWatchControlsMixin<DERIVED> self_t;
	virtual void splitTo(DERIVED& other)
	{
		EPlayState play_state = getPlayState();
		stop();
		other.reset();

		handleSplitTo(other);

		other.setPlayState(play_state);
	}

	virtual void splitFrom(DERIVED& other)
	{
		static_cast<self_t&>(other).handleSplitTo(*static_cast<DERIVED*>(this));
	}
private:
	// atomically stop this object while starting the other
	// no data can be missed in between stop and start
	virtual void handleSplitTo(DERIVED& other) {};

};

namespace LLTrace
{
	class Recording 
	:	public LLStopWatchControlsMixin<Recording>
	{
	public:
		Recording();

		Recording(const Recording& other);
		~Recording();

		Recording& operator = (const Recording& other);

		// accumulate data from subsequent, non-overlapping recording
		void appendRecording(Recording& other);

		// grab latest recorded data
		void update();

		// ensure that buffers are exclusively owned by this recording
		void makeUnique() { mBuffers.makeUnique(); }

		// Timer accessors
		LLUnit<F64, LLUnits::Seconds> getSum(const TraceType<TimeBlockAccumulator>& stat);
		LLUnit<F64, LLUnits::Seconds> getSum(const TraceType<TimeBlockAccumulator::SelfTimeFacet>& stat);
		U32 getSum(const TraceType<TimeBlockAccumulator::CallCountFacet>& stat);

		LLUnit<F64, LLUnits::Seconds> getPerSec(const TraceType<TimeBlockAccumulator>& stat);
		LLUnit<F64, LLUnits::Seconds> getPerSec(const TraceType<TimeBlockAccumulator::SelfTimeFacet>& stat);
		F32 getPerSec(const TraceType<TimeBlockAccumulator::CallCountFacet>& stat);

		// Memory accessors
		LLUnit<F64, LLUnits::Bytes> getMin(const TraceType<MemStatAccumulator>& stat);
		LLUnit<F64, LLUnits::Bytes> getMean(const TraceType<MemStatAccumulator>& stat);
		LLUnit<F64, LLUnits::Bytes> getMax(const TraceType<MemStatAccumulator>& stat);
		LLUnit<F64, LLUnits::Bytes> getStandardDeviation(const TraceType<MemStatAccumulator>& stat);
		LLUnit<F64, LLUnits::Bytes> getLastValue(const TraceType<MemStatAccumulator>& stat);

		LLUnit<F64, LLUnits::Bytes> getMin(const TraceType<MemStatAccumulator::ChildMemFacet>& stat);
		LLUnit<F64, LLUnits::Bytes> getMean(const TraceType<MemStatAccumulator::ChildMemFacet>& stat);
		LLUnit<F64, LLUnits::Bytes> getMax(const TraceType<MemStatAccumulator::ChildMemFacet>& stat);
		LLUnit<F64, LLUnits::Bytes> getStandardDeviation(const TraceType<MemStatAccumulator::ChildMemFacet>& stat);
		LLUnit<F64, LLUnits::Bytes> getLastValue(const TraceType<MemStatAccumulator::ChildMemFacet>& stat);

		U32 getSum(const TraceType<MemStatAccumulator::AllocationCountFacet>& stat);
		U32 getSum(const TraceType<MemStatAccumulator::DeallocationCountFacet>& stat);

		// CountStatHandle accessors
		F64 getSum(const TraceType<CountAccumulator>& stat);
		template <typename T>
		T getSum(const CountStatHandle<T>& stat)
		{
			return (T)getSum(static_cast<const TraceType<CountAccumulator>&> (stat));
		}

		F64 getPerSec(const TraceType<CountAccumulator>& stat);
		template <typename T>
		T getPerSec(const CountStatHandle<T>& stat)
		{
			return (T)getPerSec(static_cast<const TraceType<CountAccumulator>&> (stat));
		}

		U32 getSampleCount(const TraceType<CountAccumulator>& stat);


		// SampleStatHandle accessors
		F64 getMin(const TraceType<SampleAccumulator>& stat);
		template <typename T>
		T getMin(const SampleStatHandle<T>& stat)
		{
			return (T)getMin(static_cast<const TraceType<SampleAccumulator>&> (stat));
		}

		F64 getMean(const TraceType<SampleAccumulator>& stat);
		template <typename T>
		T getMean(SampleStatHandle<T>& stat)
		{
			return (T)getMean(static_cast<const TraceType<SampleAccumulator>&> (stat));
		}

		F64 getMax(const TraceType<SampleAccumulator>& stat);
		template <typename T>
		T getMax(const SampleStatHandle<T>& stat)
		{
			return (T)getMax(static_cast<const TraceType<SampleAccumulator>&> (stat));
		}

		F64 getStandardDeviation(const TraceType<SampleAccumulator>& stat);
		template <typename T>
		T getStandardDeviation(const SampleStatHandle<T>& stat)
		{
			return (T)getStandardDeviation(static_cast<const TraceType<SampleAccumulator>&> (stat));
		}

		F64 getLastValue(const TraceType<SampleAccumulator>& stat);
		template <typename T>
		T getLastValue(const SampleStatHandle<T>& stat)
		{
			return (T)getLastValue(static_cast<const TraceType<SampleAccumulator>&> (stat));
		}

		U32 getSampleCount(const TraceType<SampleAccumulator>& stat);

		// EventStatHandle accessors
		F64 getSum(const TraceType<EventAccumulator>& stat);
		template <typename T>
		T getSum(const EventStatHandle<T>& stat)
		{
			return (T)getSum(static_cast<const TraceType<EventAccumulator>&> (stat));
		}

		F64 getMin(const TraceType<EventAccumulator>& stat);
		template <typename T>
		T getMin(const EventStatHandle<T>& stat)
		{
			return (T)getMin(static_cast<const TraceType<EventAccumulator>&> (stat));
		}

		F64 getMax(const TraceType<EventAccumulator>& stat);
		template <typename T>
		T getMax(const EventStatHandle<T>& stat)
		{
			return (T)getMax(static_cast<const TraceType<EventAccumulator>&> (stat));
		}

		F64 getMean(const TraceType<EventAccumulator>& stat);
		template <typename T>
		T getMean(EventStatHandle<T>& stat)
		{
			return (T)getMean(static_cast<const TraceType<EventAccumulator>&> (stat));
		}

		F64 getStandardDeviation(const TraceType<EventAccumulator>& stat);
		template <typename T>
		T getStandardDeviation(const EventStatHandle<T>& stat)
		{
			return (T)getStandardDeviation(static_cast<const TraceType<EventAccumulator>&> (stat));
		}

		F64 getLastValue(const TraceType<EventAccumulator>& stat);
		template <typename T>
		T getLastValue(const EventStatHandle<T>& stat)
		{
			return (T)getLastValue(static_cast<const TraceType<EventAccumulator>&> (stat));
		}

		U32 getSampleCount(const TraceType<EventAccumulator>& stat);

		LLUnit<F64, LLUnits::Seconds> getDuration() const { return mElapsedSeconds; }

	protected:
		friend class ThreadRecorder;

		// implementation for LLStopWatchControlsMixin
		/*virtual*/ void handleStart();
		/*virtual*/ void handleStop();
		/*virtual*/ void handleReset();
		/*virtual*/ void handleSplitTo(Recording& other);

		// returns data for current thread
		class ThreadRecorder* getThreadRecorder(); 

		LLTimer				mSamplingTimer;
		LLUnit<F64, LLUnits::Seconds>			mElapsedSeconds;
		LLCopyOnWritePointer<AccumulatorBufferGroup>	mBuffers;
	};

	class LL_COMMON_API PeriodicRecording
	:	public LLStopWatchControlsMixin<PeriodicRecording>
	{
	public:
		PeriodicRecording(U32 num_periods, EPlayState state = STOPPED);

		void nextPeriod();
		size_t getNumRecordedPeriods() { return mNumPeriods; }

		LLUnit<F64, LLUnits::Seconds> getDuration() const;

		void appendPeriodicRecording(PeriodicRecording& other);
		void appendRecording(Recording& recording);
		Recording& getLastRecording();
		const Recording& getLastRecording() const;
		Recording& getCurRecording();
		const Recording& getCurRecording() const;
		Recording& getPrevRecording(U32 offset);
		const Recording& getPrevRecording(U32 offset) const;
		Recording snapshotCurRecording() const;

		// catch all for stats that have a defined sum
		template <typename T>
		typename T::value_t getPeriodMin(const TraceType<T>& stat, size_t num_periods = U32_MAX)
		{
			size_t total_periods = mRecordingPeriods.size();
			num_periods = llmin(num_periods, isStarted() ? total_periods - 1 : total_periods);

			typename T::value_t min_val = std::numeric_limits<typename T::value_t>::max();
			for (S32 i = 1; i <= num_periods; i++)
			{
				S32 index = (mCurPeriod + total_periods - i) % total_periods;
				min_val = llmin(min_val, mRecordingPeriods[index].getSum(stat));
			}
			return min_val;
		}

		F64 getPeriodMin(const TraceType<SampleAccumulator>& stat, size_t num_periods = U32_MAX);
		template<typename T>
		T getPeriodMin(const SampleStatHandle<T>& stat, size_t num_periods = U32_MAX)
		{
			return T(getPeriodMin(static_cast<const TraceType<SampleAccumulator>&>(stat), num_periods));
		}

		F64 getPeriodMin(const TraceType<EventAccumulator>& stat, size_t num_periods = U32_MAX);
		template<typename T>
		T getPeriodMin(const EventStatHandle<T>& stat, size_t num_periods = U32_MAX)
		{
			return T(getPeriodMin(static_cast<const TraceType<EventAccumulator>&>(stat), num_periods));
		}

		template <typename T>
		F64 getPeriodMinPerSec(const TraceType<T>& stat, size_t num_periods = U32_MAX)
		{
			size_t total_periods = mRecordingPeriods.size();
			num_periods = llmin(num_periods, isStarted() ? total_periods - 1 : total_periods);

			F64 min_val = std::numeric_limits<F64>::max();
			for (S32 i = 1; i <= num_periods; i++)
			{
				S32 index = (mCurPeriod + total_periods - i) % total_periods;
				min_val = llmin(min_val, mRecordingPeriods[index].getPerSec(stat));
			}
			return min_val;
		}

		// catch all for stats that have a defined sum
		template <typename T>
		typename T::value_t getPeriodMax(const TraceType<T>& stat, size_t num_periods = U32_MAX)
		{
			size_t total_periods = mRecordingPeriods.size();
			num_periods = llmin(num_periods, isStarted() ? total_periods - 1 : total_periods);

			typename T::value_t max_val = std::numeric_limits<typename T::value_t>::min();
			for (S32 i = 1; i <= num_periods; i++)
			{
				S32 index = (mCurPeriod + total_periods - i) % total_periods;
				max_val = llmax(max_val, mRecordingPeriods[index].getSum(stat));
			}
			return max_val;
		}

		F64 getPeriodMax(const TraceType<SampleAccumulator>& stat, size_t num_periods = U32_MAX);
		template<typename T>
		T getPeriodMax(const SampleStatHandle<T>& stat, size_t num_periods = U32_MAX)
		{
			return T(getPeriodMax(static_cast<const TraceType<SampleAccumulator>&>(stat), num_periods));
		}

		F64 getPeriodMax(const TraceType<EventAccumulator>& stat, size_t num_periods = U32_MAX);
		template<typename T>
		T getPeriodMax(const EventStatHandle<T>& stat, size_t num_periods = U32_MAX)
		{
			return T(getPeriodMax(static_cast<const TraceType<EventAccumulator>&>(stat), num_periods));
		}

		template <typename T>
		F64 getPeriodMaxPerSec(const TraceType<T>& stat, size_t num_periods = U32_MAX)
		{
			size_t total_periods = mRecordingPeriods.size();
			num_periods = llmin(num_periods, isStarted() ? total_periods - 1 : total_periods);

			F64 max_val = std::numeric_limits<F64>::min();
			for (S32 i = 1; i <= num_periods; i++)
			{
				S32 index = (mCurPeriod + total_periods - i) % total_periods;
				max_val = llmax(max_val, mRecordingPeriods[index].getPerSec(stat));
			}
			return max_val;
		}

		// catch all for stats that have a defined sum
		template <typename T>
		typename T::mean_t getPeriodMean(const TraceType<T >& stat, size_t num_periods = U32_MAX)
		{
			size_t total_periods = mRecordingPeriods.size();
			num_periods = llmin(num_periods, isStarted() ? total_periods - 1 : total_periods);

			typename T::mean_t mean = 0;
			if (num_periods <= 0) { return mean; }

			for (S32 i = 1; i <= num_periods; i++)
			{
				S32 index = (mCurPeriod + total_periods - i) % total_periods;
				if (mRecordingPeriods[index].getDuration() > 0.f)
				{
					mean += mRecordingPeriods[index].getSum(stat);
				}
			}
			mean = mean / num_periods;
			return mean;
		}

		F64 getPeriodMean(const TraceType<SampleAccumulator>& stat, size_t num_periods = U32_MAX);
		template<typename T> 
		T getPeriodMean(const SampleStatHandle<T>& stat, size_t num_periods = U32_MAX)
		{
			return T(getPeriodMean(static_cast<const TraceType<SampleAccumulator>&>(stat), num_periods));
		}

		F64 getPeriodMean(const TraceType<EventAccumulator>& stat, size_t num_periods = U32_MAX);
		template<typename T>
		T getPeriodMean(const EventStatHandle<T>& stat, size_t num_periods = U32_MAX)
		{
			return T(getPeriodMean(static_cast<const TraceType<EventAccumulator>&>(stat), num_periods));
		}

		template <typename T>
		typename T::mean_t getPeriodMeanPerSec(const TraceType<T>& stat, size_t num_periods = U32_MAX)
		{
			size_t total_periods = mRecordingPeriods.size();
			num_periods = llmin(num_periods, isStarted() ? total_periods - 1 : total_periods);

			typename T::mean_t mean = 0;
			if (num_periods <= 0) { return mean; }

			for (S32 i = 1; i <= num_periods; i++)
			{
				S32 index = (mCurPeriod + total_periods - i) % total_periods;
				if (mRecordingPeriods[index].getDuration() > 0.f)
				{
					mean += mRecordingPeriods[index].getPerSec(stat);
				}
			}
			mean = mean / num_periods;
			return mean;
		}

	private:
		// implementation for LLStopWatchControlsMixin
		/*virtual*/ void handleStart();
		/*virtual*/ void handleStop();
		/*virtual*/ void handleReset();
		/*virtual*/ void handleSplitTo(PeriodicRecording& other);

	private:
		std::vector<Recording>	mRecordingPeriods;
		const bool				mAutoResize;
		size_t					mCurPeriod;
		size_t					mNumPeriods;
	};

	PeriodicRecording& get_frame_recording();

	class ExtendableRecording
	:	public LLStopWatchControlsMixin<ExtendableRecording>
	{
	public:
		void extend();

		Recording& getAcceptedRecording() { return mAcceptedRecording; }
		const Recording& getAcceptedRecording() const {return mAcceptedRecording;}

		Recording& getPotentialRecording()				{ return mPotentialRecording; }
		const Recording& getPotentialRecording() const	{ return mPotentialRecording;}

	private:
		// implementation for LLStopWatchControlsMixin
		/*virtual*/ void handleStart();
		/*virtual*/ void handleStop();
		/*virtual*/ void handleReset();
		/*virtual*/ void handleSplitTo(ExtendableRecording& other);

	private:
		Recording mAcceptedRecording;
		Recording mPotentialRecording;
	};

	class ExtendablePeriodicRecording
	:	public LLStopWatchControlsMixin<ExtendablePeriodicRecording>
	{
	public:
		ExtendablePeriodicRecording();
		void extend();

		PeriodicRecording& getAcceptedRecording()				{ return mAcceptedRecording; }
		const PeriodicRecording& getAcceptedRecording() const	{return mAcceptedRecording;}
		
		PeriodicRecording& getPotentialRecording()				{ return mPotentialRecording; }
		const PeriodicRecording& getPotentialRecording() const	{return mPotentialRecording;}

	private:
		// implementation for LLStopWatchControlsMixin
		/*virtual*/ void handleStart();
		/*virtual*/ void handleStop();
		/*virtual*/ void handleReset();
		/*virtual*/ void handleSplitTo(ExtendablePeriodicRecording& other);

	private:
		PeriodicRecording mAcceptedRecording;
		PeriodicRecording mPotentialRecording;
	};
}

#endif // LL_LLTRACERECORDING_H
