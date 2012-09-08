/** 
 * @file llviewerjointmesh.h
 * @brief Implementation of LLViewerJointMesh class
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

#ifndef LL_LLVIEWERJOINTMESH_H
#define LL_LLVIEWERJOINTMESH_H

#include "llviewerjoint.h"
#include "llviewertexture.h"
#include "llavatarjointmesh.h"
#include "llpolymesh.h"
#include "v4color.h"

class LLDrawable;
class LLFace;
class LLCharacter;
class LLViewerTexLayerSet;

//-----------------------------------------------------------------------------
// class LLViewerJointMesh
//-----------------------------------------------------------------------------
class LLViewerJointMesh : public LLAvatarJointMesh
{
	friend class LLVOAvatar;
public:
	// Constructor
	LLViewerJointMesh();

	// Destructor
	virtual ~LLViewerJointMesh();

	// Render time method to upload batches of joint matrices
	void uploadJointMatrices();

	// overloaded from base class
	/*virtual*/ void drawBone();
	/*virtual*/ BOOL isTransparent();
	/*virtual*/ U32 drawShape( F32 pixelArea, BOOL first_pass, BOOL is_dummy );

	/*virtual*/ void updateFaceSizes(U32 &num_vertices, U32& num_indices, F32 pixel_area);
	/*virtual*/ void updateFaceData(LLFace *face, F32 pixel_area, BOOL damp_wind = FALSE, bool terse_update = false);
	/*virtual*/ BOOL updateLOD(F32 pixel_area, BOOL activate);
	/*virtual*/ void updateJointGeometry();
	/*virtual*/ void dump();

	void setIsTransparent(BOOL is_transparent) { mIsTransparent = is_transparent; }

	/*virtual*/ BOOL isAnimatable() const { return FALSE; }
	
private:

	//copy mesh into given face's vertex buffer, applying current animation pose
	static void updateGeometry(LLFace* face, LLPolyMesh* mesh);
};

#endif // LL_LLVIEWERJOINTMESH_H
