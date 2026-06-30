#include "core/Renderer.h"
#include "core/Camera.h"
#include "ecs/Entity.h"
#include "utils/Shader.h"
#include <vector>
#include <cmath>

Renderer::Renderer() = default;
Renderer::~Renderer() { Shutdown(); }

void Renderer::Init() {
    SetupGridBuffers();
    SetupQuad();
    SetupGizmoBuffers();

    m_LineShader = std::make_unique<Shader>("assets/shaders/line.vert", "assets/shaders/line.frag");
    m_SpriteShader = std::make_unique<Shader>("assets/shaders/sprite.vert", "assets/shaders/sprite.frag");
    m_CircleShader = std::make_unique<Shader>("assets/shaders/sprite.vert", "assets/shaders/circle.frag");
}

void Renderer::Shutdown() {
    glDeleteVertexArrays(1, &m_GridVAO);
    glDeleteBuffers(1, &m_GridVBO);
    glDeleteVertexArrays(1, &m_QuadVAO);
    glDeleteBuffers(1, &m_QuadVBO);
    glDeleteBuffers(1, &m_QuadEBO);
    glDeleteVertexArrays(1, &m_GizmoVAO);
    glDeleteBuffers(1, &m_GizmoVBO);
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

// ============ SCENE RENDERING ============
void Renderer::BeginScene(Camera* camera, int width, int height) {
    m_ScreenW = width;
    m_ScreenH = height;
    camera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::EndScene() {}

void Renderer::RenderEntities(const std::vector<Entity>& entities, Camera* camera) {
    for (const auto& entity : entities) {
        if (!entity.active) continue;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(entity.transform.position, 0.0f));
        model = glm::rotate(model, glm::radians(entity.transform.rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(entity.transform.scale, 1.0f));

        glm::mat4 mvp = camera->GetViewProjectionMatrix() * model;

        if (entity.sprite.type == SpriteType::Quad) {
            m_SpriteShader->Use();
            m_SpriteShader->SetMat4("u_MVP", mvp);
            m_SpriteShader->SetVec3("u_Color", entity.sprite.color);
            glBindVertexArray(m_QuadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        } else if (entity.sprite.type == SpriteType::Circle) {
            m_CircleShader->Use();
            m_CircleShader->SetMat4("u_MVP", mvp);
            m_CircleShader->SetVec3("u_Color", entity.sprite.color);
            glBindVertexArray(m_QuadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
    glBindVertexArray(0);
}

// ============ GIZMO ============
void Renderer::RenderGizmo(const Entity* selectedEntity, Camera* camera, int screenW, int screenH) {
    if (!selectedEntity) return;

    glm::vec2 pos = selectedEntity->transform.position;
    float scale = selectedEntity->transform.scale.x * 0.5f + 30.0f * camera->GetZoom();

    // Рендерим оси X (красный) и Y (зелёный)
    std::vector<float> xAxis = {
        pos.x, pos.y,
        pos.x + scale, pos.y
    };
    std::vector<float> yAxis = {
        pos.x, pos.y,
        pos.x, pos.y + scale
    };

    // X axis - red
    glLineWidth(3.0f);
    m_LineShader->Use();
    m_LineShader->SetMat4("u_ViewProj", camera->GetViewProjectionMatrix());
    m_LineShader->SetVec3("u_Color", glm::vec3(1.0f, 0.2f, 0.2f));
    glBindBuffer(GL_ARRAY_BUFFER, m_GizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, xAxis.size() * sizeof(float), xAxis.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(m_GizmoVAO);
    glDrawArrays(GL_LINES, 0, 2);

    // Y axis - green
    m_LineShader->SetVec3("u_Color", glm::vec3(0.2f, 1.0f, 0.2f));
    glBufferData(GL_ARRAY_BUFFER, yAxis.size() * sizeof(float), yAxis.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, 2);

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
