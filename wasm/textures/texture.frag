#version 100

#ifdef GL_ES
precision mediump float;
#endif

varying vec2 texture_coord;
varying vec3 normal;
varying vec3 light_vector;

uniform sampler2D texture_sampler;
uniform vec3 ambient_colour; // The light and object's combined ambient colour
uniform vec3 diffuse_colour; // The light and object's combined diffuse colour

const float inv_radius_square = 0.00001;

void main()
{
    // Base colour (from the diffuse texture)
    vec4 colour = texture2D(texture_sampler, texture_coord);
    //vec4 colour = texture2D(texture_sampler, gl_FragCoord);

    // Ambient lighting
    vec3 ambient = vec3(ambient_colour * colour.xyz);

    // Calculate the light attenuation, and direction
    float dist_square = dot(light_vector, light_vector);
    float attenuation = clamp(1.0 - inv_radius_square * sqrt(dist_square),
                              0.0,
                              1.0);
    vec3 light_direction = light_vector * inversesqrt(dist_square);

    // Diffuse lighting
    vec3 diffuse = max(dot(light_direction, normal),
                       0.0) * diffuse_colour * colour.xyz;

    // The final colour
    // NOTE: Alpha channel shouldn't be affected by lights
    vec3 final_colour = (ambient + diffuse) * attenuation;
    gl_FragColor = vec4(final_colour, colour.w);
}
