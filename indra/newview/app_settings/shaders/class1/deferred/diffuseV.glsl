/** 
 * @file diffuseV.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * $/LicenseInfo$
 */
 

attribute vec4 position;
attribute vec4 diffuse_color;
attribute vec3 normal;
attribute vec2 texcoord0;

varying vec3 vary_normal;
varying float vary_texture_index;

void main()
{
	//transform vertex
	gl_Position = gl_ModelViewProjectionMatrix * vec4(position.xyz, 1.0); 
	gl_TexCoord[0] = gl_TextureMatrix[0] * vec4(texcoord0,0,1);
	
	vary_texture_index = position.w;
	vary_normal = normalize(gl_NormalMatrix * normal);

	gl_FrontColor = diffuse_color;
}
