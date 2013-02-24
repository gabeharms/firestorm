/** 
 * @file softenLightF.glsl
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
uniform sampler2DRect specularRect;
uniform sampler2DRect normalMap;
uniform sampler2DRect lightMap;
uniform sampler2DRect depthMap;
uniform samplerCube environmentMap;
uniform sampler2D	  lightFunc;
uniform vec3 gi_quad;

uniform float blur_size;
uniform float blur_fidelity;

// Inputs
uniform vec4 morphFactor;
uniform vec3 camPosLocal;
//uniform vec4 camPosWorld;
uniform vec4 gamma;
uniform vec4 lightnorm;
uniform vec4 sunlight_color;
uniform vec4 ambient;
uniform vec4 blue_horizon;
uniform vec4 blue_density;
uniform float haze_horizon;
uniform float haze_density;
uniform float cloud_shadow;
uniform float density_multiplier;
uniform float distance_multiplier;
uniform float max_y;
uniform vec4 glow;
uniform float scene_light_strength;
uniform mat3 env_mat;
uniform vec4 shadow_clip;
uniform float ssao_effect;

uniform mat4 inv_proj;
uniform vec2 screen_res;

uniform vec3 sun_dir;

VARYING vec2 vary_fragcoord;

vec3 vary_PositionEye;

vec3 vary_SunlitColor;
vec3 vary_AmblitColor;
vec3 vary_AdditiveColor;
vec3 vary_AtmosAttenuation;

vec4 getPosition_d(vec2 pos_screen, float depth)
{
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

vec4 getPosition(vec2 pos_screen)
{ //get position in screen space (world units) given window coordinate and depth map
	float depth = texture2DRect(depthMap, pos_screen.xy).r;
	return getPosition_d(pos_screen, depth);
}

vec3 getPositionEye()
{
	return vary_PositionEye;
}
vec3 getSunlitColor()
{
	return vary_SunlitColor;
}
vec3 getAmblitColor()
{
	return vary_AmblitColor;
}
vec3 getAdditiveColor()
{
	return vary_AdditiveColor;
}
vec3 getAtmosAttenuation()
{
	return vary_AtmosAttenuation;
}


void setPositionEye(vec3 v)
{
	vary_PositionEye = v;
}

void setSunlitColor(vec3 v)
{
	vary_SunlitColor = v;
}

void setAmblitColor(vec3 v)
{
	vary_AmblitColor = v;
}

void setAdditiveColor(vec3 v)
{
	vary_AdditiveColor = v;
}

void setAtmosAttenuation(vec3 v)
{
	vary_AtmosAttenuation = v;
}

void calcAtmospherics(vec3 inPositionEye, float ambFactor) {

	vec3 P = inPositionEye;
	setPositionEye(P);
	
	vec3 tmpLightnorm = lightnorm.xyz;

	vec3 Pn = normalize(P);
	float Plen = length(P);

	vec4 temp1 = vec4(0);
	vec3 temp2 = vec3(0);
	vec4 blue_weight;
	vec4 haze_weight;
	vec4 sunlight = sunlight_color;
	vec4 light_atten;

	//sunlight attenuation effect (hue and brightness) due to atmosphere
	//this is used later for sunlight modulation at various altitudes
	light_atten = (blue_density + vec4(haze_density * 0.25)) * (density_multiplier * max_y);
		//I had thought blue_density and haze_density should have equal weighting,
		//but attenuation due to haze_density tends to seem too strong

	temp1 = blue_density + vec4(haze_density);
	blue_weight = blue_density / temp1;
	haze_weight = vec4(haze_density) / temp1;

	//(TERRAIN) compute sunlight from lightnorm only (for short rays like terrain)
	temp2.y = max(0.0, tmpLightnorm.y);
	temp2.y = 1. / temp2.y;
	sunlight *= exp( - light_atten * temp2.y);

	// main atmospheric scattering line integral
	temp2.z = Plen * density_multiplier;

	// Transparency (-> temp1)
	// ATI Bugfix -- can't store temp1*temp2.z*distance_multiplier in a variable because the ati
	// compiler gets confused.
	temp1 = exp(-temp1 * temp2.z * distance_multiplier);

	//final atmosphere attenuation factor
	setAtmosAttenuation(temp1.rgb);
	
	//compute haze glow
	//(can use temp2.x as temp because we haven't used it yet)
	temp2.x = dot(Pn, tmpLightnorm.xyz);
	temp2.x = 1. - temp2.x;
		//temp2.x is 0 at the sun and increases away from sun
	temp2.x = max(temp2.x, .03);	//was glow.y
		//set a minimum "angle" (smaller glow.y allows tighter, brighter hotspot)
	temp2.x *= glow.x;
		//higher glow.x gives dimmer glow (because next step is 1 / "angle")
	temp2.x = pow(temp2.x, glow.z);
		//glow.z should be negative, so we're doing a sort of (1 / "angle") function

	//add "minimum anti-solar illumination"
	temp2.x += .25;
	
	//increase ambient when there are more clouds
	vec4 tmpAmbient = ambient + (vec4(1.) - ambient) * cloud_shadow * 0.5;
	
	//haze color
	setAdditiveColor(
		vec3(blue_horizon * blue_weight * (sunlight*(1.-cloud_shadow) + tmpAmbient)
	  + (haze_horizon * haze_weight) * (sunlight*(1.-cloud_shadow) * temp2.x
		  + tmpAmbient)));

	// decrease ambient value for occluded areas
	tmpAmbient *= mix(ssao_effect, 1.0, ambFactor);

	//brightness of surface both sunlight and ambient
	setSunlitColor(vec3(sunlight * .5));
	setAmblitColor(vec3(tmpAmbient * .25));
	setAdditiveColor(getAdditiveColor() * vec3(1.0 - temp1));
}

vec3 atmosLighting(vec3 light)
{
	light *= getAtmosAttenuation().r;
	light += getAdditiveColor();
	return (2.0 * light);
}

vec3 atmosTransport(vec3 light) {
	light *= getAtmosAttenuation().r;
	light += getAdditiveColor() * 2.0;
	return light;
}
vec3 atmosGetDiffuseSunlightColor()
{
	return getSunlitColor();
}

vec3 scaleDownLight(vec3 light)
{
	return (light / scene_light_strength );
}

vec3 scaleUpLight(vec3 light)
{
	return (light * scene_light_strength);
}

vec3 atmosAmbient(vec3 light)
{
	return getAmblitColor() + light / 2.0;
}

vec3 atmosAffectDirectionalLight(float lightIntensity)
{
	return getSunlitColor() * lightIntensity;
}

vec3 scaleSoftClip(vec3 light)
{
	//soft clip effect:
	light = 1. - clamp(light, vec3(0.), vec3(1.));
	light = 1. - pow(light, gamma.xxx);

	return light;
}

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() 
{
	vec2 tc = vary_fragcoord.xy;
	float depth = texture2DRect(depthMap, tc.xy).r;
	vec3 pos = getPosition_d(tc, depth).xyz;
	vec3 norm = texture2DRect(normalMap, tc).xyz;
	norm = (norm.xyz-0.5)*2.0; // unpack norm
		
	float da = max(dot(norm.xyz, sun_dir.xyz), 0.0);
	
	vec4 diffuse = texture2DRect(diffuseRect, tc);

	vec3 col;
	float bloom = 0.0;
	float fullbrightification = diffuse.a;

	if (fullbrightification < 1.0)
	{
		vec4 spec = texture2DRect(specularRect, vary_fragcoord.xy);
		
		vec2 scol_ambocc = texture2DRect(lightMap, vary_fragcoord.xy).rg;
		float scol = scol_ambocc.r;//1220*max(scol_ambocc.r, fullbrightification); 
		//float scol = scol_ambocc.r;//max(scol_ambocc.r, scol_ambocc.r*(1.0 - fullbrightification));//1;//da;//max(da, scol_ambocc.r);
		float ambocc = scol_ambocc.g;
	
		calcAtmospherics(pos.xyz, ambocc);
	
		col = atmosAmbient(vec3(0));
		//col += atmosAffectDirectionalLight(max(min(da, scol), fullbrightification));
		col += atmosAffectDirectionalLight(min(da, scol));
	
		col *= diffuse.rgb;
	
		//spec.rgba = max(spec.rbga, vec4(0.8, 0.8, 0.8, 0.2));
		//spec.rgba = max(spec.rbga, vec4(1.0, 1.0, 1.0, 0.0));

		//spec.rgba *= (1.0-dot(diffuse.rgb, vec3(0.299, 0.587, 0.144)));
		if (spec.a > 0.0) // specular reflection
		{
			// the old infinite-sky shiny reflection
			//
			vec3 refnormpersp = normalize(reflect(pos.xyz, norm.xyz));
			float sa = dot(refnormpersp, sun_dir.xyz);
			float magic = 3.0; // TODO: work out what shadow val is being pre-div'd by
			vec3 dumbshiny = (vary_SunlitColor)*(scol * magic)*(2.0 * texture2D(lightFunc, vec2(sa, spec.a)).r);
			dumbshiny = min(dumbshiny, vec3(1));

			// screen-space cheap fakey reflection map
			//
			vec3 refnorm = normalize(reflect(vec3(0,0,-1), norm.xyz));
			depth -= 0.5; // unbias depth
			// first figure out where we'll make our 2D guess from
			//vec2 ref2d = (0.25 * screen_res.y) * (refnorm.xy) * abs(refnorm.z) / depth;
			vec2 orig_ref2d = (norm.xy);// * (1.0- depth);

			// Offset the guess source a little according to a trivial
			// checkerboard dither function and spec.a.
			// This is meant to be similar to sampling a blurred version
			// of the diffuse map.  LOD would be better in that regard.
			// The goal of the blur is to soften reflections in surfaces
			// with low shinyness, and also to disguise our lameness.
			
			float checkerboard = floor(mod(tc.x+tc.y, 2.0)); // 0.0, 1.0
			//float checkoffset = ((1.0 + 1.0-abs(norm.z))) * (1.0 + (11.0*(1.0-spec.a)))*(checkerboard-0.5);

			// hack because I can't decide whether refnormpersp or refnorm are better :3
			//if (checkerboard > 0.0) {
			vec2 orig_ref2dpersp = (refnormpersp.xy);
			  //}


			vec3 best_refn = vec3(0);
			float best_refshad = 0;
			float best_refapprop = -1.0;
			vec3 best_refcol = vec3(0);
			float total_refapprop = 0;
			float rnd = rand(tc.xy);
			//vec3 reflight = reflect(sun_dir.xyz, -norm.xyz);
			//vec3 reflight = sun_dir.xyz;
			vec3 reflight = sun_dir.xyz;//reflect(sun_dir.xyz, norm.xyz);
			float bloomdamp = 0.0;
			const int its = 26;
			const float itsf = 26.0;
			for (int guessnum = 1; guessnum <= its; ++guessnum)
			{
			  //float rdpow2 = float(guessnum)/(26.0);
			  float guessnumfp = float(guessnum);
			  //guessnumfp -= rnd;
			  float gnfrac = guessnumfp / itsf;
			  guessnumfp -= (checkerboard*0.5 + rnd);
			  //guessnumfp += abs(fract(sin(orig_ref2d.x*123.45)+cos(orig_ref2d.y*555.0)));
			  //float rdpow2 = (guessnumfp)/(26.0);
			  float rd = guessnumfp / itsf;
			  float rdpow2 = rd * rd;
			  float refdist = 0.5*(screen_res.y * (rd));// / (-depth) ;
			  //float refdist = 0.25*(screen_res.y * (rdpow2)) ;
			  //float refdist = (0.13* (screen_res.y) * ((1-abs(norm.z)) + ((1-depth)))) * (1.0*(guessnum+1)/161.0);
			  //vec2 ref2d = mix(orig_ref2d, orig_ref2dpersp, 0*fract(orig_ref2d.y*12345.678)) * refdist;
			  vec2 ref2d = (orig_ref2d + (1.0 - min(length(spec.rgb), 1.0))*0.13*vec2(rnd*2.0-1.0)) * refdist;
			
			  //ref2d.x += checkerboard;

			  ////vec2 ref2d = (0.1 * screen_res.y) * (norm.xy) * (1.0-abs(norm.z));// / (depth);
			  // Get a 3D point along this reflector's normal, project it back to 2D... uh.. project the normal into screenspace?
			  /*vec3 meh = norm.xyz;
			    vec2 ref2d = (norm.xy);
			    float ref2dlen = length(ref2d);
			    ref2d *= abs(norm.z);
			    ref2d *= (0.25 * screen_res.y) / depth;*/
			  //ref2d += screen_res.y * 0.05 * vec2(checkoffset, checkoffset);
			  ////ref2d += normalize(ref2d)*checkoffset;
			  ref2d += tc.xy; // use as offset from destination

			  if (ref2d.y < 0.0 || ref2d.y > screen_res.y ||
			      ref2d.x < 0.0 || ref2d.x > screen_res.x) break;

			  // Get attributes from the 2D guess point.
			  float refdepth = texture2DRect(depthMap, ref2d).r;
			  //if (refdepth == 1.0) continue; // don't sample the sky
			  vec3 refcol = texture2DRect(diffuseRect, ref2d).rgb;
			  vec3 refpos = getPosition_d(ref2d, refdepth).xyz;
			  // figure out how appropriate our guess actually was, directionwise
			  //float refapprop=1;
			  //float refapprop = max(0.0, dot(refnorm, normalize(refpos - pos)));
			  float refapprop = 1.0 - rdpow2;
			  if (refdepth < 1.0) { // non-sky
			    //refapprop *= step(0.0, dot(refnorm, (refpos - pos)));
			    refapprop *= max(0.0, dot(refnorm, normalize(refpos - pos)));
			    //refapprop *= (1.0 - sqrt(rdpow2));
			    float refshad = texture2DRect(lightMap, ref2d).r;
			    vec3 refn = normalize(texture2DRect(normalMap, ref2d).rgb * 2.0 - 1.0);
			    // darken reflections from points which face away from the reflected ray - our guess was a back-face
			    //refapprop *= step(dot(refnorm, refn), 0.0);
			    refapprop = max(min(refapprop, -dot(refnorm, refn)), 0.0);
			    //refapprop = min(refapprop, max(0.0, -dot(refnorm, refn))); // more conservative variant
			    // kill guesses which are 'behind' the reflector
			    float ppdist = dot(norm.xyz, refpos.xyz - pos.xyz);
			    //if (ppdist > -0.1) {
			    //  refapprop = 0.0;
			    //}

			    refapprop *= step(0.0, ppdist);
			    //refapprop = min(refapprop, smoothstep(0.0, 0.05, ppdist));

			    ////			refapprop = min(refapprop, 1.0-smoothstep(3.0, 5.0, ppdist));
			    //float distweight = 1.0-smoothstep(0.05*0.05, 6.0*6.0, dot(refpos.xyz-pos.xyz,refpos.xyz-pos.xyz));

			    //float distweight = smoothstep(0.05, 60.0*spec.a, distance(refpos.xyz,pos.xyz));
			    //refapprop = min(refapprop, 1.0 - distweight);

			    /*float distweight = 1.0 / (1.0 + (1.0-spec.a)*distance(refpos.xyz,pos.xyz));
			      refapprop = min(refapprop, distweight);*/

			    //refapprop *= 1.0/(1.0+distance(refpos.xyz,pos.xyz));

			    //refapprop = /*magickybooster*/2.5*(refapprop);
			    total_refapprop += refapprop;
			    best_refn += refn.xyz * refapprop;
			    best_refshad += refshad * refapprop;
			    best_refcol += ( (vary_AmblitColor + vary_SunlitColor * min(max(0.0, dot(reflight, refn)), refshad)) * refcol.rgb + vary_AdditiveColor) * refapprop;
			    //if (refapprop > best_refapprop) {
			    //best_refapprop = refapprop;
			    ////best_refn = refn.xyz;
			    //}
			  }
			  else  // sky
			  {
			    //refapprop = 0.0;

			    // avoid forward-pointing reflections picking up sky
			    //refapprop = max(min(refapprop, -dot(refnorm, vec3(0,0,1))), 0.0);
			    refapprop *= max(-refnorm.z, 0.0);//dot(refnorm, vec3(0.0, 0.0, -1.0));
 
			    //			    refapprop *= 0.5;
			    total_refapprop += refapprop;
			    //best_refn += vec3(0.0, 0.0, -1.0);//reflight.xyz * refapprop; // treat sky samples as if they always face the sun
			    best_refn += reflight.xyz * refapprop; // treat sky samples as if they always face the sun
			    best_refshad += 1.0 * refapprop; // sky is not shadowed
			    best_refcol += refcol.rgb * refapprop;
			    //bloomdamp += refapprop; // even though we say they face the sun, don't want sky reflections blooming
			  }
			}
			if (total_refapprop > 0.0) {
			  best_refn = normalize(best_refn);
			  best_refshad /= total_refapprop;
			  best_refcol /= total_refapprop;
			  bloomdamp /= total_refapprop;
			}
			//best_refapprop = min((total_refapprop / ((itsf+1.0) * 0.4)), 1.0);
			best_refapprop = min((total_refapprop / ((itsf))), 1.0);
			//best_refapprop = min(total_refapprop, 1.0);
			// USE: refn, refshad, refapprop, refcol

			// get appropriate light strength for guess-point
			// reflect light direction to increase the illusion that
			// these are reflections.
			//float reflit = min(max(dot(best_refn, reflight.xyz), 0.0), best_refshad);
			// apply sun color to guess-point, dampen according to inappropriateness of guess
			//float refmod = min(refapprop, reflit);
			//vec3 refprod = vary_SunlitColor * refcol.rgb * refmod;
			//vec3 ssshiny = (refprod * spec.a);
			// apply sun color to guess-point, dampen according to inappropriateness of guess
			//float refmod = min(best_refapprop, reflit);
			//			float refmod = min(total_refapprop, reflit);//min(best_refapprop, reflit);
			// get env map
			//vec3 env_vec = env_mat * refnormpersp;
			//vec3 env_col = textureCube(environmentMap, env_vec).rgb;
			vec3 refprod = best_refcol.rgb * best_refapprop;
			vec3 ssshiny = (refprod);// * spec.a);
			//vec3 refprod = mix(env_col, vary_SunlitColor * refcol.rgb * refmod * spec.a, refapprop*0+1);

			//vec3 ssshiny = (refprod);

			//ssshiny *= 0.3; // dampen it even more
			//ssshiny *= 10.0;

			ssshiny *= spec.rgb;


			// add the two types of shiny together
			vec3 spec_contrib = (ssshiny * (1.0 - fullbrightification) * 0.5 + dumbshiny);
			bloom = spec.a * dot(spec_contrib, spec_contrib) * 0.25 * (1.0 - bloomdamp);

			// add environmentmap
			//vec3 env_vecx = env_mat * refnormpersp;
			//col = mix(col.rgb, textureCube(environmentMap, env_vecx).rgb, scol_ambocc.g * (1.0 - refapprop) * max(spec.a-diffuse.a*2.0, 0.0));
			//col.rgb += (0.25*textureCube(environmentMap, env_vecx).rgb + spec_contrib.rgb) * min((2.0 - dot(vec3(0.2126, 0.7152, 0.0722), diffuse.rgb)) * scol_ambocc.g * spec.a, 1.0);

			//col.rgb += (1.0-dot(diffuse.rgb, vec3(0.299, 0.587, 0.144))) * (ssshiny.rgb + dumbshiny.rgb) * spec.rgb;

		//col *= 2.0;
			
		//col = mix(col, diffuse.rgb, diffuse.a); // mix with raw sky reflection if it's water

			// simply additive specular
			//col.rgb = col.rgb + (ssshiny.rgb + dumbshiny.rgb) * spec.rgb;

			// emulate non-deferred shiny where higher shiny dims underlying diffuse
			col.rgb = col.rgb * ((1.0)-spec.a*spec.a);
			// mix with raw sky reflection if it's water (diffuse.a > 0), add shiny always
			col.rgb = mix(col.rgb + ssshiny, diffuse.rgb, fullbrightification) + dumbshiny;

			//(0.25*textureCube(environmentMap, env_vecx).rgb + spec_contrib.rgb) * min((2.0 - 0.333*(diffuse.r+diffuse.g+diffuse.b)) * scol_ambocc.g * spec.a, 1.0);
			//col = mix(col.rgb, 0.3*textureCube(environmentMap, env_vecx).rgb, scol_ambocc.g * max(spec.a-diffuse.a*2.0, 0.0));
			//col += spec_contrib;// * scol_ambocc.g;
		}

		col = atmosLighting(col);
		col = scaleSoftClip(col);

		//col.rgb = mix(col.rgb, diffuse.rgb, diffuse.a)
	}
	else
	{
	  col = diffuse.rgb; // raw sky
	}
		
	frag_color.rgb = col;
	frag_color.a = bloom;
}
