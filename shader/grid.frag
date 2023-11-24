#version 450

layout(location = 0) out vec4 o_color;

void main() {
	const float x = 100.0;
	const float cb1 = 0.9;
	const float cb2 = 0.8;
	vec4 spos = gl_FragCoord;
	vec2 spos2 = vec2(spos[0] / spos[3], spos[1] / spos[3]);
	if ((mod(spos2[0], x) < x / 2) == (mod(spos2[1], x) < x / 2)) {
		o_color = vec4(cb1, cb1, cb1, 1.0);
	} else {
		o_color = vec4(cb2, cb2, cb2, 1.0);
	}
}
