/** 
 * @file alphaSkinnedV.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
 
#version 120

vec4 calcLighting(vec3 pos, vec3 norm, vec4 color, vec4 baseCol);
mat4 getObjectSkinnedTransform();
void calcAtmospherics(vec3 inPositionEye);

float calcDirectionalLight(vec3 n, vec3 l);
float calcPointLightOrSpotLight(vec3 v, vec3 n, vec4 lp, vec3 ln, float la, float is_pointlight);

vec3 atmosAmbient(vec3 light);
vec3 atmosAffectDirectionalLight(float lightIntensity);
vec3 scaleDownLight(vec3 light);
vec3 scaleUpLight(vec3 light);

varying vec3 vary_position;
varying vec3 vary_ambient;
varying vec3 vary_directional;
varying vec3 vary_normal;
varying vec3 vary_light;
varying vec3 vary_fragcoord;

uniform float near_clip;

void main()
{
	gl_TexCoord[0] = gl_MultiTexCoord0;
				
	vec4 pos;
	vec3 norm;
	
	mat4 trans = getObjectSkinnedTransform();
	trans = gl_ModelViewMatrix * trans;
	
	pos = trans * gl_Vertex;
	
	norm = gl_Vertex.xyz + gl_Normal.xyz;
	norm = normalize(( trans*vec4(norm, 1.0) ).xyz-pos.xyz);
	
	vec4 frag_pos = gl_ProjectionMatrix * pos;
	gl_Position = frag_pos;
	
	vary_position = pos.xyz;
	vary_normal = norm;	
	
	calcAtmospherics(pos.xyz);

	vec4 col = vec4(0.0, 0.0, 0.0, gl_Color.a);

	// Collect normal lights (need to be divided by two, as we later multiply by 2)
	col.rgb += gl_LightSource[2].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[2].position, gl_LightSource[2].spotDirection.xyz, gl_LightSource[2].linearAttenuation, gl_LightSource[2].specular.a);
	col.rgb += gl_LightSource[3].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[3].position, gl_LightSource[3].spotDirection.xyz, gl_LightSource[3].linearAttenuation, gl_LightSource[3].specular.a);
	col.rgb += gl_LightSource[4].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[4].position, gl_LightSource[4].spotDirection.xyz, gl_LightSource[4].linearAttenuation, gl_LightSource[4].specular.a);
	col.rgb += gl_LightSource[5].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[5].position, gl_LightSource[5].spotDirection.xyz, gl_LightSource[5].linearAttenuation, gl_LightSource[5].specular.a);
	col.rgb += gl_LightSource[6].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[6].position, gl_LightSource[6].spotDirection.xyz, gl_LightSource[6].linearAttenuation, gl_LightSource[6].specular.a);
	col.rgb += gl_LightSource[7].diffuse.rgb*calcPointLightOrSpotLight(pos.xyz, norm, gl_LightSource[7].position, gl_LightSource[7].spotDirection.xyz, gl_LightSource[7].linearAttenuation, gl_LightSource[7].specular.a);
	col.rgb += gl_LightSource[1].diffuse.rgb*calcDirectionalLight(norm, gl_LightSource[1].position.xyz);
	col.rgb = scaleDownLight(col.rgb);
	
	// Add windlight lights
	col.rgb += atmosAmbient(vec3(0.));
	
	vary_light = gl_LightSource[0].position.xyz;
	
	vary_ambient = col.rgb*gl_Color.rgb;
	vary_directional = gl_Color.rgb*atmosAffectDirectionalLight(max(calcDirectionalLight(norm, gl_LightSource[0].position.xyz), (1.0-gl_Color.a)*(1.0-gl_Color.a)));
	
	col.rgb = min(col.rgb*gl_Color.rgb, 1.0);
	
	gl_FrontColor = col;

	gl_FogFragCoord = pos.z;
	
	vary_fragcoord.xyz = frag_pos.xyz + vec3(0,0,near_clip);
}


