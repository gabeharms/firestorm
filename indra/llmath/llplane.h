/** 
 * @file llplane.h
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

#ifndef LL_LLPLANE_H
#define LL_LLPLANE_H

#include "v3math.h"
#include "v4math.h"

// A simple way to specify a plane is to give its normal,
// and it's nearest approach to the origin.
// 
// Given the equation for a plane : A*x + B*y + C*z + D = 0
// The plane normal = [A, B, C]
// The closest approach = D / sqrt(A*A + B*B + C*C)

class LLPlane : public LLVector4
{
public:
	LLPlane() {}; // no default constructor
	LLPlane(const LLVector3 &p0, F32 d) { setVec(p0, d); }
	LLPlane(const LLVector3 &p0, const LLVector3 &n) { setVec(p0, n); }
	void setVec(const LLVector3 &p0, F32 d) { LLVector4::setVec(p0[0], p0[1], p0[2], d); }
	void setVec(const LLVector3 &p0, const LLVector3 &n)
	{
		F32 d = -(p0 * n);
		setVec(n, d);
	}
	void setVec(const LLVector3 &p0, const LLVector3 &p1, const LLVector3 &p2)
	{
		LLVector3 u, v, w;
		u = p1 - p0;
		v = p2 - p0;
		w = u % v;
		w.normVec();
		F32 d = -(w * p0);
		setVec(w, d);
	}
	LLPlane& operator=(const LLVector4& v2) {  LLVector4::setVec(v2[0],v2[1],v2[2],v2[3]); return *this;}
	F32 dist(const LLVector3 &v2) const { return mV[0]*v2[0] + mV[1]*v2[1] + mV[2]*v2[2] + mV[3]; }
};



#endif // LL_LLPLANE_H
