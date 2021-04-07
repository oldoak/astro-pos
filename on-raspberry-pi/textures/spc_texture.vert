#version 100

attribute vec3 vertex_position;
//attribute vec3 vertex_colour;
attribute vec2 vertex_texture;

//varying vec3 frag_colour;
varying vec2 texture_coord;

void main()
{
    //frag_colour = vertex_colour;
    texture_coord = vertex_texture;
    gl_Position = vec4(vertex_position, 1.0);
}
