#version 100

attribute vec3 vertex_position;
attribute vec2 vertex_texture;
attribute vec3 vertex_normal;

varying vec2 texture_coord;
varying vec3 normal;
varying vec3 light_vector;

uniform mat4 mv_mat;
uniform mat4 normal_mat;
uniform mat4 proj_mat;
uniform vec3 light_position;

// NOTE: position in view space (so after
// (being transformed by its own MV matrix)
void main()
{
    // Pass on the texture coordinate
    texture_coord = vertex_texture;

    // Calc. the position in view space
    vec4 view_position = mv_mat * vec4(vertex_position, 1.0);

    // Calc the position
    gl_Position = proj_mat * view_position;

    // Transform the normal
    normal = normalize((normal_mat * vec4(vertex_normal, 1.0)).xyz);

    // Calc. the light vector
    light_vector = light_position - view_position.xyz;
}
