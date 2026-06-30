#version 460 core
in vec2 v_UV;
out vec4 FragColor;

uniform vec3 u_Color;

void main() {
    vec2 center = v_UV - vec2(0.5);
    float dist = length(center);
    if (dist > 0.5) discard;
    FragColor = vec4(u_Color, 1.0);
}
