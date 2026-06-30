#version 460 core
layout (location = 0) in vec2 a_Pos;

out vec2 v_UV;

uniform mat4 u_MVP;

void main() {
    gl_Position = u_MVP * vec4(a_Pos, 0.0, 1.0);
    v_UV = a_Pos + 0.5; // от -0.5..0.5 к 0..1
}
