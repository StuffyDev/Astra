#version 460 core
layout (location = 0) in vec2 a_Pos;

uniform mat4 u_ViewProj;

void main() {
    gl_Position = u_ViewProj * vec4(a_Pos, 0.0, 1.0);
}
