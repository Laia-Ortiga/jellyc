#version 420

out vec2 cs_in_pos;

void main()
{
	cs_in_pos = vec2(float(gl_VertexID % 97), float(gl_VertexID / 97)) * 16.0 - vec2(768.0);
}
