#version 330

in FragIn
{
    float pos;
} frag_in;

layout (location = 0) out vec4 frag_color;

uniform mat3 colors;

void main()
{
    vec3 color_positive = mix(sqrt(colors[1]), sqrt(colors[0]), clamp(frag_in.pos, 0.0, 1.0));
    vec3 color = mix(color_positive, sqrt(colors[2]), clamp(-frag_in.pos, 0.0, 1.0));
    frag_color = vec4(color * color, 1.0);
}
