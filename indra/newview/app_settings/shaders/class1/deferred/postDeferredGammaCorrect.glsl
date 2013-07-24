/** 
 * @file postDeferredGammaCorrect.glsl
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

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRect diffuseRect;

uniform vec2 screen_res;
VARYING vec2 vary_fragcoord;

uniform float display_gamma;

vec3 linear_to_srgb(vec3 cl)
{
	    /*{  0.0,                          0         <= cl
            {  12.92 * c,                    0         <  cl < 0.0031308
    cs = {  1.055 * cl^0.41666 - 0.055,   0.0031308 <= cl < 1
            {  1.0,                                       cl >= 1*/

	cl = clamp(cl, vec3(0), vec3(1));

	if ((cl.r+cl.g+cl.b) < 0.0031308)
		return 12.92 * cl;
	return 1.055 * pow(cl, vec3(0.41666)) - 0.055;
}

void main() 
{
	vec4 diff = texture2DRect(diffuseRect, vary_fragcoord);
	diff.rgb = pow(diff.rgb,vec3(display_gamma));
	frag_color = diff;
}

