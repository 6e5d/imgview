#version 450

vec2 plane[6] = vec2[] (
	vec2(-1, -1),
	vec2(1, 1),
	vec2(-1, 1),
	vec2(1, 1),
	vec2(-1, -1),
	vec2(1, -1)
);

void main() {
	gl_Position = vec4(plane[gl_VertexIndex], 0.0, 1.0);
}
