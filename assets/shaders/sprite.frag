#version 460 core
in vec2 v_UV;
out vec4 FragColor;

uniform vec3 u_Color;

void main() {
    FragColor = vec4(u_Color, 1.0);
}
