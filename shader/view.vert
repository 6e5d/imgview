#version 450

vec2 vs[6] = vec2[](
	vec2(0.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(1.0, 1.0)
);
layout(location = 0) out vec2 uv;

layout(set = 0, binding = 0) uniform Data {
	mat4 model;
	mat4 proj;
	mat4 view;
} uniforms;

void main() {
	uv = vs[gl_VertexIndex];
	vec4 pos = vec4(uv, 0.0, 1.0);
	gl_Position = uniforms.proj * uniforms.view * uniforms.model * pos;
}
