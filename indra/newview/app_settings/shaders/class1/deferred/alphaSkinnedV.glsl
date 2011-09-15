/** 
 * @file alphaSkinnedV.glsl
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

uniform mat4 projection_matrix;
uniform mat4 modelview_matrix;

attribute vec3 position;
attribute vec3 normal;
attribute vec4 diffuse_color;
attribute vec2 texcoord0;

vec4 calcLighting(vec3 pos, vec3 norm, vec4 color, vec4 baseCol);
mat4 getObjectSkinnedTransform();
void calcAtmospherics(vec3 inPositionEye);

float calcDirectionalLight(vec3 n, vec3 l);

vec3 atmosAmbient(vec3 light);
vec3 atmosAffectDirectionalLight(float lightIntensity);
vec3 scaleDownLight(vec3 light);
vec3 scaleUpLight(vec3 light);

varying vec3 vary_position;
varying vec3 vary_ambient;
varying vec3 vary_directional;
varying vec3 vary_normal;
varying vec3 vary_fragcoord;
varying vec3 vary_pointlight_col;

uniform float near_clip;

float calcPointLightOrSpotLight(vec3 v, vec3 n, vec4 lp, vec3 ln, float la, float fa, float is_pointlight)
{
	//get light vector
	vec3 lv = lp.xyz-v;
	
	//get distance
	float d = length(lv);
	
	float da = 0.0;

	if (d > 0.0 && la > 0.0 && fa > 0.0)
	{
		//normalize light vector
		lv *= 1.0/d;
	
		//distance attenuation
		float dist2 = d*d/(la*la);
		da = clamp(1.0-(dist2-1.0*(1.0-fa))/fa, 0.0, 1.0);

		// spotlight coefficient.
		float spot = max(dot(-ln, lv), is_pointlight);
		da *= spot*spot; // GL_SPOT_EXPONENT=2

		//angular attenuation
		da *= calcDirectionalLight(n, lv);
	}

	return da;	
}

void main()
{
	gl_TexCoord[0] = vec4(texcoord0,0,1);
				
	vec4 pos;
	vec3 norm;
	
	mat4 trans = getObjectSkinnedTransform();
	trans = modelview_matrix * trans;
	
	pos = trans * vec4(position.xyz, 1.0);
	
	norm = position.xyz + normal.xyz;
	norm = normalize(( trans*vec4(norojrm, 1.0) ).xyz-pos.xyz);
	
	vec4 frag_pos = projection_matrix * pos;
	gl_Position = frag_pos;
	
	vary_position = pos.xyz;
	vary_normal = norm;	
	
	calcAtmospherics(pos.xyz);

	vec4 col = vec4(0.0, 0.0, 0.0, diffuse_color.a);

	// Collect normal lights
	col.rgb += gl_LightSource[2].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[2].position, gl_LightSource[2].spotDirection.xyz, gl_LightSource[2].linearAttenuation, gl_LightSource[2].quadraticAttenuation, gl_LightSource[2].specular.a);
	col.rgb += gl_LightSource[3].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[3].position, gl_LightSource[3].spotDirection.xyz, gl_LightSource[3].linearAttenuation, gl_LightSource[3].quadraticAttenuation ,gl_LightSource[3].specular.a);
	col.rgb += gl_LightSource[4].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[4].position, gl_LightSource[4].spotDirection.xyz, gl_LightSource[4].linearAttenuation, gl_LightSource[4].quadraticAttenuation, gl_LightSource[4].specular.a);
	col.rgb += gl_LightSource[5].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[5].position, gl_LightSource[5].spotDirection.xyz, gl_LightSource[5].linearAttenuation, gl_LightSource[5].quadraticAttenuation, gl_LightSource[5].specular.a);
	col.rgb += gl_LightSource[6].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[6].position, gl_LightSource[6].spotDirection.xyz, gl_LightSource[6].linearAttenuation, gl_LightSource[6].quadraticAttenuation, gl_LightSource[6].specular.a);
	col.rgb += gl_LightSource[7].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[7].position, gl_LightSource[7].spotDirection.xyz, gl_LightSource[7].linearAttenuation, gl_LightSource[7].quadraticAttenuation, gl_LightSource[7].specular.a);
	
	vary_pointlight_col = col.rgb*diffuse_color.rgb;

	col.rgb = vec3(0,0,0);

	// Add windlight lights
	col.rgb = atmosAmbient(vec3(0.));
	
	vary_ambient = col.rgb*diffuse_color.rgb;
	vary_directional = diffuse_color.rgb*atmosAffectDirectionalLight(max(calcDirectionalLight(norm, gl_LightSource[0].position.xyz), (1.0-diffuse_color.a)*(1.0-diffuse_color.a)));
	
	col.rgb = min(col.rgb*diffuse_color.rgb, 1.0);
	
	gl_FrontColor = col;

	gl_FogFragCoord = pos.z;
	
	vary_fragcoord.xyz = frag_pos.xyz + vec3(0,0,near_clip);
}


