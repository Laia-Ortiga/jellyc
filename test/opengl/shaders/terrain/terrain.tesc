#version 450

layout (vertices = 4) out;

in vec2 cs_in_pos[];

out vec2 es_in_pos[];

layout (std140, binding = 0) uniform SharedData
{
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 ambient;
    vec3 diffuse;
    vec3 light_dir;
    vec3 eye_pos;
};

float calc_dist(vec2 a, vec2 b)
{
    vec4 cam_plane = -vec4(view_matrix[0][2], view_matrix[1][2], view_matrix[2][2], view_matrix[3][2]);
    return max(dot(cam_plane, vec4(a.x, 0.0, a.y, 1.0)), dot(cam_plane, vec4(b.x, 0.0, b.y, 1.0)));
}

float level(float dist)
{
    // 16 * sqrt(2)
    if (dist < -22.627416997969520780827019587355)
	{
        return 0.0;
    }
    else
	{
        return clamp(16.0 - 0.0078125 * dist, 1.0, 16.0);
    }
}

void main()
{
    es_in_pos[gl_InvocationID] = cs_in_pos[gl_InvocationID];
    
    float dist0 = calc_dist(es_in_pos[0], es_in_pos[1]);
    float dist1 = calc_dist(es_in_pos[0], es_in_pos[3]);
    float dist2 = calc_dist(es_in_pos[2], es_in_pos[3]);
    float dist3 = calc_dist(es_in_pos[1], es_in_pos[2]);
    
    gl_TessLevelOuter[0] = level(dist0);
    gl_TessLevelOuter[1] = level(dist1);
    gl_TessLevelOuter[2] = level(dist2);
    gl_TessLevelOuter[3] = level(dist3);
    gl_TessLevelInner[0] = mix(gl_TessLevelOuter[0], gl_TessLevelOuter[2], 0.5);
    gl_TessLevelInner[1] = mix(gl_TessLevelOuter[1], gl_TessLevelOuter[3], 0.5);
}
