
/** 
 * @file lltraceaccumulators.h
 * @brief Storage for accumulating statistics
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

#ifndef LL_LLTRACEACCUMULATORS_H
#define LL_LLTRACEACCUMULATORS_H


#include "stdtypes.h"
#include "llpreprocessor.h"
#include "llunits.h"
#include "lltimer.h"
#include "llrefcount.h"
#include "llthreadlocalstorage.h"
#include "llmemory.h"
#include <limits>

namespace LLTrace
{
	const F64 NaN	= std::numeric_limits<double>::quiet_NaN();

	enum EBufferAppendType
	{
		SEQUENTIAL,
		NON_SEQUENTIAL
	};

	template<typename ACCUMULATOR>
	class AccumulatorBuffer : public LLRefCount
	{
		typedef AccumulatorBuffer<ACCUMULATOR> self_t;
		static const S32 DEFAULT_ACCUMULATOR_BUFFER_SIZE = 32;
	private:
		struct StaticAllocationMarker { };

		AccumulatorBuffer(StaticAllocationMarker m)
		:	mStorageSize(0),
			mStorage(NULL)
		{}

	public:

		AccumulatorBuffer(const AccumulatorBuffer& other = *getDefaultBuffer())
		:	mStorageSize(0),
			mStorage(NULL)
		{
			resize(sNextStorageSlot);
			for (S32 i = 0; i < sNextStorageSlot; i++)
			{
				mStorage[i] = other.mStorage[i];
			}
		}

		~AccumulatorBuffer()
		{
			if (isCurrent())
			{
				LLThreadLocalSingletonPointer<ACCUMULATOR>::setInstance(NULL);
			}
			delete[] mStorage;
		}

		LL_FORCE_INLINE ACCUMULATOR& operator[](size_t index) 
		{ 
			return mStorage[index]; 
		}

		LL_FORCE_INLINE const ACCUMULATOR& operator[](size_t index) const
		{ 
			return mStorage[index]; 
		}

		void addSamples(const AccumulatorBuffer<ACCUMULATOR>& other, EBufferAppendType append_type)
		{
			llassert(mStorageSize >= sNextStorageSlot && other.mStorageSize >= sNextStorageSlot);
			for (size_t i = 0; i < sNextStorageSlot; i++)
			{
				mStorage[i].addSamples(other.mStorage[i], append_type);
			}
		}

		void copyFrom(const AccumulatorBuffer<ACCUMULATOR>& other)
		{
			llassert(mStorageSize >= sNextStorageSlot && other.mStorageSize >= sNextStorageSlot);
			for (size_t i = 0; i < sNextStorageSlot; i++)
			{
				mStorage[i] = other.mStorage[i];
			}
		}

		void reset(const AccumulatorBuffer<ACCUMULATOR>* other = NULL)
		{
			llassert(mStorageSize >= sNextStorageSlot);
			for (size_t i = 0; i < sNextStorageSlot; i++)
			{
				mStorage[i].reset(other ? &other->mStorage[i] : NULL);
			}
		}

		void sync(F64SecondsImplicit time_stamp)
		{
			llassert(mStorageSize >= sNextStorageSlot);
			for (size_t i = 0; i < sNextStorageSlot; i++)
			{
				mStorage[i].sync(time_stamp);
			}
		}

		void makeCurrent()
		{
			LLThreadLocalSingletonPointer<ACCUMULATOR>::setInstance(mStorage);
		}

		bool isCurrent() const
		{
			return LLThreadLocalSingletonPointer<ACCUMULATOR>::getInstance() == mStorage;
		}

		static void clearCurrent()
		{
			LLThreadLocalSingletonPointer<ACCUMULATOR>::setInstance(NULL);
		}

		// NOTE: this is not thread-safe.  We assume that slots are reserved in the main thread before any child threads are spawned
		size_t reserveSlot()
		{
			size_t next_slot = sNextStorageSlot++;
			if (next_slot >= mStorageSize)
			{
				// don't perform doubling, as this should only happen during startup
				// want to keep a tight bounds as we will have a lot of these buffers
				resize(mStorageSize + mStorageSize / 2);
			}
			llassert(mStorage && next_slot < mStorageSize);
			return next_slot;
		}

		void resize(size_t new_size)
		{
			if (new_size <= mStorageSize) return;

			ACCUMULATOR* old_storage = mStorage;
			mStorage = new ACCUMULATOR[new_size];
			if (old_storage)
			{
				for (S32 i = 0; i < mStorageSize; i++)
				{
					mStorage[i] = old_storage[i];
				}
			}
			mStorageSize = new_size;
			delete[] old_storage;

			self_t* default_buffer = getDefaultBuffer();
			if (this != default_buffer
				&& new_size > default_buffer->size())
			{
				//NB: this is not thread safe, but we assume that all resizing occurs during static initialization
				default_buffer->resize(new_size);
			}
		}

		size_t size() const
		{
			return getNumIndices();
		}

		size_t capacity() const
		{
			return mStorageSize;
		}

		static size_t getNumIndices() 
		{
			return sNextStorageSlot;
		}

		static self_t* getDefaultBuffer()
		{
			static bool sInitialized = false;
			if (!sInitialized)
			{
				// this buffer is allowed to leak so that trace calls from global destructors have somewhere to put their data
				// so as not to trigger an access violation
				sDefaultBuffer = new AccumulatorBuffer(StaticAllocationMarker());
				sInitialized = true;
				sDefaultBuffer->resize(DEFAULT_ACCUMULATOR_BUFFER_SIZE);
			}
			return sDefaultBuffer;
		}

	private:
		ACCUMULATOR*	mStorage;
		size_t			mStorageSize;
		static size_t	sNextStorageSlot;
		static self_t*	sDefaultBuffer;
	};

	template<typename ACCUMULATOR> size_t AccumulatorBuffer<ACCUMULATOR>::sNextStorageSlot = 0;
	template<typename ACCUMULATOR> AccumulatorBuffer<ACCUMULATOR>* AccumulatorBuffer<ACCUMULATOR>::sDefaultBuffer = NULL;

	class EventAccumulator
	{
	public:
		typedef F64 value_t;

		EventAccumulator()
		:	mSum(0),
			mMin(NaN),
			mMax(NaN),
			mMean(NaN),
			mSumOfSquares(0),
			mNumSamples(0),
			mLastValue(NaN)
		{}

		void record(F64 value)
		{
			if (mNumSamples == 0)
			{
				mSum = value;
				mMean = value;
				mMin = value;
				mMax = value;
			}
			else
			{
				mSum += value;
				F64 old_mean = mMean;
				mMean += (value - old_mean) / (F64)mNumSamples;
				mSumOfSquares += (value - old_mean) * (value - mMean);

				if (value < mMin) { mMin = value; }
				else if (value > mMax) { mMax = value; }
			}

			mNumSamples++;
			mLastValue = value;
		}

		void addSamples(const EventAccumulator& other, EBufferAppendType append_type);
		void reset(const EventAccumulator* other);
		void sync(F64SecondsImplicit) {}

		F64	getSum() const               { return mSum; }
		F32	getMin() const               { return mMin; }
		F32	getMax() const               { return mMax; }
		F64	getLastValue() const         { return mLastValue; }
		F64	getMean() const              { return mMean; }
		F64 getStandardDeviation() const { return sqrtf(mSumOfSquares / mNumSamples); }
		F64 getSumOfSquares() const		 { return mSumOfSquares; }
		S32 getSampleCount() const       { return mNumSamples; }
		bool hasValue() const			 { return mNumSamples > 0; }

	private:
		F64	mSum,
			mLastValue;

		F64	mMean,
			mSumOfSquares;

		F32 mMin,
			mMax;

		S32	mNumSamples;
	};


	class SampleAccumulator
	{
	public:
		typedef F64 value_t;

		SampleAccumulator()
		:	mSum(0),
			mMin(NaN),
			mMax(NaN),
			mMean(NaN),
			mSumOfSquares(0),
			mLastSampleTimeStamp(0),
			mTotalSamplingTime(0),
			mNumSamples(0),
			mLastValue(NaN),
			mHasValue(false)
		{}

		void sample(F64 value)
		{
			F64SecondsImplicit time_stamp = LLTimer::getTotalSeconds();

			// store effect of last value
			sync(time_stamp);

			if (!mHasValue)
			{
				mHasValue = true;

				mMin = value;
				mMax = value;
				mMean = value;
				mLastSampleTimeStamp = time_stamp;
			}
			else
			{
				if (value < mMin) { mMin = value; }
				else if (value > mMax) { mMax = value; }
			}

			mLastValue = value;
			mNumSamples++;
		}

		void addSamples(const SampleAccumulator& other, EBufferAppendType append_type);
		void reset(const SampleAccumulator* other);

		void sync(F64SecondsImplicit time_stamp)
		{
			if (mHasValue && time_stamp != mLastSampleTimeStamp)
			{
				F64SecondsImplicit delta_time = time_stamp - mLastSampleTimeStamp;
				mSum += mLastValue * delta_time;
				mTotalSamplingTime += delta_time;
				F64 old_mean = mMean;
				mMean += (delta_time / mTotalSamplingTime) * (mLastValue - old_mean);
				mSumOfSquares += delta_time * (mLastValue - old_mean) * (mLastValue - mMean);
			}
			mLastSampleTimeStamp = time_stamp;
		}

		F64	getSum() const               { return mSum; }
		F32	getMin() const               { return mMin; }
		F32	getMax() const               { return mMax; }
		F64	getLastValue() const         { return mLastValue; }
		F64	getMean() const              { return mMean; }
		F64 getStandardDeviation() const { return sqrtf(mSumOfSquares / mTotalSamplingTime); }
		F64 getSumOfSquares() const		 { return mSumOfSquares; }
		F64SecondsImplicit getSamplingTime() { return mTotalSamplingTime; }
		S32 getSampleCount() const       { return mNumSamples; }
		bool hasValue() const            { return mHasValue; }

	private:
		F64		mSum,
				mLastValue;

		F64		mMean,
				mSumOfSquares;

		F64SecondsImplicit	
				mLastSampleTimeStamp,
				mTotalSamplingTime;

		F32		mMin,
				mMax;

		S32		mNumSamples;
		// distinct from mNumSamples, since we might have inherited a last value from
		// a previous sampling period
		bool	mHasValue;		
	};

	class CountAccumulator
	{
	public:
		typedef F64 value_t;

		CountAccumulator()
		:	mSum(0),
			mNumSamples(0)
		{}

		void add(F64 value)
		{
			mNumSamples++;
			mSum += value;
		}

		void addSamples(const CountAccumulator& other, EBufferAppendType /*type*/)
		{
			mSum += other.mSum;
			mNumSamples += other.mNumSamples;
		}

		void reset(const CountAccumulator* other)
		{
			mNumSamples = 0;
			mSum = 0;
		}

		void sync(F64SecondsImplicit) {}

		F64	getSum() const { return mSum; }

		S32 getSampleCount() const { return mNumSamples; }

	private:
		F64	mSum;

		S32	mNumSamples;
	};

	class TimeBlockAccumulator
	{
	public:
		typedef F64Seconds value_t;
		typedef TimeBlockAccumulator self_t;

		// fake classes that allows us to view different facets of underlying statistic
		struct CallCountFacet 
		{
			typedef S32 value_t;
		};

		struct SelfTimeFacet
		{
			typedef F64Seconds value_t;
		};

		// arrays are allocated with 32 byte alignment
		void *operator new [](size_t size)
		{
			return ll_aligned_malloc<32>(size);
		}

		void operator delete[](void* ptr, size_t size)
		{
			ll_aligned_free<32>(ptr);
		}

		TimeBlockAccumulator();
		void addSamples(const self_t& other, EBufferAppendType append_type);
		void reset(const self_t* other);
		void sync(F64SecondsImplicit) {}

		//
		// members
		//
		U64					mTotalTimeCounter,
							mSelfTimeCounter;
		S32					mCalls;
		class TimeBlock*	mParent;		// last acknowledged parent of this time block
		class TimeBlock*	mLastCaller;	// used to bootstrap tree construction
		U16					mActiveCount;	// number of timers with this ID active on stack
		bool				mMoveUpTree;	// needs to be moved up the tree of timers at the end of frame

	};

	class TimeBlock;

	class TimeBlockTreeNode
	{
	public:
		TimeBlockTreeNode();

		void setParent(TimeBlock* parent);
		TimeBlock* getParent() { return mParent; }

		TimeBlock*					mBlock;
		TimeBlock*					mParent;	
		std::vector<TimeBlock*>		mChildren;
		bool						mCollapsed;
		bool						mNeedsSorting;
	};
	
	struct BlockTimerStackRecord
	{
		class BlockTimer*	mActiveTimer;
		class TimeBlock*	mTimeBlock;
		U64					mChildTime;
	};

	struct MemAccumulator
	{
		typedef MemAccumulator self_t;

		// fake classes that allows us to view different facets of underlying statistic
		struct AllocationFacet 
		{
			typedef F64Bytes value_t;
		};

		struct DeallocationFacet 
		{
			typedef F64Bytes value_t;
		};

		void addSamples(const MemAccumulator& other, EBufferAppendType append_type)
		{
			mAllocations.addSamples(other.mAllocations, append_type);
			mDeallocations.addSamples(other.mDeallocations, append_type);

			if (append_type == SEQUENTIAL)
			{
				mSize.addSamples(other.mSize, SEQUENTIAL);
			}
			else
			{
				F64 allocation_delta(other.mAllocations.getSum() - other.mDeallocations.getSum());
				mSize.sample(mSize.hasValue() 
					? mSize.getLastValue() + allocation_delta 
					: allocation_delta);
			}
		}

		void reset(const MemAccumulator* other)
		{
			mSize.reset(other ? &other->mSize : NULL);
			mAllocations.reset(other ? &other->mAllocations : NULL);
			mDeallocations.reset(other ? &other->mDeallocations : NULL);
		}

		void sync(F64SecondsImplicit time_stamp) 
		{
			mSize.sync(time_stamp);
		}

		SampleAccumulator	mSize;
		EventAccumulator	mAllocations;
		CountAccumulator	mDeallocations;
	};

	struct AccumulatorBufferGroup : public LLRefCount
	{
		AccumulatorBufferGroup();
		AccumulatorBufferGroup(const AccumulatorBufferGroup&);
		~AccumulatorBufferGroup();

		void handOffTo(AccumulatorBufferGroup& other);
		void makeCurrent();
		bool isCurrent() const;
		static void clearCurrent();

		void append(const AccumulatorBufferGroup& other);
		void merge(const AccumulatorBufferGroup& other);
		void reset(AccumulatorBufferGroup* other = NULL);
		void sync();

		AccumulatorBuffer<CountAccumulator>	 	mCounts;
		AccumulatorBuffer<SampleAccumulator>	mSamples;
		AccumulatorBuffer<EventAccumulator>		mEvents;
		AccumulatorBuffer<TimeBlockAccumulator> mStackTimers;
		AccumulatorBuffer<MemAccumulator> 	mMemStats;
	};
}

#endif // LL_LLTRACEACCUMULATORS_H

