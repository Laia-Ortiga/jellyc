#version 420

out FragIn
{
    float pos;
} frag_in;

layout (binding = 0, std140) uniform SharedData
{
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 ambient;
    vec3 diffuse;
    vec3 light_dir;
    vec3 eye_pos;
};

void main()
{
    vec2 texcoords = vec2(float(gl_VertexID % 2), float(gl_VertexID / 2));

    mat4 inv_proj_matrix = mat4(
        vec4(1.0 / proj_matrix[0][0], 0.0, 0.0, 0.0),
        vec4(0.0, 1.0 / proj_matrix[1][1], 0.0, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0 / proj_matrix[3][2]),
        vec4(0.0, 0.0, 1.0 / proj_matrix[2][3], -proj_matrix[2][2] / (proj_matrix[2][3] * proj_matrix[3][2]))
    );
    vec3 ray_dir = vec3(mat2(inv_proj_matrix) * vec2(texcoords * 2.0 - 1.0), -1.0);
    gl_Position = vec4(texcoords * 2.0 - 1.0, 1.0, 1.0);
    frag_in.pos = dot(vec3(view_matrix[1]), ray_dir);
}
