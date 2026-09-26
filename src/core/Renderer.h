#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Shader;
class Camera;
class Texture;
struct Entity;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();
    // Очистить кэш текстур проекта (при смене проекта)
    void ClearProjectCaches();

    void BeginScene(Camera* camera, int width, int height);
    void RenderGrid(Camera* camera);
    void RenderEntities(const std::vector<Entity>& entities, Camera* camera);
    void RenderColliders(const std::vector<Entity>& entities, Camera* camera);
    // mode: 0 = Move, 1 = Rotate, 2 = Scale; activeAxis: -1 = не перетаскивать (0=X,1=Y,2=центр/дуга)
    void RenderGizmo(const Entity* selectedEntity, const std::vector<Entity>& all,
                     Camera* camera, int screenW, int screenH, int mode, int activeAxis);
    void EndScene();

    void RenderGizmoLines(const std::vector<float>& lines, const glm::vec3& color, Camera* camera);

    // Возвращает GL-текстуру из кэша; 0, если загрузка не удалась
    GLuint GetTexture(const std::string& path);

    // Пользовательский шейдер пары <basePath>.vert/.frag; nullptr, если собрать не удалось
    Shader* GetUserShader(const std::string& basePath);

    void Shutdown();

private:
    std::unique_ptr<Shader> m_LineShader;
    std::unique_ptr<Shader> m_SpriteShader;
    std::unique_ptr<Shader> m_CircleShader;
    std::unique_ptr<Shader> m_SpriteTextureShader;
    std::unique_ptr<Shader> m_CircleTextureShader;

    GLuint m_GridVAO = 0, m_GridVBO = 0;
    GLuint m_QuadVAO = 0, m_QuadVBO = 0, m_QuadEBO = 0;
    GLuint m_GizmoVAO = 0, m_GizmoVBO = 0;
    GLuint m_ColliderVAO = 0, m_ColliderVBO = 0;
    GLuint m_WhiteTexture = 0;

    std::unordered_map<std::string, std::unique_ptr<Texture>> m_TextureCache;
    std::unordered_set<std::string> m_FailedTextures;
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_UserShaderCache;
    std::unordered_set<std::string> m_FailedUserShaders;

    int m_GridCount = 0;
    int m_ScreenW = 0, m_ScreenH = 0;

    void SetupGridBuffers();
    void UpdateGrid(Camera* camera);
    void SetupQuad();
    void SetupGizmoBuffers();
    void SetupColliderBuffers();
    void SetupWhiteTexture();
    void CreateShaders();
};
