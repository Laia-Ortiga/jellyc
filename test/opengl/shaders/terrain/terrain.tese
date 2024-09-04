#version 450

layout (quads, equal_spacing, ccw) in;

in vec2 es_in_pos[];

out FragIn
{
    vec3 world_pos;
    vec3 normal;
    vec4 shadow_coords;
} frag_in;

layout (binding = 3) uniform sampler2D height_map;
uniform float tile_size;

layout (std140, binding = 0) uniform SharedData
{
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 ambient;
    vec3 diffuse;
    vec3 light_dir;
    vec3 eye_pos;
};

layout (binding = 1, std140) uniform SharedShadowData
{
    mat4 shadow_matrix;
};

vec2 interpol2D(vec2 a, vec2 b, vec2 c, vec2 d)
{
    return mix(mix(a, d, gl_TessCoord.x), mix(b, c, gl_TessCoord.x), gl_TessCoord.y);
}

float get_height(vec2 pos)
{
    return texture(height_map, pos * tile_size + 0.5).r;
}

void main()
{
    vec2 grid_pos = interpol2D(es_in_pos[0], es_in_pos[1], es_in_pos[2], es_in_pos[3]);
    vec3 pos = vec3(grid_pos.x, get_height(grid_pos), grid_pos.y);

    vec3 off = vec3(1.0, 0.0, 1.0);
    float hd = get_height(grid_pos - off.yz);
    float hl = get_height(grid_pos - off.xy);
    float hr = get_height(grid_pos + off.xy);
    float hu = get_height(grid_pos + off.yz);
    vec3 normal = normalize(vec3(hl - hr, 2.0, hd - hu));

    gl_Position = proj_matrix * view_matrix * vec4(pos, 1.0);
    frag_in.world_pos = pos;
    frag_in.normal = normal;
    frag_in.shadow_coords = shadow_matrix * vec4(pos, 1.0);
}
