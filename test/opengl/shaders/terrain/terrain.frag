#version 420

in FragIn
{
    vec3 world_pos;
    vec3 normal;
    vec4 shadow_coords;
} frag_in;

layout (location = 0) out vec4 frag_color;

layout (binding = 3) uniform sampler2D height_map;

layout (binding = 0, std140) uniform SharedData
{
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 ambient;
    vec3 diffuse;
    vec3 light_dir;
    vec3 eye_pos;
};

float xorshift(uvec2 n)
{
	uint x = n.x + 1024 * n.y;
	x += 3266489917u;
	x = x * 2386451719u + 374761393u;
	x = (x << 17) | (x >> 15);
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return int(x >> 41) / 8388608.0;
}

float cubic(float x)
{
    return x * x * (3.0 - 2.0 * x);
}

float get_linear_noise(vec2 n)
{
    uvec2 min = uvec2(floor(n));

    return 2.0 * mix(
        mix(xorshift(min + uvec2(0, 0)), xorshift(min + uvec2(1, 0)), cubic(n.x - min.x)),
        mix(xorshift(min + uvec2(0, 1)), xorshift(min + uvec2(1, 1)), cubic(n.x - min.x)), cubic(n.y - min.y)
    ) - 1.0;
}

float noise(vec2 pos)
{
    float total = 0.0;
    total += get_linear_noise(pos * 0.015625) * 0.5;
    total += get_linear_noise(pos * 0.03125) * 0.25;
    total += get_linear_noise(pos * 0.0625) * 0.125;
    total += get_linear_noise(pos * 0.125) * 0.0625;
    return total;
}

void main()
{
    vec3 normal = normalize(frag_in.normal);
    vec3 color = mix(vec3(0.37, 0.84, 0.0), vec3(1.0, 0.94, 0.5), smoothstep(0.0, 1.0, frag_in.world_pos.y * -0.0625 + 0.96875)) //(texture(detail, frag_in.world_pos.xz * 0.00390625).g * 0.125 + 0.875)
        * ((noise(frag_in.world_pos.xz + vec2(768.0)) * 0.5 + 0.5) * 0.3 + 0.7);
    float spec = 0.5;

    vec3 diffuse_factor = diffuse * max(dot(normal, light_dir), 0.0);
    vec3 lighting = ambient + diffuse_factor;
    vec3 to_cam = normalize(eye_pos - frag_in.world_pos);
    vec3 specular = vec3(0); //diffuse_factor * spec * pow(max(dot(to_cam, reflect(-light_dir, normal)), 0.0), 8.0);
    frag_color = vec4(color * lighting + specular, 1.0);
}
