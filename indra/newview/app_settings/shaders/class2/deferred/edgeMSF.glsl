/** 
 * @file edgeF.glsl
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
 
#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_texture_multisample : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 gl_FragColor;
#endif

uniform sampler2DMS depthMap;
uniform sampler2DMS normalMap;

VARYING vec2 vary_fragcoord;

uniform float depth_cutoff;
uniform float norm_cutoff;

uniform mat4 inv_proj;
uniform vec2 screen_res;

float getDepth(ivec2 pos_screen, int sample)
{
	float z = texelFetch(depthMap, pos_screen, sample).r;
	z = z*2.0-1.0;
	vec4 ndc = vec4(0.0, 0.0, z, 1.0);
	vec4 p = inv_proj*ndc;
	return p.z/p.w;
}

void main() 
{
	float e = 0;
	
	ivec2 itc = ivec2(vary_fragcoord.xy);

	for (int i = 0; i < samples; i++)
	{	
		vec3 norm = texelFetch(normalMap, itc, i).xyz;
		norm = vec3((norm.xy-0.5)*2.0,norm.z); // unpack norm
		float depth = getDepth(itc, i);
	
		vec2 tc = vary_fragcoord.xy;
	
		int sc = 1;
	
		vec2 de;
		de.x = (depth-getDepth(itc+ivec2(sc, sc),i)) + (depth-getDepth(itc+ivec2(-sc, -sc), i));
		de.y = (depth-getDepth(itc+ivec2(-sc, sc),i)) + (depth-getDepth(itc+ivec2(sc, -sc), i));
		de /= depth;
		de *= de;
		de = step(depth_cutoff, de);
	
		vec2 ne;
		vec3 nexnorm = texelFetch(normalMap, itc+ivec2(-sc,-sc), i).rgb;
		nexnorm = vec3((nexnorm.xy-0.5)*2.0,nexnorm.z); // unpack norm
		ne.x = dot(nexnorm, norm);
		vec3 neynorm = texelFetch(normalMap, itc+ivec2(sc,sc), i).rgb;
		neynorm = vec3((neynorm.xy-0.5)*2.0,neynorm.z); // unpack norm
		ne.y = dot(neynorm, norm);
	
		ne = 1.0-ne;
	
		ne = step(norm_cutoff, ne);

		e += dot(de,de)+dot(ne,ne);
	}

	e /= samples;
	
	gl_FragColor.a = e;
}
