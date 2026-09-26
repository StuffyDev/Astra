// Системные шейдеры движка, вшитые в бинарь.
// Пользователь не может их удалить или испортить в проекте.
#pragma once

namespace SystemShaders {

inline const char* LineVert = R"(
#version 460 core
layout (location = 0) in vec2 a_Pos;
uniform mat4 u_ViewProj;
void main() {
    gl_Position = u_ViewProj * vec4(a_Pos, 0.0, 1.0);
}
)";

inline const char* LineFrag = R"(
#version 460 core
out vec4 FragColor;
uniform vec3 u_Color;
void main() {
    FragColor = vec4(u_Color, 1.0);
}
)";

inline const char* SpriteVert = R"(
#version 460 core
layout (location = 0) in vec2 a_Pos;
out vec2 v_UV;
uniform mat4 u_MVP;
void main() {
    gl_Position = u_MVP * vec4(a_Pos, 0.0, 1.0);
    v_UV = a_Pos + 0.5;
}
)";

inline const char* SpriteFrag = R"(
#version 460 core
in vec2 v_UV;
out vec4 FragColor;
uniform vec3 u_Color;
void main() {
    FragColor = vec4(u_Color, 1.0);
}
)";

inline const char* CircleFrag = R"(
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
)";

inline const char* SpriteTexturedFrag = R"(
#version 460 core
in vec2 v_UV;
out vec4 FragColor;
uniform vec3 u_Color;
uniform sampler2D u_Texture;
void main() {
    FragColor = texture(u_Texture, v_UV) * vec4(u_Color, 1.0);
}
)";

inline const char* CircleTexturedFrag = R"(
#version 460 core
in vec2 v_UV;
out vec4 FragColor;
uniform vec3 u_Color;
uniform sampler2D u_Texture;
void main() {
    vec2 center = v_UV - vec2(0.5);
    if (length(center) > 0.5) discard;
    FragColor = texture(u_Texture, v_UV) * vec4(u_Color, 1.0);
}
)";

} // namespace SystemShaders
