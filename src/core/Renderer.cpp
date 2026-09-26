#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/SystemShaders.h"
#include "ecs/Entity.h"
#include "ecs/Physics.h"
#include "ecs/Transforms.h"
#include "utils/Shader.h"
#include "utils/Texture.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>

Renderer::Renderer() = default;
Renderer::~Renderer() { Shutdown(); }

void Renderer::Init() {
    SetupGridBuffers();
    SetupQuad();
    SetupGizmoBuffers();
    SetupColliderBuffers(); // <-- новое
    SetupWhiteTexture();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    CreateShaders();
}

void Renderer::CreateShaders() {
    using namespace SystemShaders;
    m_LineShader = std::make_unique<Shader>(LineVert, LineFrag);
    m_SpriteShader = std::make_unique<Shader>(SpriteVert, SpriteFrag);
    m_CircleShader = std::make_unique<Shader>(SpriteVert, CircleFrag);
    m_SpriteTextureShader = std::make_unique<Shader>(SpriteVert, SpriteTexturedFrag);
    m_CircleTextureShader = std::make_unique<Shader>(SpriteVert, CircleTexturedFrag);
}

void Renderer::ClearProjectCaches() {
    // Текстуры и пользовательские шейдеры привязаны к путям проекта
    m_TextureCache.clear();
    m_FailedTextures.clear();
    m_UserShaderCache.clear();
    m_FailedUserShaders.clear();
    // Системные шейдеры вшиты в бинарь — их не трогаем
}

void Renderer::Shutdown() {
    m_TextureCache.clear();
    glDeleteTextures(1, &m_WhiteTexture);
    m_WhiteTexture = 0;
    glDeleteVertexArrays(1, &m_GridVAO);
    glDeleteBuffers(1, &m_GridVBO);
    glDeleteVertexArrays(1, &m_QuadVAO);
    glDeleteBuffers(1, &m_QuadVBO);
    glDeleteBuffers(1, &m_QuadEBO);
    glDeleteVertexArrays(1, &m_GizmoVAO);
    glDeleteBuffers(1, &m_GizmoVBO);
    glDeleteVertexArrays(1, &m_ColliderVAO);
    glDeleteBuffers(1, &m_ColliderVBO);
}

// ============ GRID ============
void Renderer::SetupGridBuffers() {
    glGenVertexArrays(1, &m_GridVAO);
    glGenBuffers(1, &m_GridVBO);
    glBindVertexArray(m_GridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_GridVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::UpdateGrid(Camera* camera) {
    const float gridSize = 50.0f;
    float viewHeight = 1080.0f * camera->GetZoom();
    float viewWidth = viewHeight * camera->GetAspectRatio();
    glm::vec2 pos = camera->GetPosition();

    float minX = pos.x - viewWidth * 0.5f - gridSize;
    float maxX = pos.x + viewWidth * 0.5f + gridSize;
    float minY = pos.y - viewHeight * 0.5f - gridSize;
    float maxY = pos.y + viewHeight * 0.5f + gridSize;

    minX = std::floor(minX / gridSize) * gridSize;
    maxX = std::ceil(maxX / gridSize) * gridSize;
    minY = std::floor(minY / gridSize) * gridSize;
    maxY = std::ceil(maxY / gridSize) * gridSize;

    std::vector<float> lines;
    for (float x = minX; x <= maxX; x += gridSize) {
        lines.push_back(x); lines.push_back(minY);
        lines.push_back(x); lines.push_back(maxY);
    }
    for (float y = minY; y <= maxY; y += gridSize) {
        lines.push_back(minX); lines.push_back(y);
        lines.push_back(maxX); lines.push_back(y);
    }

    m_GridCount = static_cast<int>(lines.size() / 2);
    glBindBuffer(GL_ARRAY_BUFFER, m_GridVBO);
    glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
}

void Renderer::RenderGrid(Camera* camera) {
    UpdateGrid(camera);
    m_LineShader->Use();
    m_LineShader->SetMat4("u_ViewProj", camera->GetViewProjectionMatrix());
    m_LineShader->SetVec3("u_Color", glm::vec3(0.30f, 0.30f, 0.30f));
    glBindVertexArray(m_GridVAO);
    glDrawArrays(GL_LINES, 0, m_GridCount);
    glBindVertexArray(0);
}

// ============ QUAD ============
void Renderer::SetupQuad() {
    float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };
    unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glGenBuffers(1, &m_QuadEBO);

    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// ============ GIZMO BUFFERS ============
void Renderer::SetupGizmoBuffers() {
    glGenVertexArrays(1, &m_GizmoVAO);
    glGenBuffers(1, &m_GizmoVBO);
    glBindVertexArray(m_GizmoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_GizmoVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// ============ TEXTURES ============
void Renderer::SetupWhiteTexture() {
    unsigned char white[4] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_WhiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_WhiteTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint Renderer::GetTexture(const std::string& path) {
    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end()) return it->second->GetID();
    if (m_FailedTextures.count(path) > 0) return 0;

    auto texture = std::make_unique<Texture>();
    if (!texture->LoadFromFile(path)) {
        m_FailedTextures.insert(path);
        return 0;
    }
    GLuint id = texture->GetID();
    m_TextureCache.emplace(path, std::move(texture));
    return id;
}

// ============ USER SHADERS ============
// Разработчик пишет только main() (и свои функции/униформы) — всё остальное добавляет движок.
static const char* kUserVertPrelude =
    "#version 460 core\n"
    "layout(location = 0) in vec2 a_Pos;\n"
    "out vec2 v_UV;\n"
    "uniform mat4 u_MVP;\n"
    "uniform mat4 u_Model;\n"
    "uniform mat4 u_ViewProj;\n"
    "uniform float u_Time;\n"
    "uniform vec2 u_ScreenSize;\n"
    "uniform vec4 u_UVRect;\n"
    "vec2 EngineUV() { return a_Pos + vec2(0.5f); }\n"
    // Стандартный квад: вызывает из main(), если вертекс писать не хочется.
    // v_UV уже режется u_UVRect — спрайт-анимация работает «из коробки».
    "void EngineQuadVert() {\n"
    "    v_UV = u_UVRect.xy + EngineUV() * u_UVRect.zw;\n"
    "    gl_Position = u_MVP * vec4(a_Pos, 0.0, 1.0);\n"
    "}\n";

static const char* kUserFragPrelude =
    "#version 460 core\n"
    "layout(location = 0) out vec4 fragColor;\n"
    "in vec2 v_UV;\n"
    "uniform vec3 u_Color;\n"
    "uniform vec4 u_Params;\n"   // «Material» из инспектора: 4 живых числа
    "uniform vec4 u_PColor;\n"   // «Material» из инспектора: цвет
    "uniform sampler2D u_Texture;\n"
    "uniform float u_Time;\n"
    "uniform vec2 u_ScreenSize;\n"
    "float EngineCircleMask(vec2 uv) {\n"
    "    vec2 p = uv * 2.0f - 1.0f;\n"
    "    return 1.0f - smoothstep(0.96f, 1.0f, length(p));\n"
    "}\n"
    "float EngineRoundedBox(vec2 uv, float radius) {\n"
    "    vec2 p = abs(uv * 2.0f - 1.0f) - (1.0f - radius);\n"
    "    float d = length(max(p, 0.0f)) - radius;\n"
    "    return 1.0f - smoothstep(-0.02f, 0.02f, d);\n"
    "}\n"
    "float EngineRing(vec2 uv, float radius, float thickness) {\n"
    "    float d = abs(length(uv * 2.0f - 1.0f) - radius);\n"
    "    return 1.0f - smoothstep(thickness * 0.5f, thickness * 0.5f + 0.02f, d);\n"
    "}\n"
    "vec2 EngineRotate(vec2 p, float deg) {\n"
    "    float r = radians(deg);\n"
    "    float c = cos(r), s = sin(r);\n"
    "    return mat2(c, -s, s, c) * p;\n"
    "}\n"
    "float EngineNoise(vec2 p) {\n"
    "    vec2 i = floor(p), f = fract(p);\n"
    "    vec2 u = f * f * (3.0 - 2.0 * f);\n"
    "    float a = fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453);\n"
    "    float b = fract(sin(dot(i + vec2(1, 0), vec2(127.1, 311.7))) * 43758.5453);\n"
    "    float c = fract(sin(dot(i + vec2(0, 1), vec2(127.1, 311.7))) * 43758.5453);\n"
    "    float d = fract(sin(dot(i + vec2(1, 1), vec2(127.1, 311.7))) * 43758.5453);\n"
    "    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);\n"
    "}\n"
    "float EngineFbm(vec2 p, int octaves) {\n"
    "    float v = 0.0, amp = 0.5;\n"
    "    for (int i = 0; i < octaves; i++) {\n"
    "        v += amp * EngineNoise(p);\n"
    "        p = p * 2.03 + vec2(17.0);\n"
    "        amp *= 0.5;\n"
    "    }\n"
    "    return v;\n"
    "}\n"
    "vec2 EngineSwirl(vec2 uv, vec2 center, float strength, float radius) {\n"
    "    vec2 d = uv - center;\n"
    "    float dist = length(d);\n"
    "    float k = strength * max(0.0, 1.0 - dist / radius);\n"
    "    float a = k * 3.14159 * 2.0;\n"
    "    float c = cos(a), s = sin(a);\n"
    "    return center + mat2(c, -s, s, c) * d;\n"
    "}\n"
    "vec3 EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {\n"
    "    return a + b * cos(6.28318 * (c * t + d));\n"
    "}\n"
    "vec3 EngineRainbow(float t) {\n"
    "    return EnginePalette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67));\n"
    "}\n"
    "float EnginePulse(float freq) {\n"
    "    return 0.5 + 0.5 * sin(u_Time * freq * 6.28318);\n"
    "}\n"
    "float EngineGrid(vec2 uv, float cells) {\n"
    "    vec2 g = abs(fract(uv * cells) - 0.5);\n"
    "    return 1.0 - smoothstep(0.44, 0.5, max(g.x, g.y));\n"
    "}\n"
    "float EngineVignette(vec2 uv, float strength) {\n"
    "    float d = distance(uv, vec2(0.5));\n"
    "    return clamp(1.0 - d * d * strength * 4.0, 0.0, 1.0);\n"
    "}\n";

// Пользователь мог оставить #version у себя — дубликат роняет компиляцию
static std::string StripVersionLine(const std::string& src) {
    if (src.rfind("#version", 0) == 0) {
        size_t nl = src.find('\n');
        return nl == std::string::npos ? std::string() : src.substr(nl + 1);
    }
    return src;
}

static std::string LoadShaderFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

Shader* Renderer::GetUserShader(const std::string& basePath) {
    auto it = m_UserShaderCache.find(basePath);
    if (it != m_UserShaderCache.end()) return it->second.get();
    if (m_FailedUserShaders.count(basePath) > 0) return nullptr;

    std::string vert = LoadShaderFile(basePath + ".vert");
    std::string frag = LoadShaderFile(basePath + ".frag");
    // Достаточно одного .frag: геометрию отдаст EngineQuadVert из шейдера по умолчанию
    if (vert.empty() && !frag.empty())
        vert = "void main() { EngineQuadVert(); }\n";
    if (vert.empty() || frag.empty()) {
        std::cerr << "[Shader] missing pair: " << basePath << ".vert/.frag\n";
        m_FailedUserShaders.insert(basePath);
        return nullptr;
    }

    std::string vs = std::string(kUserVertPrelude) + StripVersionLine(vert);
    std::string fs = std::string(kUserFragPrelude) + StripVersionLine(frag);
    auto shader = std::make_unique<Shader>(vs.c_str(), fs.c_str());
    if (!shader->IsValid()) {
        std::cerr << "[Shader] failed to build user shader: " << basePath << "\n";
        m_FailedUserShaders.insert(basePath);
        return nullptr;
    }
    Shader* raw = shader.get();
    m_UserShaderCache.emplace(basePath, std::move(shader));
    return raw;
}

// ============ SCENE RENDERING ============
void Renderer::BeginScene(Camera* camera, int width, int height) {
    m_ScreenW = width;
    m_ScreenH = height;
    camera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::EndScene() {}

// UV-прямоугольник текущего кадра спрайт-анимации (весь кадр, если анимации нет)
static glm::vec4 AnimationRect(const Entity& entity) {
    const SpriteAnimation& a = entity.animation;
    if (!a.active || a.cols < 1 || a.rows < 1) return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    const int total = a.cols * a.rows;
    long frame = static_cast<long>(std::floor(std::max(entity.animTime, 0.0f) * std::max(a.fps, 0.01f)));
    if (a.loop) frame %= total;
    else frame = std::min(frame, static_cast<long>(total - 1));
    const int col = static_cast<int>(frame % a.cols);
    const int row = static_cast<int>(frame / a.cols);
    return glm::vec4(col / static_cast<float>(a.cols),
                     1.0f - (row + 1) / static_cast<float>(a.rows),
                     1.0f / static_cast<float>(a.cols),
                     1.0f / static_cast<float>(a.rows));
}

void Renderer::RenderEntities(const std::vector<Entity>& entities, Camera* camera) {
    for (const auto& entity : entities) {
        if (!entity.active) continue;

        glm::mat4 model = Transforms::WorldMatrix(entities, entity);
        glm::mat4 mvp = camera->GetViewProjectionMatrix() * model;

        const bool animating = entity.animation.active &&
            (entity.animation.cols >= 1 && entity.animation.rows >= 1);
        const glm::vec4 uvRect = AnimationRect(entity);

        const std::string& texPath = animating && !entity.animation.texturePath.empty()
            ? entity.animation.texturePath : entity.sprite.texturePath;
        GLuint texture = texPath.empty() ? m_WhiteTexture : GetTexture(texPath);

        // Пользовательский шейдер рисует quad всегда, независимо от sprite.type
        if (!entity.sprite.shaderPath.empty()) {
            Shader* sh = GetUserShader(entity.sprite.shaderPath);
            if (sh) {
                sh->Use();
                sh->SetMat4("u_MVP", mvp);
                sh->SetMat4("u_Model", model);
                sh->SetMat4("u_ViewProj", camera->GetViewProjectionMatrix());
                sh->SetFloat("u_Time", static_cast<float>(glfwGetTime()));
                sh->SetVec2("u_ScreenSize", glm::vec2(m_ScreenW, m_ScreenH));
                sh->SetVec3("u_Color", entity.sprite.color);
                sh->SetVec4("u_UVRect", uvRect);
                sh->SetVec4("u_Params", entity.sprite.materialParams);
                sh->SetVec4("u_PColor", entity.sprite.materialColor);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture != 0 ? texture : m_WhiteTexture);
                sh->SetInt("u_Texture", 0);
                glBindVertexArray(m_QuadVAO);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                continue;
            }
        }

        if (entity.sprite.type == SpriteType::Quad) {
            if (texture != m_WhiteTexture && texture != 0) {
                m_SpriteTextureShader->Use();
                m_SpriteTextureShader->SetMat4("u_MVP", mvp);
                m_SpriteTextureShader->SetVec3("u_Color", entity.sprite.color);
                m_SpriteTextureShader->SetVec4("u_UVRect", uvRect);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);
                m_SpriteTextureShader->SetInt("u_Texture", 0);
            } else {
                m_SpriteShader->Use();
                m_SpriteShader->SetMat4("u_MVP", mvp);
                m_SpriteShader->SetVec3("u_Color", entity.sprite.color);
            }
            glBindVertexArray(m_QuadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        } else if (entity.sprite.type == SpriteType::Circle) {
            if (texture != m_WhiteTexture && texture != 0) {
                m_CircleTextureShader->Use();
                m_CircleTextureShader->SetMat4("u_MVP", mvp);
                m_CircleTextureShader->SetVec3("u_Color", entity.sprite.color);
                m_CircleTextureShader->SetVec4("u_UVRect", uvRect);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);
                m_CircleTextureShader->SetInt("u_Texture", 0);
            } else {
                m_CircleShader->Use();
                m_CircleShader->SetMat4("u_MVP", mvp);
                m_CircleShader->SetVec3("u_Color", entity.sprite.color);
            }
            glBindVertexArray(m_QuadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
    glBindVertexArray(0);
}

// ============ GIZMO ============
static void AppendSquare(std::vector<float>& lines, const glm::vec2& c, float half) {
    glm::vec2 p[4] = {
        c + glm::vec2(-half, -half), c + glm::vec2(half, -half),
        c + glm::vec2(half, half),   c + glm::vec2(-half, half)
    };
    for (int i = 0; i < 4; i++) {
        const glm::vec2& a = p[i];
        const glm::vec2& b = p[(i + 1) % 4];
        lines.insert(lines.end(), { a.x, a.y, b.x, b.y });
    }
}

void Renderer::RenderGizmo(const Entity* selectedEntity, const std::vector<Entity>& all,
                           Camera* camera, int screenW, int screenH, int mode, int activeAxis) {
    (void)screenW;
    if (!selectedEntity) return;

    glm::vec2 pos = Transforms::WorldPosition(all, *selectedEntity);

    // Постоянный размер на экране: мировая длина = пиксели * (мировых единиц на пиксель)
    float unitsPerPixel = 1080.0f * camera->GetZoom() / static_cast<float>(screenH > 0 ? screenH : 1);
    float len = 70.0f * unitsPerPixel;
    float handle = 6.0f * unitsPerPixel;

    const glm::vec3 colX(1.0f, 0.2f, 0.2f), colY(0.2f, 1.0f, 0.2f),
                    colAct(1.0f, 0.9f, 0.2f), colWhite(0.9f, 0.9f, 0.9f);

    glLineWidth(3.0f);
    m_LineShader->Use();
    m_LineShader->SetMat4("u_ViewProj", camera->GetViewProjectionMatrix());
    glBindBuffer(GL_ARRAY_BUFFER, m_GizmoVBO);
    glBindVertexArray(m_GizmoVAO);

    auto drawLines = [&](const std::vector<float>& lines, const glm::vec3& color) {
        m_LineShader->SetVec3("u_Color", color);
        glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<int>(lines.size() / 2));
    };

    if (mode == 0 || mode == 1) {
        // Оси X/Y (в Rotate — как ориентир)
        drawLines({ pos.x, pos.y, pos.x + len, pos.y }, activeAxis == 0 ? colAct : colX);
        drawLines({ pos.x, pos.y, pos.x, pos.y + len }, activeAxis == 1 ? colAct : colY);
    }

    if (mode == 1) {
        // Дуга вращения
        std::vector<float> circle;
        const int segments = 48;
        for (int i = 0; i < segments; i++) {
            float a1 = (float)i / segments * 6.2831853f;
            float a2 = (float)(i + 1) / segments * 6.2831853f;
            circle.insert(circle.end(), {
                pos.x + cosf(a1) * len, pos.y + sinf(a1) * len,
                pos.x + cosf(a2) * len, pos.y + sinf(a2) * len });
        }
        drawLines(circle, activeAxis == 2 ? colAct : colWhite);
    }

    if (mode == 2) {
        // Оси + квадраты на концах (X/Y-скейл) и квадрат в центре (равномерный)
        drawLines({ pos.x, pos.y, pos.x + len, pos.y }, activeAxis == 0 ? colAct : colX);
        drawLines({ pos.x, pos.y, pos.x, pos.y + len }, activeAxis == 1 ? colAct : colY);
        std::vector<float> sq;
        AppendSquare(sq, pos + glm::vec2(len, 0.0f), handle);
        drawLines(sq, activeAxis == 0 ? colAct : colX);
        sq.clear();
        AppendSquare(sq, pos + glm::vec2(0.0f, len), handle);
        drawLines(sq, activeAxis == 1 ? colAct : colY);
        sq.clear();
        AppendSquare(sq, pos, handle);
        drawLines(sq, activeAxis == 2 ? colAct : colWhite);
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void Renderer::RenderGizmoLines(const std::vector<float>& lines, const glm::vec3& color, Camera* camera) {
    m_LineShader->Use();
    m_LineShader->SetMat4("u_ViewProj", camera->GetViewProjectionMatrix());
    m_LineShader->SetVec3("u_Color", color);
    glBindBuffer(GL_ARRAY_BUFFER, m_GizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(m_GizmoVAO);
    glDrawArrays(GL_LINES, 0, static_cast<int>(lines.size() / 2));
    glBindVertexArray(0);
}

void Renderer::SetupColliderBuffers() {
    glGenVertexArrays(1, &m_ColliderVAO);
    glGenBuffers(1, &m_ColliderVBO);
    glBindVertexArray(m_ColliderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_ColliderVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::RenderColliders(const std::vector<Entity>& entities, Camera* camera) {
    for (const auto& entity : entities) {
        if (!entity.active || entity.collider.type == ColliderType::None) continue;

        ColliderPose p = Physics::WorldPose(entities, entity);
        glm::vec2 center = p.center;
        std::vector<float> lines;
        glm::vec3 color = entity.collider.isTrigger ? glm::vec3(0.0f, 1.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        if (entity.collider.type == ColliderType::Box) {
            glm::vec2 he = entity.collider.size * p.scale;
            float a = glm::radians(p.angleDeg);
            glm::vec2 u(std::cos(a), std::sin(a)), v(-std::sin(a), std::cos(a));
            glm::vec2 corners[4] = {
                center - u * he.x - v * he.y,
                center + u * he.x - v * he.y,
                center + u * he.x + v * he.y,
                center - u * he.x + v * he.y
            };
            for (int i = 0; i < 4; i++) {
                const glm::vec2& A = corners[i];
                const glm::vec2& B = corners[(i + 1) % 4];
                lines.insert(lines.end(), { A.x, A.y, B.x, B.y });
            }
        } else if (entity.collider.type == ColliderType::Circle) {
            // Круг как 16 линий
            int segments = 16;
            float radius = entity.collider.radius * std::max(p.scale.x, p.scale.y);
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * 6.28318f;
                float angle2 = (float)(i + 1) / segments * 6.28318f;
                lines.push_back(center.x + cos(angle1) * radius);
                lines.push_back(center.y + sin(angle1) * radius);
                lines.push_back(center.x + cos(angle2) * radius);
                lines.push_back(center.y + sin(angle2) * radius);
            }
        }

        if (!lines.empty()) {
            m_LineShader->Use();
            m_LineShader->SetMat4("u_ViewProj", camera->GetViewProjectionMatrix());
            m_LineShader->SetVec3("u_Color", color);
            glBindBuffer(GL_ARRAY_BUFFER, m_ColliderVBO);
            glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
            glBindVertexArray(m_ColliderVAO);
            glDrawArrays(GL_LINES, 0, static_cast<int>(lines.size() / 2));
        }
    }
    glBindVertexArray(0);
}
