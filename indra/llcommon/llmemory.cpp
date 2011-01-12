/** 
 * @file llmemory.cpp
 * @brief Very special memory allocation/deallocation stuff here
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "linden_common.h"

#include "llthread.h"
#if defined(LL_WINDOWS)
//# include <windows.h>
# include <psapi.h>
#elif defined(LL_DARWIN)
# include <sys/types.h>
# include <mach/task.h>
# include <mach/mach_init.h>
#elif LL_LINUX || LL_SOLARIS
# include <unistd.h>
#endif

#include "llmemory.h"
#include "llsys.h"
#include "llframetimer.h"

//----------------------------------------------------------------------------

//static
char* LLMemory::reserveMem = 0;
U32 LLMemory::sAvailPhysicalMemInKB = U32_MAX ;
U32 LLMemory::sMaxPhysicalMemInKB = 0;
U32 LLMemory::sAllocatedMemInKB = 0;
U32 LLMemory::sAllocatedPageSizeInKB = 0 ;
U32 LLMemory::sMaxHeapSizeInKB = U32_MAX ;
BOOL LLMemory::sEnableMemoryFailurePrevention = FALSE;

//static
void LLMemory::initClass()
{
	if (!reserveMem)
	{
		reserveMem = new char[16*1024]; // reserve 16K for out of memory error handling
	}
}

//static
void LLMemory::cleanupClass()
{
	delete [] reserveMem;
	reserveMem = NULL;
}

//static
void LLMemory::freeReserve()
{
	delete [] reserveMem;
	reserveMem = NULL;
}

//static 
void LLMemory::initMaxHeapSizeGB(F32 max_heap_size_gb, BOOL prevent_heap_failure)
{
	sMaxHeapSizeInKB = (U32)(max_heap_size_gb * 1024 * 1024) ;
	sEnableMemoryFailurePrevention = prevent_heap_failure ;
}

//static 
void LLMemory::updateMemoryInfo() 
{
#if LL_WINDOWS	
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;
	
	if (!GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		llwarns << "GetProcessMemoryInfo failed" << llendl;
		return ;
	}

	sAllocatedMemInKB = (U32)(counters.WorkingSetSize / 1024) ;
	sAllocatedPageSizeInKB = (U32)(counters.PagefileUsage / 1024) ;
	sMaxPhysicalMemInKB = llmin(LLMemoryInfo::getAvailableMemoryKB() + sAllocatedMemInKB, sMaxHeapSizeInKB);

	if(sMaxPhysicalMemInKB > sAllocatedMemInKB)
	{
		sAvailPhysicalMemInKB = sMaxPhysicalMemInKB - sAllocatedMemInKB ;
	}
	else
	{
		sAvailPhysicalMemInKB = 0 ;
	}
#else
	//not valid for other systems for now.
	sAllocatedMemInKB = (U32)(LLMemory::getCurrentRSS() / 1024) ;
	sMaxPhysicalMemInKB = U32_MAX ;
	sAvailPhysicalMemInKB = U32_MAX ;
#endif

	return ;
}

//
//this function is to test if there is enough space with the size in the virtual address space.
//it does not do any real allocation
//if success, it returns the address where the memory chunk can fit in;
//otherwise it returns NULL.
//
//static 
void* LLMemory::tryToAlloc(void* address, U32 size)
{
#if LL_WINDOWS
	address = VirtualAlloc(address, size, MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS) ;
	if(address)
	{
		if(!VirtualFree(address, 0, MEM_RELEASE))
		{
			llerrs << "error happens when free some memory reservation." << llendl ;
		}
	}
#else
#endif

	return address ;
}

//static 
void LLMemory::logMemoryInfo(BOOL update)
{
	if(update)
	{
		updateMemoryInfo() ;
	}

	llinfos << "Current allocated physical memory(KB): " << sAllocatedMemInKB << llendl ;
	llinfos << "Current allocated page size (KB): " << sAllocatedPageSizeInKB << llendl ;
	llinfos << "Current availabe physical memory(KB): " << sAvailPhysicalMemInKB << llendl ;
	llinfos << "Current max usable memory(KB): " << sMaxPhysicalMemInKB << llendl ;
}

//return 0: everything is normal;
//return 1: the memory pool is low, but not in danger;
//return -1: the memory pool is in danger, is about to crash.
//static 
S32 LLMemory::isMemoryPoolLow()
{
	static const U32 LOW_MEMEOY_POOL_THRESHOLD_KB = 64 * 1024 ; //64 MB for emergency use

	if(!sEnableMemoryFailurePrevention)
	{
		return 0 ; //no memory failure prevention.
	}

	if(sAvailPhysicalMemInKB < (LOW_MEMEOY_POOL_THRESHOLD_KB >> 2)) //out of physical memory
	{
		return -1 ;
	}

	if(sAllocatedPageSizeInKB + (LOW_MEMEOY_POOL_THRESHOLD_KB >> 2) > sMaxHeapSizeInKB) //out of virtual address space.
	{
		return -1 ;
	}

	return (S32)(sAvailPhysicalMemInKB < LOW_MEMEOY_POOL_THRESHOLD_KB || 
		sAllocatedPageSizeInKB + LOW_MEMEOY_POOL_THRESHOLD_KB > sMaxHeapSizeInKB) ;
}

//static 
U32 LLMemory::getAvailableMemKB() 
{
	return sAvailPhysicalMemInKB ;
}

//static 
U32 LLMemory::getMaxMemKB() 
{
	return sMaxPhysicalMemInKB ;
}

//static 
U32 LLMemory::getAllocatedMemKB() 
{
	return sAllocatedMemInKB ;
}

void* ll_allocate (size_t size)
{
	if (size == 0)
	{
		llwarns << "Null allocation" << llendl;
	}
	void *p = malloc(size);
	if (p == NULL)
	{
		LLMemory::freeReserve();
		llerrs << "Out of memory Error" << llendl;
	}
	return p;
}

void ll_release (void *p)
{
	free(p);
}

//----------------------------------------------------------------------------

#if defined(LL_WINDOWS)

U64 LLMemory::getCurrentRSS()
{
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;
	
	if (!GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		llwarns << "GetProcessMemoryInfo failed" << llendl;
		return 0;
	}

	return counters.WorkingSetSize;
}

#elif defined(LL_DARWIN)

/* 
	The API used here is not capable of dealing with 64-bit memory sizes, but is available before 10.4.
	
	Once we start requiring 10.4, we can use the updated API, which looks like this:
	
	task_basic_info_64_data_t basicInfo;
	mach_msg_type_number_t  basicInfoCount = TASK_BASIC_INFO_64_COUNT;
	if (task_info(mach_task_self(), TASK_BASIC_INFO_64, (task_info_t)&basicInfo, &basicInfoCount) == KERN_SUCCESS)
	
	Of course, this doesn't gain us anything unless we start building the viewer as a 64-bit executable, since that's the only way
	for our memory allocation to exceed 2^32.
*/

// 	if (sysctl(ctl, 2, &page_size, &size, NULL, 0) == -1)
// 	{
// 		llwarns << "Couldn't get page size" << llendl;
// 		return 0;
// 	} else {
// 		return page_size;
// 	}
// }

U64 LLMemory::getCurrentRSS()
{
	U64 residentSize = 0;
	task_basic_info_data_t basicInfo;
	mach_msg_type_number_t  basicInfoCount = TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&basicInfo, &basicInfoCount) == KERN_SUCCESS)
	{
		residentSize = basicInfo.resident_size;

		// If we ever wanted it, the process virtual size is also available as:
		// virtualSize = basicInfo.virtual_size;
		
//		llinfos << "resident size is " << residentSize << llendl;
	}
	else
	{
		llwarns << "task_info failed" << llendl;
	}

	return residentSize;
}

#elif defined(LL_LINUX)

U64 LLMemory::getCurrentRSS()
{
	static const char statPath[] = "/proc/self/stat";
	LLFILE *fp = LLFile::fopen(statPath, "r");
	U64 rss = 0;

	if (fp == NULL)
	{
		llwarns << "couldn't open " << statPath << llendl;
		goto bail;
	}

	// Eee-yew!	 See Documentation/filesystems/proc.txt in your
	// nearest friendly kernel tree for details.
	
	{
		int ret = fscanf(fp, "%*d (%*[^)]) %*c %*d %*d %*d %*d %*d %*d %*d "
						 "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %Lu",
						 &rss);
		if (ret != 1)
		{
			llwarns << "couldn't parse contents of " << statPath << llendl;
			rss = 0;
		}
	}
	
	fclose(fp);

bail:
	return rss;
}

#elif LL_SOLARIS
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define _STRUCTURED_PROC 1
#include <sys/procfs.h>

U64 LLMemory::getCurrentRSS()
{
	char path [LL_MAX_PATH];	/* Flawfinder: ignore */ 

	sprintf(path, "/proc/%d/psinfo", (int)getpid());
	int proc_fd = -1;
	if((proc_fd = open(path, O_RDONLY)) == -1){
		llwarns << "LLmemory::getCurrentRSS() unable to open " << path << ". Returning 0 RSS!" << llendl;
		return 0;
	}
	psinfo_t proc_psinfo;
	if(read(proc_fd, &proc_psinfo, sizeof(psinfo_t)) != sizeof(psinfo_t)){
		llwarns << "LLmemory::getCurrentRSS() Unable to read from " << path << ". Returning 0 RSS!" << llendl;
		close(proc_fd);
		return 0;
	}

	close(proc_fd);

	return((U64)proc_psinfo.pr_rssize * 1024);
}
#else

U64 LLMemory::getCurrentRSS()
{
	return 0;
}

#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//minimum block sizes (page size) for small allocation, medium allocation, large allocation 
const U32 MIN_BLOCK_SIZES[LLPrivateMemoryPool::SUPER_ALLOCATION] = {2 << 10, 4 << 10, 16 << 10} ; //

//maximum block sizes for small allocation, medium allocation, large allocation 
const U32 MAX_BLOCK_SIZES[LLPrivateMemoryPool::SUPER_ALLOCATION] = {64 << 10, 1 << 20, 4 << 20} ;

//minimum slot sizes for small allocation, medium allocation, large allocation 
const U32 MIN_SLOT_SIZES[LLPrivateMemoryPool::SUPER_ALLOCATION]  = {8, 2 << 10, 512 << 10};

//maximum slot sizes for small allocation, medium allocation, large allocation 
const U32 MAX_SLOT_SIZES[LLPrivateMemoryPool::SUPER_ALLOCATION]  = {(2 << 10) - 8, (512 - 2) << 10, 4 << 20};

//size of a block with multiple slots can not exceed CUT_OFF_SIZE
const U32 CUT_OFF_SIZE = (64 << 10) ; //64 KB

//-------------------------------------------------------------
//class LLPrivateMemoryPool::LLMemoryBlock
//-------------------------------------------------------------
//
//each memory block could fit for two page sizes: 0.75 * mSlotSize, which starts from the beginning of the memory chunk and grow towards the end of the
//the block; another is mSlotSize, which starts from the end of the block and grows towards the beginning of the block.
//
LLPrivateMemoryPool::LLMemoryBlock::LLMemoryBlock()
{
	//empty
}
		
LLPrivateMemoryPool::LLMemoryBlock::~LLMemoryBlock() 
{
	//empty
}

//create and initialize a memory block
void LLPrivateMemoryPool::LLMemoryBlock::init(char* buffer, U32 buffer_size, U32 slot_size)
{
	mBuffer = buffer ;
	mBufferSize = buffer_size ;
	mSlotSize = slot_size ;
	mTotalSlots = buffer_size / mSlotSize ;	
	
	llassert_always(buffer_size / mSlotSize < 256) ; //max number is 256
	
	mAllocatedSlots = 0 ;

	//init the bit map.
	//mark free bits
	S32 usage_bit_len = (mTotalSlots + 31) / 32 ;
	mDummySize = usage_bit_len - 1 ; //if the mTotalSlots more than 32, needs extra space for bit map
	if(mDummySize > 0) //reserve extra space from mBuffer to store bitmap if needed.
	{
		mTotalSlots -= (mDummySize * sizeof(mUsageBits) + mSlotSize - 1) / mSlotSize ;
		usage_bit_len = (mTotalSlots + 31) / 32 ;
		mDummySize = usage_bit_len - 1 ;//number of 32bits reserved from mBuffer for bitmap

		if(mDummySize > 0)
		{
			mUsageBits = 0 ;
			for(S32 i = 0 ; i < mDummySize ; i++)
			{
				*((U32*)mBuffer + i) = 0 ;
			}
			if(mTotalSlots & 31)
			{
				*((U32*)mBuffer + mDummySize - 1) = (0xffffffff << (mTotalSlots & 31)) ;
			}
		}
	}
	
	if(mDummySize < 1)//no extra bitmap space reserved
	{
		mUsageBits = 0 ;
		if(mTotalSlots & 31)
		{
			mUsageBits = (0xffffffff << (mTotalSlots & 31)) ;
		}
	}

	mSelf = this ;
	mNext = NULL ;
	mPrev = NULL ;

	llassert_always(mTotalSlots > 0) ;
}

//mark this block to be free with the memory [mBuffer, mBuffer + mBufferSize).
void LLPrivateMemoryPool::LLMemoryBlock::setBuffer(char* buffer, U32 buffer_size)
{
	mBuffer = buffer ;
	mBufferSize = buffer_size ;
	mSelf = NULL ;
	mTotalSlots = 0 ; //set the block is free.
}

//reserve a slot
char* LLPrivateMemoryPool::LLMemoryBlock::allocate() 
{
	llassert_always(mAllocatedSlots < mTotalSlots) ;
	
	//find a free slot
	U32* bits = NULL ;
	U32  k = 0 ;
	if(mUsageBits != 0xffffffff)
	{
		bits = &mUsageBits ;
	}
	else if(mDummySize > 0)//go to extra space
	{		
		for(S32 i = 0 ; i < mDummySize; i++)
		{
			if(*((U32*)mBuffer + i) != 0xffffffff)
			{
				bits = (U32*)mBuffer + i ;
				k = i + 1 ;
				break ;
			}
		}
	}	
	S32 idx = 0 ;
	U32 tmp = *bits ;
	for(; tmp & 1 ; tmp >>= 1, idx++) ;

	//set the slot reserved
	if(!idx)
	{
		*bits |= 1 ;
	}
	else
	{
		*bits |= (1 << idx) ;
	}

	mAllocatedSlots++ ;
	
	return mBuffer + mDummySize * sizeof(U32) + (k * 32 + idx) * mSlotSize ;
}

//free a slot
void  LLPrivateMemoryPool::LLMemoryBlock::free(void* addr) 
{
	//bit index
	U32 idx = ((U32)addr - (U32)mBuffer - mDummySize * sizeof(U32)) / mSlotSize ;

	U32* bits = &mUsageBits ;
	if(idx >= 32)
	{
		bits = (U32*)mBuffer + (idx - 32) / 32 ;
	}

	//reset the bit
	if(idx & 31)
	{
		*bits &= ~(1 << (idx & 31)) ;
	}
	else
	{
		*bits &= ~1 ;
	}

	mAllocatedSlots-- ;
}

//for debug use: reset the entire bitmap.
void  LLPrivateMemoryPool::LLMemoryBlock::resetBitMap()
{
	for(S32 i = 0 ; i < mDummySize ; i++)
	{
		*((U32*)mBuffer + i) = 0 ;
	}
	mUsageBits = 0 ;
}
//-------------------------------------------------------------------
//class LLMemoryChunk
//--------------------------------------------------------------------
LLPrivateMemoryPool::LLMemoryChunk::LLMemoryChunk()
{
	//empty
}

LLPrivateMemoryPool::LLMemoryChunk::~LLMemoryChunk()
{
	//empty
}

//create and init a memory chunk
void LLPrivateMemoryPool::LLMemoryChunk::init(char* buffer, U32 buffer_size, U32 min_slot_size, U32 max_slot_size, U32 min_block_size, U32 max_block_size) 
{
	mBuffer = buffer ;
	mBufferSize = buffer_size ;
	mAlloatedSize = 0 ;

	mMetaBuffer = mBuffer + sizeof(LLMemoryChunk) ;

	mMinBlockSize = min_block_size; //page size
	mMinSlotSize = min_slot_size;
	mMaxSlotSize = max_slot_size ;
	mBlockLevels = mMaxSlotSize / mMinSlotSize ;
	mPartitionLevels = max_block_size / mMinBlockSize + 1 ;

	S32 max_num_blocks = (buffer_size - sizeof(LLMemoryChunk) - mBlockLevels * sizeof(LLMemoryBlock*) - mPartitionLevels * sizeof(LLMemoryBlock*)) / 
		                 (mMinBlockSize + sizeof(LLMemoryBlock)) ;
	//meta data space
	mBlocks = (LLMemoryBlock*)mMetaBuffer ; //space reserved for all memory blocks.
	mAvailBlockList = (LLMemoryBlock**)((char*)mBlocks + sizeof(LLMemoryBlock) * max_num_blocks) ; 
	mFreeSpaceList = (LLMemoryBlock**)((char*)mAvailBlockList + sizeof(LLMemoryBlock*) * mBlockLevels) ; 
	
	//data buffer, which can be used for allocation
	mDataBuffer = (char*)mFreeSpaceList + sizeof(LLMemoryBlock*) * mPartitionLevels ;

	//init
	for(U32 i = 0 ; i < mBlockLevels; i++)
	{
		mAvailBlockList[i] = NULL ;
	}
	for(U32 i = 0 ; i < mPartitionLevels ; i++)
	{
		mFreeSpaceList[i] = NULL ;
	}

	//assign the entire chunk to the first block
	mBlocks[0].mPrev = NULL ;
	mBlocks[0].mNext = NULL ;
	mBlocks[0].setBuffer(mDataBuffer, buffer_size - (mDataBuffer - mBuffer)) ;
	addToFreeSpace(&mBlocks[0]) ;

	mHashNext = NULL ;
	mNext = NULL ;
	mPrev = NULL ;
}

//static 
U32 LLPrivateMemoryPool::LLMemoryChunk::getMaxOverhead(U32 data_buffer_size, U32 min_slot_size, 
													   U32 max_slot_size, U32 min_block_size, U32 max_block_size)
{
	//for large allocations, reserve some extra memory for meta data to avoid wasting much 
	if(data_buffer_size / min_slot_size < 64) //large allocations
	{
		U32 overhead = sizeof(LLMemoryChunk) + (data_buffer_size / min_block_size) * sizeof(LLMemoryBlock) +
			sizeof(LLMemoryBlock*) * (max_slot_size / min_slot_size) + sizeof(LLMemoryBlock*) * (max_block_size / min_block_size + 1) ;

		//round to integer times of min_block_size
		overhead = ((overhead + min_block_size - 1) / min_block_size) * min_block_size ;
		return overhead ;
	}
	else
	{
		return 0 ; //do not reserve extra overhead if for small allocations
	}
}

char* LLPrivateMemoryPool::LLMemoryChunk::allocate(U32 size)
{
	if(mMinSlotSize > size)
	{
		size = mMinSlotSize ;
	}
	if(mAlloatedSize + size  > mBufferSize - (mDataBuffer - mBuffer))
	{
		return NULL ; //no enough space in this chunk.
	}

	char* p = NULL ;
	U32 blk_idx = getBlockLevel(size);

	LLMemoryBlock* blk = NULL ;

	//check if there is free block available
	if(mAvailBlockList[blk_idx])
	{
		blk = mAvailBlockList[blk_idx] ;
		p = blk->allocate() ;
		
		if(blk->isFull())
		{
			popAvailBlockList(blk_idx) ;
		}
	}

	//ask for a new block
	if(!p)
	{
		blk = addBlock(blk_idx) ;
		if(blk)
		{
			p = blk->allocate() ;

			if(blk->isFull())
			{
				popAvailBlockList(blk_idx) ;
			}
		}
	}

	//ask for space from larger blocks
	if(!p)
	{
		for(S32 i = blk_idx + 1 ; i < mBlockLevels; i++)
		{
			if(mAvailBlockList[i])
			{
				blk = mAvailBlockList[i] ;
				p = blk->allocate() ;

				if(blk->isFull())
				{
					popAvailBlockList(i) ;
				}
				break ;
			}
		}
	}

	if(p && blk)
	{		
		mAlloatedSize += blk->getSlotSize() ;
	}
	return p ;
}

void LLPrivateMemoryPool::LLMemoryChunk::free(void* addr)
{	
	U32 blk_idx = getPageIndex((U32)addr) ;
	LLMemoryBlock* blk = (LLMemoryBlock*)(mMetaBuffer + blk_idx * sizeof(LLMemoryBlock)) ;
	blk = blk->mSelf ;

	bool was_full = blk->isFull() ;
	blk->free(addr) ;
	mAlloatedSize -= blk->getSlotSize() ;

	if(blk->empty())
	{
		removeBlock(blk) ;
	}
	else if(was_full)
	{
		addToAvailBlockList(blk) ;
	}	
}

bool LLPrivateMemoryPool::LLMemoryChunk::empty()
{
	return !mAlloatedSize ;
}

bool LLPrivateMemoryPool::LLMemoryChunk::containsAddress(const char* addr) const
{
	return (U32)mBuffer <= (U32)addr && (U32)mBuffer + mBufferSize > (U32)addr ;
}

//debug use
void LLPrivateMemoryPool::LLMemoryChunk::dump()
{
#if 0
	//sanity check
	//for(S32 i = 0 ; i < mBlockLevels ; i++)
	//{
	//	LLMemoryBlock* blk = mAvailBlockList[i] ;
	//	while(blk)
	//	{
	//		blk_list.push_back(blk) ;
	//		blk = blk->mNext ;
	//	}
	//}
	for(S32 i = 0 ; i < mPartitionLevels ; i++)
	{
		LLMemoryBlock* blk = mFreeSpaceList[i] ;
		while(blk)
		{
			blk_list.push_back(blk) ;
			blk = blk->mNext ;
		}
	}

	std::sort(blk_list.begin(), blk_list.end(), LLMemoryBlock::CompareAddress());

	U32 total_size = blk_list[0]->getBufferSize() ;
	for(U32 i = 1 ; i < blk_list.size(); i++)
	{
		total_size += blk_list[i]->getBufferSize() ;
		if((U32)blk_list[i]->getBuffer() < (U32)blk_list[i-1]->getBuffer() + blk_list[i-1]->getBufferSize())
		{
			llerrs << "buffer corrupted." << llendl ;
		}
	}

	llassert_always(total_size + mMinBlockSize >= mBufferSize - ((U32)mDataBuffer - (U32)mBuffer)) ;

	U32 blk_num = (mBufferSize - (mDataBuffer - mBuffer)) / mMinBlockSize ;
	for(U32 i = 0 ; i < blk_num ; )
	{
		LLMemoryBlock* blk = &mBlocks[i] ;
		if(blk->mSelf)
		{
			U32 end = blk->getBufferSize() / mMinBlockSize ;
			for(U32 j = 0 ; j < end ; j++)
			{
				llassert_always(blk->mSelf == blk || !blk->mSelf) ;
			}
			i += end ;
		}
		else
		{
			llerrs << "gap happens" << llendl ;
		}
	}
#endif
#if 0
	llinfos << "---------------------------" << llendl ;
	llinfos << "Chunk buffer: " << (U32)getBuffer() << " size: " << getBufferSize() << llendl ;

	llinfos << "available blocks ... " << llendl ;
	for(S32 i = 0 ; i < mBlockLevels ; i++)
	{
		LLMemoryBlock* blk = mAvailBlockList[i] ;
		while(blk)
		{
			llinfos << "blk buffer " << (U32)blk->getBuffer() << " size: " << blk->getBufferSize() << llendl ;
			blk = blk->mNext ;
		}
	}

	llinfos << "free blocks ... " << llendl ;
	for(S32 i = 0 ; i < mPartitionLevels ; i++)
	{
		LLMemoryBlock* blk = mFreeSpaceList[i] ;
		while(blk)
		{
			llinfos << "blk buffer " << (U32)blk->getBuffer() << " size: " << blk->getBufferSize() << llendl ;
			blk = blk->mNext ;
		}
	}
#endif
}

//compute the size for a block, the size is round to integer times of mMinBlockSize.
U32 LLPrivateMemoryPool::LLMemoryChunk::calcBlockSize(U32 slot_size)
{
	//
	//Note: we try to make a block to have 32 slots if the size is not over 32 pages
	//32 is the number of bits of an integer in a 32-bit system
	//

	U32 block_size;
	U32 cut_off_size = llmin(CUT_OFF_SIZE, (U32)(mMinBlockSize << 5)) ;

	if((slot_size << 5) <= mMinBlockSize)//for small allocations, return one page 
	{
		block_size = mMinBlockSize ;
	}
	else if(slot_size >= cut_off_size)//for large allocations, return one-slot block
	{
		block_size = (slot_size / mMinBlockSize) * mMinBlockSize ;
		if(block_size < slot_size)
		{
			block_size += mMinBlockSize ;
		}
	}
	else //medium allocations
	{
		if((slot_size << 5) >= cut_off_size)
		{
			block_size = cut_off_size ;
		}
		else
		{
			block_size = ((slot_size << 5) / mMinBlockSize) * mMinBlockSize ;
		}
	}

	llassert_always(block_size >= slot_size) ;

	return block_size ;
}

//create a new block in the chunk
LLPrivateMemoryPool::LLMemoryBlock* LLPrivateMemoryPool::LLMemoryChunk::addBlock(U32 blk_idx)
{	
	U32 slot_size = mMinSlotSize * (blk_idx + 1) ;
	U32 preferred_block_size = calcBlockSize(slot_size) ;	
	U16 idx = getPageLevel(preferred_block_size); 
	LLMemoryBlock* blk = NULL ;
	
	if(mFreeSpaceList[idx])//if there is free slot for blk_idx
	{
		blk = createNewBlock(mFreeSpaceList[idx], preferred_block_size, slot_size, blk_idx) ;
	}
	else if(mFreeSpaceList[mPartitionLevels - 1]) //search free pool
	{		
		blk = createNewBlock(mFreeSpaceList[mPartitionLevels - 1], preferred_block_size, slot_size, blk_idx) ;
	}
	else //search for other non-preferred but enough space slot.
	{
		S32 min_idx = 0 ;
		if(slot_size > mMinBlockSize)
		{
			min_idx = getPageLevel(slot_size) ;
		}
		for(S32 i = (S32)idx - 1 ; i >= min_idx ; i--) //search the small slots first
		{
			if(mFreeSpaceList[i])
			{
				U32 new_preferred_block_size = mFreeSpaceList[i]->getBufferSize();
				new_preferred_block_size = (new_preferred_block_size / mMinBlockSize) * mMinBlockSize ; //round to integer times of mMinBlockSize.

				//create a NEW BLOCK THERE.
				if(new_preferred_block_size >= slot_size) //at least there is space for one slot.
				{
					
					blk = createNewBlock(mFreeSpaceList[i], new_preferred_block_size, slot_size, blk_idx) ;
				}
				break ;
			} 
		}

		if(!blk)
		{
			for(U16 i = idx + 1 ; i < mPartitionLevels - 1; i++) //search the large slots 
			{
				if(mFreeSpaceList[i])
				{
					//create a NEW BLOCK THERE.
					blk = createNewBlock(mFreeSpaceList[i], preferred_block_size, slot_size, blk_idx) ;
					break ;
				} 
			}
		}
	}

	return blk ;
}

//create a new block at the designed location
LLPrivateMemoryPool::LLMemoryBlock* LLPrivateMemoryPool::LLMemoryChunk::createNewBlock(LLMemoryBlock* blk, U32 buffer_size, U32 slot_size, U32 blk_idx)
{
	//unlink from the free space
	removeFromFreeSpace(blk) ;

	//check the rest space
	U32 new_free_blk_size = blk->getBufferSize() - buffer_size ;	
	if(new_free_blk_size < mMinBlockSize) //can not partition the memory into size smaller than mMinBlockSize
	{
		new_free_blk_size = 0 ; //discard the last small extra space.
	}			

	//add the rest space back to the free list
	if(new_free_blk_size > 0) //blk still has free space
	{
		LLMemoryBlock* next_blk = blk + (buffer_size / mMinBlockSize) ;
		next_blk->setBuffer(blk->getBuffer() + buffer_size, new_free_blk_size) ;
		addToFreeSpace(next_blk) ;
	}

	blk->init(blk->getBuffer(), buffer_size, slot_size) ;
	//insert to the available block list...
	mAvailBlockList[blk_idx] = blk ;

	//mark the address map: all blocks covered by this block space pointing back to this block.
	U32 end = (buffer_size / mMinBlockSize) ;
	for(U32 i = 1 ; i < end ; i++)
	{
		(blk + i)->mSelf = blk ;
	}

	return blk ;
}

//delete a block, release the block to the free pool.
void LLPrivateMemoryPool::LLMemoryChunk::removeBlock(LLMemoryBlock* blk)
{
	//remove from the available block list
	if(blk->mPrev)
	{
		blk->mPrev->mNext = blk->mNext ;
	}
	if(blk->mNext)
	{
		blk->mNext->mPrev = blk->mPrev ;
	}
	U32 blk_idx = getBlockLevel(blk->getSlotSize());
	if(mAvailBlockList[blk_idx] == blk)
	{
		mAvailBlockList[blk_idx] = blk->mNext ;
	}

	blk->mNext = NULL ;
	blk->mPrev = NULL ;
	
	//mark it free
	blk->setBuffer(blk->getBuffer(), blk->getBufferSize()) ;

#if 1
	//merge blk with neighbors if possible
	if(blk->getBuffer() > mDataBuffer) //has the left neighbor
	{
		if((blk - 1)->mSelf->isFree())
		{
			LLMemoryBlock* left_blk = (blk - 1)->mSelf ;
			removeFromFreeSpace((blk - 1)->mSelf);
			left_blk->setBuffer(left_blk->getBuffer(), left_blk->getBufferSize() + blk->getBufferSize()) ;
			blk = left_blk ;
		}
	}
	if(blk->getBuffer() + blk->getBufferSize() <= mBuffer + mBufferSize - mMinBlockSize) //has the right neighbor
	{
		U32 d = blk->getBufferSize() / mMinBlockSize ;
		if((blk + d)->isFree())
		{
			LLMemoryBlock* right_blk = blk + d ;
			removeFromFreeSpace(blk + d) ;
			blk->setBuffer(blk->getBuffer(), blk->getBufferSize() + right_blk->getBufferSize()) ;
		}
	}
#endif
	
	addToFreeSpace(blk) ;

	return ;
}

//the top block in the list is full, pop it out of the list
void LLPrivateMemoryPool::LLMemoryChunk::popAvailBlockList(U32 blk_idx) 
{
	if(mAvailBlockList[blk_idx])
	{
		LLMemoryBlock* next = mAvailBlockList[blk_idx]->mNext ;
		if(next)
		{
			next->mPrev = NULL ;
		}
		mAvailBlockList[blk_idx]->mPrev = NULL ;
		mAvailBlockList[blk_idx]->mNext = NULL ;
		mAvailBlockList[blk_idx] = next ;
	}
}

//add the block back to the free pool
void LLPrivateMemoryPool::LLMemoryChunk::addToFreeSpace(LLMemoryBlock* blk) 
{
	llassert_always(!blk->mPrev) ;
	llassert_always(!blk->mNext) ;

	U16 free_idx = blk->getBufferSize() / mMinBlockSize - 1;

	(blk + free_idx)->mSelf = blk ; //mark the end pointing back to the head.
	free_idx = llmin(free_idx, (U16)(mPartitionLevels - 1)) ;

	blk->mNext = mFreeSpaceList[free_idx] ;
	if(mFreeSpaceList[free_idx])
	{
		mFreeSpaceList[free_idx]->mPrev = blk ;
	}
	mFreeSpaceList[free_idx] = blk ;
	blk->mPrev = NULL ;
	blk->mSelf = blk ;
	
	return ;
}

//remove the space from the free pool
void LLPrivateMemoryPool::LLMemoryChunk::removeFromFreeSpace(LLMemoryBlock* blk) 
{
	U16 free_idx = blk->getBufferSize() / mMinBlockSize - 1;
	free_idx = llmin(free_idx, (U16)(mPartitionLevels - 1)) ;

	if(mFreeSpaceList[free_idx] == blk)
	{
		mFreeSpaceList[free_idx] = blk->mNext ;
	}
	if(blk->mPrev)
	{
		blk->mPrev->mNext = blk->mNext ;
	}
	if(blk->mNext)
	{
		blk->mNext->mPrev = blk->mPrev ;
	}
	blk->mNext = NULL ;
	blk->mPrev = NULL ;
	blk->mSelf = NULL ;

	return ;
}

void LLPrivateMemoryPool::LLMemoryChunk::addToAvailBlockList(LLMemoryBlock* blk) 
{
	llassert_always(!blk->mPrev) ;
	llassert_always(!blk->mNext) ;

	U32 blk_idx = getBlockLevel(blk->getSlotSize());

	blk->mNext = mAvailBlockList[blk_idx] ;
	if(blk->mNext)
	{
		blk->mNext->mPrev = blk ;
	}
	blk->mPrev = NULL ;
	mAvailBlockList[blk_idx] = blk ;

	return ;
}

U32 LLPrivateMemoryPool::LLMemoryChunk::getPageIndex(U32 addr)
{
	return (addr - (U32)mDataBuffer) / mMinBlockSize ;
}

//for mAvailBlockList
U32 LLPrivateMemoryPool::LLMemoryChunk::getBlockLevel(U32 size)
{
	llassert(size >= mMinSlotSize && size <= mMaxSlotSize) ;

	//start from 0
	return (size + mMinSlotSize - 1) / mMinSlotSize - 1 ;
}

//for mFreeSpaceList
U16 LLPrivateMemoryPool::LLMemoryChunk::getPageLevel(U32 size)
{
	//start from 0
	U16 level = size / mMinBlockSize - 1 ;
	if(level >= mPartitionLevels)
	{
		level = mPartitionLevels - 1 ;
	}
	return level ;
}

//-------------------------------------------------------------------
//class LLPrivateMemoryPool
//--------------------------------------------------------------------
const U32 CHUNK_SIZE = 4 << 20 ; //4 MB
const U32 LARGE_CHUNK_SIZE = 4 * CHUNK_SIZE ; //16 MB
LLPrivateMemoryPool::LLPrivateMemoryPool(U32 max_size, bool threaded) :
	mMutexp(NULL),
	mMaxPoolSize(max_size),
	mReservedPoolSize(0),
	mHashFactor(1)
{
	if(threaded)
	{
		mMutexp = new LLMutex(NULL) ;
	}

	for(S32 i = 0 ; i < SUPER_ALLOCATION ; i++)
	{
		mChunkList[i] = NULL ;
	}	
	
	mNumOfChunks = 0 ;
}

LLPrivateMemoryPool::~LLPrivateMemoryPool()
{
	destroyPool();
	delete mMutexp ;
}

char* LLPrivateMemoryPool::allocate(U32 size)
{	
	if(!size)
	{
		return NULL ;
	}

	//if the asked size larger than MAX_BLOCK_SIZE, fetch from heap directly, the pool does not manage it
	if(size >= CHUNK_SIZE)
	{
		return new char[size] ;
	}

	char* p = NULL ;

	//find the appropriate chunk
	S32 chunk_idx = getChunkIndex(size) ;
	
	lock() ;

	LLMemoryChunk* chunk = mChunkList[chunk_idx];
	while(chunk)
	{
		if((p = chunk->allocate(size)))
		{
			break ;
		}
		chunk = chunk->mNext ;
	}
	
	//fetch new memory chunk
	if(!p)
	{
		if(mReservedPoolSize + CHUNK_SIZE > mMaxPoolSize)
		{
			chunk = mChunkList[chunk_idx];
			while(chunk)
			{
				if((p = chunk->allocate(size)))
				{
					break ;
				}
				chunk = chunk->mNext ;
			}
		}

		chunk = addChunk(chunk_idx) ;
		if(chunk)
		{
			p = chunk->allocate(size) ;
		}
	}

	unlock() ;

	return p ;
}

void LLPrivateMemoryPool::free(void* addr)
{
	if(!addr)
	{
		return ;
	}

	lock() ;
	
	LLMemoryChunk* chunk = findChunk((char*)addr) ;
	
	if(!chunk)
	{
		delete[] (char*)addr ; //release from heap
	}
	else
	{
		chunk->free(addr) ;

		if(chunk->empty())
		{
			removeChunk(chunk) ;
		}
	}
	
	unlock() ;
}

void LLPrivateMemoryPool::dump()
{
}

U32 LLPrivateMemoryPool::getTotalAllocatedSize()
{
	U32 total_allocated = 0 ;

	LLMemoryChunk* chunk ;
	for(S32 i = 0 ; i < SUPER_ALLOCATION ; i++)
	{
		chunk = mChunkList[i];
		while(chunk)
		{
			total_allocated += chunk->getAllocatedSize() ;
			chunk = chunk->mNext ;
		}
	}

	return total_allocated ;
}

void LLPrivateMemoryPool::lock()
{
	if(mMutexp)
	{
		mMutexp->lock() ;
	}
}

void LLPrivateMemoryPool::unlock()
{
	if(mMutexp)
	{
		mMutexp->unlock() ;
	}
}

S32  LLPrivateMemoryPool::getChunkIndex(U32 size) 
{
	S32 i ;
	for(i = 0 ; size > MAX_SLOT_SIZES[i]; i++);
	
	llassert_always(i < SUPER_ALLOCATION);

	return i ;
}

//destroy the entire pool
void  LLPrivateMemoryPool::destroyPool()
{
	lock() ;
	if(mNumOfChunks > 0)
	{
		for(U32 i = 0 ; i < mHashFactor; i++)
		{
			while(mChunkHashList[i])
			{
				removeChunk(mChunkHashList[i]) ;
			}
		}
	}
	mChunkHashList.clear() ;
	mHashFactor = 1 ;
	llassert_always(mNumOfChunks == 0) ;
	llassert_always(mReservedPoolSize == 0) ;

	for(S32 i = 0 ; i < SUPER_ALLOCATION ; i++)
	{
		mChunkList[i] = NULL ;
	}

	unlock() ;
}

void  LLPrivateMemoryPool::checkSize(U32 asked_size)
{
	if(mReservedPoolSize + asked_size > mMaxPoolSize)
	{
		llinfos << "Max pool size: " << mMaxPoolSize << llendl ;
		llinfos << "Total reserved size: " << mReservedPoolSize + asked_size << llendl ;
		llinfos << "Total_allocated Size: " << getTotalAllocatedSize() << llendl ;

		llerrs << "The pool is overflowing..." << llendl ;
	}
}

LLPrivateMemoryPool::LLMemoryChunk* LLPrivateMemoryPool::addChunk(S32 chunk_index)
{
	U32 preferred_size ;
	U32 overhead ;
	if(chunk_index < LARGE_ALLOCATION)
	{
		preferred_size = CHUNK_SIZE ; //4MB
		overhead = LLMemoryChunk::getMaxOverhead(preferred_size, MIN_SLOT_SIZES[chunk_index],
			MAX_SLOT_SIZES[chunk_index], MIN_BLOCK_SIZES[chunk_index], MAX_BLOCK_SIZES[chunk_index]) ;
	}
	else
	{
		preferred_size = LARGE_CHUNK_SIZE ; //16MB
		overhead = LLMemoryChunk::getMaxOverhead(preferred_size, MIN_SLOT_SIZES[chunk_index], 
			MAX_SLOT_SIZES[chunk_index], MIN_BLOCK_SIZES[chunk_index], MAX_BLOCK_SIZES[chunk_index]) ;
	}

	checkSize(preferred_size + overhead) ;
	mReservedPoolSize += preferred_size + overhead ;

	char* buffer = new(std::nothrow) char[preferred_size + overhead] ;
	if(!buffer)
	{
		return NULL ;
	}
	
	LLMemoryChunk* chunk = new (buffer) LLMemoryChunk() ;
	chunk->init(buffer, preferred_size + overhead, MIN_SLOT_SIZES[chunk_index],
		MAX_SLOT_SIZES[chunk_index], MIN_BLOCK_SIZES[chunk_index], MAX_BLOCK_SIZES[chunk_index]) ;

	//add to the tail of the linked list
	{
		if(!mChunkList[chunk_index])
		{
			mChunkList[chunk_index] = chunk ;
		}
		else
		{
			LLMemoryChunk* cur = mChunkList[chunk_index] ;
			while(cur->mNext)
			{
				cur = cur->mNext ;
			}
			cur->mNext = chunk ;
			chunk->mPrev = cur ;
		}
	}

	//insert into the hash table
	addToHashTable(chunk) ;
	
	mNumOfChunks++;

	return chunk ;
}

void LLPrivateMemoryPool::removeChunk(LLMemoryChunk* chunk) 
{
	if(!chunk)
	{
		return ;
	}

	//remove from the linked list
	for(S32 i = 0 ; i < SUPER_ALLOCATION ; i++)
	{
		if(mChunkList[i] == chunk)
		{
			mChunkList[i] = chunk->mNext ;
		}
	}

	if(chunk->mPrev)
	{
		chunk->mPrev->mNext = chunk->mNext ;
	}
	if(chunk->mNext)
	{
		chunk->mNext->mPrev = chunk->mPrev ;
	}

	//remove from the hash table
	removeFromHashTable(chunk) ;
	
	mNumOfChunks--;
	mReservedPoolSize -= chunk->getBufferSize() ;
	
	//release memory
	delete[] chunk->getBuffer() ;
}

U16 LLPrivateMemoryPool::findHashKey(const char* addr)
{
	return (((U32)addr) / CHUNK_SIZE) % mHashFactor ;
}

LLPrivateMemoryPool::LLMemoryChunk* LLPrivateMemoryPool::findChunk(const char* addr)
{
	U16 key = findHashKey(addr) ;	
	if(mChunkHashList.size() <= key)
	{
		return NULL ;
	}

	//check the hash value "key"
	LLMemoryChunk* chunk = mChunkHashList[key] ;
	while(chunk && !chunk->containsAddress(addr))
	{
		chunk = chunk->mHashNext ;
	}

	return chunk ;
}

void LLPrivateMemoryPool::addToHashTable(LLMemoryChunk* chunk) 
{
	static const U16 HASH_FACTORS[] = {41, 83, 193, 317, 419, 523, 0xFFFF}; 
	
	U16 i ;
	if(mChunkHashList.empty())
	{
		mHashFactor = HASH_FACTORS[0] ;
		rehash() ;		
	}

	U16 start_key = findHashKey(chunk->getBuffer()) ;
	U16 end_key = findHashKey(chunk->getBuffer() + chunk->getBufferSize() - 1) ;
	bool need_rehash = false ;
	
	if(mChunkHashList[start_key])
	{
		if(mChunkHashList[start_key] == chunk) 
		{
			return; //already inserted.
		}
		
		llassert_always(!chunk->mHashNext) ;

		chunk->mHashNext = mChunkHashList[start_key] ;
		mChunkHashList[start_key] = chunk ;
	}
	else
	{
		mChunkHashList[start_key] = chunk ;
	}

	if(!need_rehash)
	{
		if(mChunkHashList[end_key])
		{
			llassert_always(mChunkHashList[end_key] != chunk)
			
			need_rehash =  mChunkHashList[end_key]->mHashNext != NULL || mChunkHashList[end_key] == chunk->mHashNext;
			if(!need_rehash)
			{
				mChunkHashList[end_key]->mHashNext = chunk ;
			}			
		}
		else
		{
			mChunkHashList[end_key] = chunk ;
		}
	}

	if(!need_rehash)
	{
		if(end_key < start_key)
		{
			need_rehash = fillHashTable(start_key + 1, mHashFactor, chunk) ;
			if(!need_rehash)
			{
				need_rehash = fillHashTable(0, end_key, chunk) ;
			}
		}
		else
		{
			need_rehash = fillHashTable(start_key + 1, end_key, chunk) ;
		}
	}
	
	if(need_rehash)
	{
		i = 0 ;
		while(HASH_FACTORS[i] <= mHashFactor) i++;

		mHashFactor = HASH_FACTORS[i] ;
		llassert_always(mHashFactor != 0xFFFF) ;//stop point to prevent endlessly recursive calls

		rehash() ;
	}
}

void LLPrivateMemoryPool::removeFromHashTable(LLMemoryChunk* chunk) 
{
	U16 start_key = findHashKey(chunk->getBuffer()) ;
	U16 end_key = findHashKey(chunk->getBuffer() + chunk->getBufferSize() - 1) ;
	
	mChunkHashList[start_key] = chunk->mHashNext ;
	chunk->mHashNext = NULL ;

	if(mChunkHashList[end_key] != chunk)
	{
		mChunkHashList[end_key]->mHashNext = NULL ;
	}
	else
	{
		mChunkHashList[end_key] = NULL ;
	}

	if(end_key < start_key)
	{
		for(U16 i = start_key + 1 ; i < mHashFactor; i++)
		{
			mChunkHashList[i] = NULL ;
		}
		for(U16 i = 0 ; i < end_key; i++)
		{
			mChunkHashList[i] = NULL ;
		}
	}
	else
	{
		for(U16 i = start_key + 1 ; i < end_key; i++)
		{
			mChunkHashList[i] = NULL ;
		}
	}
}

void LLPrivateMemoryPool::rehash()
{
	llinfos << "new hash factor: " << mHashFactor << llendl ;

	mChunkHashList.clear() ;
	mChunkHashList.resize(mHashFactor, NULL) ;

	LLMemoryChunk* chunk ;
	for(U16 i = 0 ; i < SUPER_ALLOCATION ; i++)
	{
		chunk = mChunkList[i] ; 
		while(chunk)
		{
			chunk->mHashNext = NULL ;
			addToHashTable(chunk) ;
			chunk = chunk->mNext ;
		}
	}
}

bool LLPrivateMemoryPool::fillHashTable(U16 start, U16 end, LLMemoryChunk* chunk)
{
	for(U16 i = start; i < end; i++)
	{
		if(mChunkHashList[i]) //the slot is occupied.
		{
			llassert_always(mChunkHashList[i] != chunk) ;
			return true ;
		}
		else
		{
			mChunkHashList[i] = chunk ;
		}
	}

	return false ;
}

//--------------------------------------------------------------------
//class LLPrivateMemoryPoolManager
//--------------------------------------------------------------------
LLPrivateMemoryPoolManager* LLPrivateMemoryPoolManager::sInstance = NULL ;

LLPrivateMemoryPoolManager::LLPrivateMemoryPoolManager() 
{
}

LLPrivateMemoryPoolManager::~LLPrivateMemoryPoolManager() 
{
	//all private pools should be released by their owners before reaching here.
	llassert_always(mPoolList.empty()) ;

#if 0
	if(!mPoolList.empty())
	{
		for(std::set<LLPrivateMemoryPool*>::iterator iter = mPoolList.begin(); iter != mPoolList.end(); ++iter)
		{
			delete *iter;
		}
		mPoolList.clear() ;
	}
#endif
}

//static 
LLPrivateMemoryPoolManager* LLPrivateMemoryPoolManager::getInstance() 
{
	if(!sInstance)
	{
		sInstance = new LLPrivateMemoryPoolManager() ;
	}
	return sInstance ;
}
	
//static 
void LLPrivateMemoryPoolManager::destroyClass() 
{
	if(sInstance)
	{
		delete sInstance ;
		sInstance = NULL ;
	}
}

LLPrivateMemoryPool* LLPrivateMemoryPoolManager::newPool(U32 max_size, bool threaded) 
{
	LLPrivateMemoryPool* pool = new LLPrivateMemoryPool(max_size, threaded) ;
	mPoolList.insert(pool) ;

	return pool ;
}

void LLPrivateMemoryPoolManager::deletePool(LLPrivateMemoryPool* pool) 
{
	mPoolList.erase(pool) ;
	delete pool;
}

//debug
void LLPrivateMemoryPoolManager::updateStatistics()
{
	mTotalReservedSize = 0 ;
	mTotalAllocatedSize = 0 ;

	for(std::set<LLPrivateMemoryPool*>::iterator iter = mPoolList.begin(); iter != mPoolList.end(); ++iter)
	{
		mTotalReservedSize += (*iter)->getTotalReservedSize() ;
		mTotalAllocatedSize += (*iter)->getTotalAllocatedSize() ;
	}
}

//--------------------------------------------------------------------
//class LLPrivateMemoryPoolTester
//--------------------------------------------------------------------
LLPrivateMemoryPoolTester* LLPrivateMemoryPoolTester::sInstance = NULL ;
LLPrivateMemoryPool* LLPrivateMemoryPoolTester::sPool = NULL ;
LLPrivateMemoryPoolTester::LLPrivateMemoryPoolTester()
{	
}
	
LLPrivateMemoryPoolTester::~LLPrivateMemoryPoolTester() 
{	
}

//static 
LLPrivateMemoryPoolTester* LLPrivateMemoryPoolTester::getInstance() 
{
	if(!sInstance)
	{
		sInstance = ::new LLPrivateMemoryPoolTester() ;
	}
	return sInstance ;
}

//static 
void LLPrivateMemoryPoolTester::destroy()
{
	if(sInstance)
	{
		::delete sInstance ;
		sInstance = NULL ;
	}

	if(sPool)
	{
		LLPrivateMemoryPoolManager::getInstance()->deletePool(sPool) ;
		sPool = NULL ;
	}
}

void LLPrivateMemoryPoolTester::run(bool threaded) 
{
	const U32 max_pool_size = 1024 << 20 ;
	
	if(sPool)
	{
		LLPrivateMemoryPoolManager::getInstance()->deletePool(sPool) ;
	}
	sPool = LLPrivateMemoryPoolManager::getInstance()->newPool(max_pool_size, threaded) ;

	//run the test
	correctnessTest() ;
	performanceTest() ;
	//fragmentationtest() ;

	//release pool.
	LLPrivateMemoryPoolManager::getInstance()->deletePool(sPool) ;
	sPool = NULL ;
}

void LLPrivateMemoryPoolTester::test(U32 min_size, U32 max_size, U32 stride, U32 times, 
									 bool random_deletion, bool output_statistics)
{
	U32 levels = (max_size - min_size) / stride + 1 ;
	char*** p ;
	U32 i, j ;
	U32 total_allocated_size = 0 ;

	//allocate space for p ;
	if(!(p = ::new char**[times]) || !(*p = ::new char*[times * levels]))
	{
		llerrs << "memory initialization for p failed" << llendl ;
	}

	//init
	for(i = 0 ; i < times; i++)
	{
		p[i] = *p + i * levels ;
		for(j = 0 ; j < levels; j++)
		{
			p[i][j] = NULL ;
		}
	}

	//allocation
	U32 size ;
	for(i = 0 ; i < times ; i++)
	{
		for(j = 0 ; j < levels; j++) 
		{
			size = min_size + j * stride ;
			p[i][j] = sPool->allocate(size) ;

			total_allocated_size+= size ;

			*(U32*)p[i][j] = i ;
			*((U32*)p[i][j] + 1) = j ;
			//p[i][j][size - 1] = '\0' ; //access the last element to verify the success of the allocation.

			//randomly release memory
			if(random_deletion)
			{
				S32 k = rand() % levels ;

				if(p[i][k])
				{
					llassert_always(*(U32*)p[i][k] == i && *((U32*)p[i][k] + 1) == k) ;
					sPool->free(p[i][k]) ;
					total_allocated_size -= min_size + k * stride ;
					p[i][k] = NULL ;
				}
			}
		}
	}

	//output pool allocation statistics
	if(output_statistics)
	{
	}

	//release all memory allocations
	for(i = 0 ; i < times; i++)
	{
		for(j = 0 ; j < levels; j++)
		{
			if(p[i][j])
			{
				llassert_always(*(U32*)p[i][j] == i && *((U32*)p[i][j] + 1) == j) ;
				sPool->free(p[i][j]) ;
				total_allocated_size -= min_size + j * stride ;
				p[i][j] = NULL ;
			}
		}
	}

	::delete[] *p ;
	::delete[] p ;
}

void LLPrivateMemoryPoolTester::testAndTime(U32 size, U32 times)
{
	LLTimer timer ;

	llinfos << " -**********************- " << llendl ;
	llinfos << "test size: " << size << " test times: " << times << llendl ;

	timer.reset() ;
	char** p = new char*[times] ;
		
	//using the customized memory pool
	//allocation
	for(U32 i = 0 ; i < times; i++)
	{
		p[i] = sPool->allocate(size) ;
		if(!p[i])
		{
			llerrs << "allocation failed" << llendl ;
		}
	}
	//de-allocation
	for(U32 i = 0 ; i < times; i++)
	{
		sPool->free(p[i]) ;
		p[i] = NULL ;
	}
	llinfos << "time spent using customized memory pool: " << timer.getElapsedTimeF32() << llendl ;

	timer.reset() ;

	//using the standard allocator/de-allocator:
	//allocation
	for(U32 i = 0 ; i < times; i++)
	{
		p[i] = ::new char[size] ;
		if(!p[i])
		{
			llerrs << "allocation failed" << llendl ;
		}
	}
	//de-allocation
	for(U32 i = 0 ; i < times; i++)
	{
		::delete[] p[i] ;
		p[i] = NULL ;
	}
	llinfos << "time spent using standard allocator/de-allocator: " << timer.getElapsedTimeF32() << llendl ;

	delete[] p;
}

void LLPrivateMemoryPoolTester::correctnessTest() 
{
	//try many different sized allocation, and all kinds of edge cases, access the allocated memory 
	//to see if allocation is right.
	
	//edge case
	char* p = sPool->allocate(0) ;
	sPool->free(p) ;

	//small sized
	// [8 bytes, 2KB), each asks for 256 allocations and deallocations
	test(8, 2040, 8, 256, true, true) ;
	
	//medium sized
	//[2KB, 512KB), each asks for 16 allocations and deallocations
	test(2048, 512 * 1024 - 2048, 2048, 16, true, true) ;

	//large sized
	//[512KB, 4MB], each asks for 8 allocations and deallocations
	test(512 * 1024, 4 * 1024 * 1024, 64 * 1024, 6, true, true) ;
}

void LLPrivateMemoryPoolTester::performanceTest() 
{
	U32 test_size[3] = {768, 3* 1024, 3* 1024 * 1024};
	
	//small sized
	testAndTime(test_size[0], 8) ;
	
	//medium sized
	testAndTime(test_size[1], 8) ;

	//large sized
	testAndTime(test_size[2], 8) ;
}

void LLPrivateMemoryPoolTester::fragmentationtest() 
{
	//for internal fragmentation statistics:
	//every time when asking for a new chunk during correctness test, and performance test,
	//print out the chunk usage statistices.
}

//--------------------------------------------------------------------
