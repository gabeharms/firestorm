/** 
 * @file lltrace.h
 * @brief Runtime statistics accumulation.
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

#ifndef LL_LLTRACETHREADRECORDER_H
#define LL_LLTRACETHREADRECORDER_H

#include "stdtypes.h"
#include "llpreprocessor.h"

#include "llmutex.h"
#include "lltracerecording.h"

namespace LLTrace
{
	class LL_COMMON_API ThreadRecorder
	{
	protected:
		struct ActiveRecording;
		typedef std::list<ActiveRecording*> active_recording_list_t;
	public:
		ThreadRecorder();

		virtual ~ThreadRecorder();

		void activate(Recording* recording);
		active_recording_list_t::iterator update(Recording* recording);
		void deactivate(Recording* recording);

		virtual void pushToMaster() = 0;

		TimeBlockTreeNode* getTimeBlockTreeNode(S32 index);

	protected:
		struct ActiveRecording
		{
			ActiveRecording(Recording* target);

			Recording*	mTargetRecording;
			Recording	mPartialRecording;

			void moveBaselineToTarget();
		};
		Recording					mThreadRecording;

		active_recording_list_t		mActiveRecordings;

		class BlockTimer*			mRootTimer;
		TimeBlockTreeNode*			mTimeBlockTreeNodes;
		size_t						mNumTimeBlockTreeNodes;
	};

	class LL_COMMON_API MasterThreadRecorder : public ThreadRecorder
	{
	public:
		MasterThreadRecorder();

		void addSlaveThread(class SlaveThreadRecorder* child);
		void removeSlaveThread(class SlaveThreadRecorder* child);

		/*virtual */ void pushToMaster();

		// call this periodically to gather stats data from slave threads
		void pullFromSlaveThreads();

		LLMutex* getSlaveListMutex() { return &mSlaveListMutex; }

	private:

		typedef std::list<class SlaveThreadRecorder*> slave_thread_recorder_list_t;

		slave_thread_recorder_list_t	mSlaveThreadRecorders;
		LLMutex							mSlaveListMutex;
	};

	class LL_COMMON_API SlaveThreadRecorder : public ThreadRecorder
	{
	public:
		SlaveThreadRecorder();
		~SlaveThreadRecorder();

		// call this periodically to gather stats data for master thread to consume
		/*virtual*/ void pushToMaster();

		MasterThreadRecorder* 	mMaster;

		class SharedData
		{
		public:
			void appendFrom(const Recording& source);
			void appendTo(Recording& sink);
			void mergeFrom(const Recording& source);
			void mergeTo(Recording& sink);
			void reset();
		private:
			LLMutex		mRecordingMutex;
			Recording	mRecording;
		};
		SharedData		mSharedData;
	};
}

#endif // LL_LLTRACETHREADRECORDER_H
