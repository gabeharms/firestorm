/** 
 * @file bumpV.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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
 


varying vec3 vary_mat0;
varying vec3 vary_mat1;
varying vec3 vary_mat2;

void main()
{
	//transform vertex
	gl_Position = ftransform(); 
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
	
	vec3 n = normalize(gl_NormalMatrix * gl_Normal);
	vec3 b = normalize(gl_NormalMatrix * gl_MultiTexCoord2.xyz);
	vec3 t = cross(b, n);
	
	vary_mat0 = vec3(t.x, b.x, n.x);
	vary_mat1 = vec3(t.y, b.y, n.y);
	vary_mat2 = vec3(t.z, b.z, n.z);
	
	gl_FrontColor = gl_Color;
}
