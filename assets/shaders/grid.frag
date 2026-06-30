#version 460 core
in vec2 v_WorldPos;
out vec4 FragColor;

uniform vec3 u_BgColor;
uniform vec3 u_GridColor;
uniform float u_GridSize;

float grid(vec2 worldPos, float gridSize) {
    vec2 coord = worldPos / gridSize;
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);
    return 1.0 - min(line, 1.0);
}

void main() {
    float g = grid(v_WorldPos, u_GridSize);
    vec3 color = mix(u_BgColor, u_GridColor, g);
    FragColor = vec4(color, 1.0);
}
