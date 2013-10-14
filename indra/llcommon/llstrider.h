/** 
 * @file llstrider.h
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

#ifndef LL_LLSTRIDER_H
#define LL_LLSTRIDER_H

#include "stdtypes.h"

template <class Object> class LLStrider
{
	union
	{
		Object* mObjectp;
		U8*		mBytep;
	};
	U32     mSkip;
public:
	~LLStrider() { } 

	void setStride (S32 skipBytes)	{ mSkip = (skipBytes ? skipBytes : sizeof(Object));}

    void skip(const U32 index)     { mBytep += mSkip*index;}
    U32 getSkip() const            { return mSkip; }

#ifndef OPENSIM // <FS:ND> protect against buffer overflows, but only for non HAvok builds. Otherwise changing the object size plays really foul when used in the binary Havok blob
	LLStrider()  { mObjectp = NULL; mSkip = sizeof(Object); } 
	const LLStrider<Object>& operator =  (Object *first)    { mObjectp = first; return *this;}

	LLStrider<Object> operator+(const S32& index) 
	{
		LLStrider<Object> ret;
		ret.mBytep = mBytep + mSkip*index;
		ret.mSkip = mSkip;
		return ret;
	}

	Object* get()                  { return mObjectp; }
	Object const* get() const      { return mObjectp; }	// <FS:CR>
    Object* operator->()           { return mObjectp; }
    Object& operator *()           { return *mObjectp; }
    Object* operator ++(int)       { Object* old = mObjectp; mBytep += mSkip; return old; }
    Object* operator +=(int i)     { mBytep += mSkip*i; return mObjectp; }

    Object& operator[](U32 index)  { return *(Object*)(mBytep + (mSkip * index)); }
#else
	LLStrider()  { mObjectp = NULL; mSkip = sizeof(Object); mBufferEnd = 0; } 
	const LLStrider<Object>& operator =  (Object *first)    { mObjectp = first; mBufferEnd = 0; return *this;}

	LLStrider<Object> operator+(const S32& index) 
	{
		LLStrider<Object> ret;
		ret.mBytep = mBytep + mSkip*index;
		ret.mSkip = mSkip;
		ret.mBufferEnd = mBufferEnd;

		return ret;
	}

	Object* get()
	{
		if( !assertValid( mBytep ) )
			return &mDummy;

		return mObjectp;
	}

	Object const* get() const
	{
		return mObjectp;
	}

	Object* operator->()
	{
		if( !assertValid( mBytep ) )
			return &mDummy;

		return mObjectp;
	}

	Object& operator *()
	{
		if( !assertValid( mBytep ) )
			return mDummy;

		return *mObjectp;
	}

	Object* operator ++(int)
	{
		Object* old = mObjectp;
		mBytep += mSkip;

		if( !assertValid( (U8*)old ) )
			return &mDummy;

		return old;
	}

	Object* operator +=(int i)
	{
		mBytep += mSkip*i;
		assertValid( mBytep );
		return mObjectp;
	}

	Object& operator[](U32 index)
	{
		if( !assertValid( mBytep + mSkip*index ) )
			return mDummy;

		return *(Object*)(mBytep + (mSkip * index));
	}

	void setCount( U32 aCount )
	{
		mBufferEnd = mBytep + mSkip*aCount;
#if LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
		mCount = aCount;
#endif
	}

	bool assertValid( U8 const *aBuffer )
	{
		if( !aBuffer || !mBufferEnd )
			return true;
		if( aBuffer < mBufferEnd )
			return true;

		llerrs << "Vertex buffer access beyond end of VBO" << llendl;
		return false;
	}

private:
	U8 *mBufferEnd;

#if LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
	U32 mCount;
#endif

	Object mDummy;
#endif
};

#endif // LL_LLSTRIDER_H
