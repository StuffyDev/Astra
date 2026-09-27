#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/SystemShaders.h"
#include "core/Prefs.h"
#include <glm/gtc/matrix_transform.hpp>
#include "utils/AssetIO.h"
#include "ecs/Entity.h"
#include "ecs/Physics.h"
#include "ecs/Transforms.h"
#include "utils/Shader.h"
#include "utils/Texture.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>

Renderer::Renderer() = default;
Renderer::~Renderer() { Shutdown(); }

static const char* kMesh3DVert = R"(
#version 460 core
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;
uniform mat4 u_VP;
uniform mat4 u_Model;
out vec3 v_Normal;
out vec3 v_World;
out vec2 v_UV;
void main() {
    vec4 w = u_Model * vec4(a_Pos, 1.0);
    v_World = w.xyz;
    v_Normal = mat3(u_Model) * a_Normal;
    v_UV = a_UV;
    gl_Position = u_VP * w;
}
)";

static const char* kMesh3DFrag = R"(
#version 460 core
in vec3 v_Normal;
in vec3 v_World;
in vec2 v_UV;
out vec4 FragColor;
uniform vec3 u_Color;
uniform sampler2D u_Texture;
uniform float u_UseTex;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_Ambient;
void main() {
    vec4 tex = mix(vec4(1.0), texture(u_Texture, v_UV), u_UseTex);
    vec3 n = normalize(v_Normal);
    float ndl = max(dot(n, normalize(u_LightDir)), 0.0);
    vec3 light = u_LightColor * ndl + vec3(u_Ambient);
    FragColor = vec4(u_Color * tex.rgb * light, tex.a);
}
)";


void Renderer::Init() {
    SetupGridBuffers();
    SetupQuad();
    SetupBatch();
    SetupGizmoBuffers();
    SetupColliderBuffers(); // <-- новое
    SetupWhiteTexture();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    CreateShaders();
    m_Mesh3DShader = std::make_unique<Shader>(kMesh3DVert, kMesh3DFrag);
    Setup3D();
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
    const float gridSize = AstraPrefs::GridSize;
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
    if (!AstraPrefs::ShowGrid) return;
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

// ================= 3D =================

struct Mesh3DData {
    std::vector<float> verts;
    std::vector<uint32_t> idx;
};

static void PushVert(std::vector<float>& v, float x, float y, float z,
                     float nx, float ny, float nz, float u, float vv) {
    v.insert(v.end(), { x, y, z, nx, ny, nz, u, vv });
}

static Mesh3DData BuildCube() {
    std::vector<float> vs; std::vector<uint32_t> is;
    struct F { glm::vec3 n; glm::vec3 a, b, c, d; };
    float s = 0.5f;
    F faces[6] = {
        { {0,0,1},  {-s,-s,s}, {s,-s,s}, {s,s,s}, {-s,s,s} },
        { {0,0,-1}, {s,-s,-s}, {-s,-s,-s}, {-s,s,-s}, {s,s,-s} },
        { {1,0,0},  {s,-s,s}, {s,-s,-s}, {s,s,-s}, {s,s,s} },
        { {-1,0,0}, {-s,-s,-s}, {-s,-s,s}, {-s,s,s}, {-s,s,-s} },
        { {0,1,0},  {-s,s,s}, {s,s,s}, {s,s,-s}, {-s,s,-s} },
        { {0,-1,0}, {-s,-s,-s}, {s,-s,-s}, {s,-s,s}, {-s,-s,s} },
    };
    float uv[4][2] = { {0,0}, {1,0}, {1,1}, {0,1} };
    for (int f = 0; f < 6; f++) {
        uint32_t base = (uint32_t)(vs.size() / 8);
        glm::vec3 faceVerts[4] = { faces[f].a, faces[f].b, faces[f].c, faces[f].d };
        for (int i = 0; i < 4; i++)
            PushVert(vs, faceVerts[i].x, faceVerts[i].y, faceVerts[i].z,
                     faces[f].n.x, faces[f].n.y, faces[f].n.z, uv[i][0], uv[i][1]);
        is.insert(is.end(), { base, base+1, base+2, base, base+2, base+3 });
    }
    return { vs, is };
}

static Mesh3DData BuildPlane() {
    std::vector<float> vs; std::vector<uint32_t> is;
    float s = 0.5f;
    PushVert(vs, -s, 0, s, 0,1,0, 0,1); PushVert(vs, s, 0, s, 0,1,0, 1,1);
    PushVert(vs, s, 0, -s, 0,1,0, 1,0); PushVert(vs, -s, 0, -s, 0,1,0, 0,0);
    is = { 0,1,2, 0,2,3 };
    return { vs, is };
}

static Mesh3DData BuildSphere() {
    std::vector<float> vs; std::vector<uint32_t> is;
    const int stacks = 16, slices = 24;
    for (int i = 0; i <= stacks; i++) {
        float phi = (float)i / stacks * 3.14159265f;
        for (int j = 0; j <= slices; j++) {
            float th = (float)j / slices * 2.0f * 3.14159265f;
            float x = sinf(phi) * cosf(th), y = cosf(phi), z = sinf(phi) * sinf(th);
            PushVert(vs, x*0.5f, y*0.5f, z*0.5f, x, y, z, (float)j/slices, 1.0f - (float)i/stacks);
        }
    }
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < slices; j++) {
            uint32_t a = i*(slices+1)+j, b = a + slices + 1;
            is.insert(is.end(), { a, b, a+1, b, b+1, a+1 });
        }
    return { vs, is };
}

Renderer::Mesh3D Renderer::MakeMesh3D(const std::vector<float>& verts, const std::vector<uint32_t>& idx) {
    Mesh3D m;
    m.indexCount = (int)idx.size();
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glBindVertexArray(0);
    return m;
}

const Renderer::Mesh3D& Renderer::GetObjMesh(const std::string& path) {
    static Mesh3D empty;
    auto it = m_ObjCache.find(path);
    if (it != m_ObjCache.end()) return it->second;
    Mesh3D out;
    std::ifstream f(path);
    if (f.is_open()) {
        std::vector<glm::vec3> pos, nrm;
        std::vector<glm::vec2> uvs;
        std::vector<float> verts; std::vector<uint32_t> idx;
        auto splitIdx = [](const std::string& s, int& a, int& b, int& c) {
            a = b = c = 0;
            size_t p1 = s.find('/'), p2 = p1 == std::string::npos ? std::string::npos : s.find('/', p1 + 1);
            try {
                a = std::stoi(s.substr(0, p1));
                if (p1 != std::string::npos) {
                    std::string mid = s.substr(p1 + 1, p2 == std::string::npos ? std::string::npos : p2 - p1 - 1);
                    if (!mid.empty()) b = std::stoi(mid);
                    if (p2 != std::string::npos) c = std::stoi(s.substr(p2 + 1));
                }
            } catch (...) {}
        };
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("v ", 0) == 0) {
                glm::vec3 v(0); sscanf(line.c_str() + 2, "%f %f %f", &v.x, &v.y, &v.z); pos.push_back(v);
            } else if (line.rfind("vn ", 0) == 0) {
                glm::vec3 v(0); sscanf(line.c_str() + 3, "%f %f %f", &v.x, &v.y, &v.z); nrm.push_back(v);
            } else if (line.rfind("vt ", 0) == 0) {
                glm::vec2 t(0); sscanf(line.c_str() + 3, "%f %f", &t.x, &t.y); uvs.push_back(t);
            } else if (line.rfind("f ", 0) == 0) {
                std::vector<uint32_t> poly;
                std::vector<char> noNrm;
                std::istringstream iss(line.substr(2));
                std::string tok;
                while (iss >> tok) {
                    int vi = 0, ti = 0, ni = 0; splitIdx(tok, vi, ti, ni);
                    uint32_t p = (uint32_t)(vi > 0 ? vi - 1 : (int)pos.size() + vi);
                    uint32_t tn = (uint32_t)(ti > 0 ? ti - 1 : 0), tn2 = (uint32_t)(ni > 0 ? ni - 1 : 0);
                    glm::vec3 P = p < pos.size() ? pos[p] : glm::vec3(0);
                    bool hasN = tn2 < nrm.size();
                    glm::vec3 N = hasN ? nrm[tn2] : glm::vec3(0, 1, 0);
                    // vt в OBJ — начало координат внизу, у движка (stb) — вверху
                    glm::vec2 T = tn < uvs.size() ? glm::vec2(uvs[tn].x, 1.0f - uvs[tn].y) : glm::vec2(0);
                    verts.insert(verts.end(), { P.x, P.y, P.z, N.x, N.y, N.z, T.x, T.y });
                    poly.push_back((uint32_t)(verts.size() / 8) - 1);
                    noNrm.push_back(hasN ? 0 : 1);
                }
                auto setNormalAt = [&](uint32_t c, const glm::vec3& n) {
                    verts[c * 8 + 3] = n.x; verts[c * 8 + 4] = n.y; verts[c * 8 + 5] = n.z;
                };
                for (size_t i = 2; i < poly.size(); i++) {
                    uint32_t a = poly[0], b = poly[i - 1], c = poly[i];
                    idx.insert(idx.end(), { a, b, c });
                    // каждая вершина веера уникальна (не делится на другие грани) —
                    // если vn нет, просто пишем нормаль этой грани
                    if (noNrm[0] || noNrm[i - 1] || noNrm[i]) {
                        glm::vec3 face = glm::cross(
                            glm::vec3(verts[b * 8] - verts[a * 8], verts[b * 8 + 1] - verts[a * 8 + 1],
                                      verts[b * 8 + 2] - verts[a * 8 + 2]),
                            glm::vec3(verts[c * 8] - verts[a * 8], verts[c * 8 + 1] - verts[a * 8 + 1],
                                      verts[c * 8 + 2] - verts[a * 8 + 2]));
                        if (glm::length(face) > 1e-8f) {
                            face = glm::normalize(face);
                            if (noNrm[0]) setNormalAt(a, face);
                            if (noNrm[i - 1]) setNormalAt(b, face);
                            if (noNrm[i]) setNormalAt(c, face);
                        }
                    }
                }
            }
        }
        if (!idx.empty()) out = MakeMesh3D(verts, idx);
    }
    auto res = m_ObjCache.emplace(path, out);
    return res.first->second;
}

void Renderer::Setup3D() {
    auto upload = [&](const Mesh3DData& d, Renderer::Mesh3D& out) { out = MakeMesh3D(d.verts, d.idx); };
    upload(BuildCube(), m_PrimCube);
    upload(BuildPlane(), m_PrimPlane);
    upload(BuildSphere(), m_PrimSphere);
}

void Renderer::RenderEntities3D(const std::vector<Entity>& entities, Camera* camera) {
    bool any = false;
    for (const auto& e : entities) if (e.is3D && e.active) { any = true; break; }
    if (!any) return;

        glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    m_Mesh3DShader->Use();
    m_Mesh3DShader->SetMat4("u_VP", camera->GetViewProjectionMatrix());
    glm::vec3 ld = glm::length(AstraPrefs::LightDir) > 0.0f ? glm::normalize(AstraPrefs::LightDir) : glm::vec3(0,1,0);
    m_Mesh3DShader->SetVec3("u_LightDir", ld);
    m_Mesh3DShader->SetVec3("u_LightColor", AstraPrefs::LightColor);
    m_Mesh3DShader->SetFloat("u_Ambient", AstraPrefs::Ambient);
    m_Mesh3DShader->SetInt("u_Texture", 0);

    for (const auto& e : entities) {
        if (!e.is3D || !e.active) continue;
        glm::mat4 model(1.0f);
        model = glm::translate(model, e.pos3);
        model = glm::rotate(model, glm::radians(e.rot3.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(e.rot3.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(e.rot3.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, e.scale3);
        m_Mesh3DShader->SetMat4("u_Model", model);
        m_Mesh3DShader->SetVec3("u_Color", e.mesh.color);

        const Mesh3D* mesh = nullptr;
        switch (e.mesh.type) {
            case 1: mesh = &m_PrimPlane; break;
            case 2: mesh = &m_PrimSphere; break;
            case 3: mesh = &GetObjMesh(e.mesh.meshPath); break;
            default: mesh = &m_PrimCube; break;
        }
        if (!mesh || mesh->indexCount == 0) continue;

        GLuint tex = e.mesh.texturePath.empty() ? m_WhiteTexture : GetTexture(e.mesh.texturePath);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex ? tex : m_WhiteTexture);
        m_Mesh3DShader->SetFloat("u_UseTex", e.mesh.texturePath.empty() ? 0.0f : (tex ? 1.0f : 0.0f));
        glBindVertexArray(mesh->vao);
        glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
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
    return AssetIO::ReadAll(path);   // прозрачно расшифровывает билд-ассеты
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
    glClearColor(AstraPrefs::ClearColor.r, AstraPrefs::ClearColor.g, AstraPrefs::ClearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::EndScene() {}

// UV-прямоугольник текущего кадра спрайт-анимации (весь кадр, если анимации нет)
static glm::vec4 AnimationRect(const Entity& entity) {
    const SpriteAnimation& a = entity.animation;
    if (!a.active || a.cols < 1 || a.rows < 1) return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int first = 0, last = a.cols * a.rows - 1, count = a.cols * a.rows;
    float fps = a.fps;
    bool loop = a.loop;
    if (!a.clips.empty()) {
        const AnimClip& c = a.clips[std::clamp(a.activeClip, 0, (int)a.clips.size() - 1)];
        first = std::clamp(c.first, 0, a.cols * a.rows - 1);
        last = std::clamp(c.last, first, a.cols * a.rows - 1);
        count = last - first + 1;
        fps = c.fps;
        loop = c.loop;
    }
    long frame = static_cast<long>(std::floor(std::max(entity.animTime, 0.0f) * std::max(fps, 0.01f)));
    if (loop) frame = first + frame % count;
    else frame = std::min(first + frame, static_cast<long>(last));
    const int col = static_cast<int>(frame % a.cols);
    const int row = static_cast<int>(frame / a.cols);
    return glm::vec4(col / static_cast<float>(a.cols),
                     1.0f - (row + 1) / static_cast<float>(a.rows),
                     1.0f / static_cast<float>(a.cols),
                     1.0f / static_cast<float>(a.rows));
}

void Renderer::RenderEntities(const std::vector<Entity>& entities, Camera* camera) {
    // Порядок отрисовки: sortingOrder (меньше — раньше), стабильно внутри уровня
    std::vector<const Entity*> ordered;
    ordered.reserve(entities.size());
    for (const auto& e : entities) ordered.push_back(&e);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Entity* a, const Entity* b) {
                         return a->sprite.sortingOrder < b->sprite.sortingOrder;
                     });
    for (const Entity* entityPtr : ordered) {
        const Entity& entity = *entityPtr;
        if (!entity.active || entity.is3D) continue;

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
                m_SpriteTextureShader->SetFloat("u_Alpha", 1.0f);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);
                m_SpriteTextureShader->SetInt("u_Texture", 0);
            } else {
                m_SpriteShader->Use();
                m_SpriteShader->SetMat4("u_MVP", mvp);
                m_SpriteShader->SetVec3("u_Color", entity.sprite.color);
                m_SpriteShader->SetFloat("u_Alpha", 1.0f);
            }
            glBindVertexArray(m_QuadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        } else if (entity.sprite.type == SpriteType::Circle) {
            if (texture != m_WhiteTexture && texture != 0) {
                m_CircleTextureShader->Use();
                m_CircleTextureShader->SetMat4("u_MVP", mvp);
                m_CircleTextureShader->SetVec3("u_Color", entity.sprite.color);
                m_CircleTextureShader->SetVec4("u_UVRect", uvRect);
                m_CircleTextureShader->SetFloat("u_Alpha", 1.0f);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);
                m_CircleTextureShader->SetInt("u_Texture", 0);
            } else {
                m_CircleShader->Use();
                m_CircleShader->SetMat4("u_MVP", mvp);
                m_CircleShader->SetVec3("u_Color", entity.sprite.color);
                m_CircleShader->SetFloat("u_Alpha", 1.0f);
            }
            glBindVertexArray(m_QuadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
    glBindVertexArray(0);
}

glm::ivec2 Renderer::GetTextureSize(const std::string& path) {
    auto it = m_TextureCache.find(path);
    if (it == m_TextureCache.end()) return glm::ivec2(0, 0);
    return glm::ivec2(it->second->GetWidth(), it->second->GetHeight());
}

void Renderer::SetupBatch() {
    glGenVertexArrays(1, &m_BatchVAO);
    glBindVertexArray(m_BatchVAO);
    glGenBuffers(1, &m_BatchVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

static void AppendBatchQuad(std::vector<float>& v,
                            float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1) {
    // два треугольника; uv: v0 — верх, v1 — низ (текстура загружена flip=true)
    const float verts[6][4] = {
        { x0, y1, u0, v1 }, { x1, y1, u1, v1 }, { x1, y0, u1, v0 },
        { x0, y1, u0, v1 }, { x1, y0, u1, v0 }, { x0, y0, u0, v0 },
    };
    for (auto& q : verts) v.insert(v.end(), q, q + 4);
}

void Renderer::RenderTilemaps(const std::vector<Entity>& entities, Camera* camera) {
    glm::mat4 vp = camera->GetViewProjectionMatrix();
    m_SpriteTextureShader->Use();
    m_SpriteTextureShader->SetMat4("u_MVP", vp);
    m_SpriteTextureShader->SetVec4("u_UVRect", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_SpriteTextureShader->SetInt("u_Texture", 0);
    glBindVertexArray(m_BatchVAO);
    for (const auto& e : entities) {
        if (!e.hasTilemap || !e.active || e.tilemap.texturePath.empty()) continue;
        const Tilemap& tm = e.tilemap;
        GLuint tex = GetTexture(tm.texturePath);
        if (!tex) continue;
        glm::ivec2 texSize = GetTextureSize(tm.texturePath);
        const int colsTotal = std::max(1, texSize.x / std::max(tm.tileW, 1));
        const int rowsTotal = std::max(1, texSize.y / std::max(tm.tileH, 1));
        const int need = (int)(tm.width * tm.height);
        if ((int)tm.cells.size() < need) continue;
        float x0 = e.transform.position.x, yTop = e.transform.position.y;
        std::vector<float> verts;
        verts.reserve((size_t)need * 24);
        for (int r = 0; r < tm.height; r++) {
            for (int c = 0; c < tm.width; c++) {
                int idx = tm.cells[(size_t)r * tm.width + c];
                if (idx < 0) continue;
                int ac = (idx % std::max(tm.atlasCols, 1));
                int ar = idx / std::max(tm.atlasCols, 1);
                if (ac >= colsTotal || ar >= rowsTotal) continue;
                float u0 = ac / (float)colsTotal, u1 = (ac + 1) / (float)colsTotal;
                float vT = 1.0f - ar / (float)rowsTotal, vB = 1.0f - (ar + 1) / (float)rowsTotal;
                float qx0 = x0 + c * tm.tileW, qx1 = qx0 + tm.tileW;
                float qy1 = yTop - r * tm.tileH, qy0 = qy1 - tm.tileH;
                AppendBatchQuad(verts, qx0, qy0, qx1, qy1, u0, vT, u1, vB);
            }
        }
        if (verts.empty()) continue;
        m_SpriteTextureShader->SetVec3("u_Color", glm::vec3(tm.color));
        m_SpriteTextureShader->SetFloat("u_Alpha", tm.color.a);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size() / 4);
    }
    glBindVertexArray(0);
}

void Renderer::RenderParticles(const std::vector<Entity>& entities, Camera* camera) {
    glm::mat4 vp = camera->GetViewProjectionMatrix();
    m_SpriteTextureShader->Use();
    m_SpriteTextureShader->SetMat4("u_MVP", vp);
    m_SpriteTextureShader->SetVec4("u_UVRect", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_SpriteTextureShader->SetInt("u_Texture", 0);
    glBindVertexArray(m_BatchVAO);
    for (const auto& e : entities) {
        if (!e.active || e.particles.empty()) continue;
        GLuint tex = e.emitter.texturePath.empty() ? m_WhiteTexture : GetTexture(e.emitter.texturePath);
        if (!tex) tex = m_WhiteTexture;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        for (const auto& pt : e.particles) {
            float t = pt.life > 0.0f ? std::clamp(pt.age / pt.life, 0.0f, 1.0f) : 1.0f;
            glm::vec4 col = glm::mix(e.emitter.colorStart, e.emitter.colorEnd, t);
            float h = pt.size * 0.5f;
            std::vector<float> verts;
            AppendBatchQuad(verts, pt.position.x - h, pt.position.y - h,
                            pt.position.x + h, pt.position.y + h, 0.0f, 1.0f, 1.0f, 0.0f);
            m_SpriteTextureShader->SetVec3("u_Color", glm::vec3(col));
            m_SpriteTextureShader->SetFloat("u_Alpha", col.a);
            glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
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
    if (!AstraPrefs::ShowColliders) return;
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
