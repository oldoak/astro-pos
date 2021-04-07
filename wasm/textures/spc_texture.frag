#version 100

#ifdef GL_ES
precision mediump float;
#endif

varying vec3 frag_colour;
varying vec2 texture_coord;

uniform sampler2D ourTexture;

void main()
{
	gl_FragColor = texture2D(ourTexture, texture_coord);
}
