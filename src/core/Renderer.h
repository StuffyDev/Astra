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
    // Пол на XZ в 3D-сцене (+ цветные оси X/Z)
    void RenderGrid3D(Camera* camera);
    void RenderEntities(const std::vector<Entity>& entities, Camera* camera);
    void RenderTilemaps(const std::vector<Entity>& entities, Camera* camera);
    void RenderEntities3D(const std::vector<Entity>& entities, Camera* camera);
    // 3D-гизмо: оси (mode 0), кольца вращения (1) или оси с ручками масштаба (2).
    // len — длина оси в мировых единицах; grabbed: -1 нет, 0..2 ось, 3 центр/равномерно
    void RenderGizmo3D(const glm::vec3& center, float len, int mode, int grabbed, Camera* camera);
    // Границы меша в ЛОКАЛЬНЫХ координатах (центр + половинный размер) — для выделения кликом
    bool GetMeshBounds(int meshType, const std::string& objPath, glm::vec3& center, glm::vec3& half);
    void RenderParticles(const std::vector<Entity>& entities, Camera* camera);
    void RenderColliders(const std::vector<Entity>& entities, Camera* camera);
    // mode: 0 = Move, 1 = Rotate, 2 = Scale; activeAxis: -1 = не перетаскивать (0=X,1=Y,2=центр/дуга)
    void RenderGizmo(const Entity* selectedEntity, const std::vector<Entity>& all,
                     Camera* camera, int screenW, int screenH, int mode, int activeAxis);
    void EndScene();

    void RenderGizmoLines(const std::vector<float>& lines, const glm::vec3& color, Camera* camera);

    // Возвращает GL-текстуру из кэша; 0, если загрузка не удалась
    GLuint GetTexture(const std::string& path);
    glm::ivec2 GetTextureSize(const std::string& path); // (0,0) если нет

    // Пользовательский шейдер пары <basePath>.vert/.frag; nullptr, если собрать не удалось
    Shader* GetUserShader(const std::string& basePath);

    void Shutdown();

private:
    std::unique_ptr<Shader> m_LineShader;
    std::unique_ptr<Shader> m_SpriteShader;
    std::unique_ptr<Shader> m_CircleShader;
    std::unique_ptr<Shader> m_SpriteTextureShader;
    std::unique_ptr<Shader> m_CircleTextureShader;
    std::unique_ptr<Shader> m_Mesh3DShader;
    std::unique_ptr<Shader> m_ShadowShader;
    GLuint m_ShadowFBO = 0, m_ShadowTex = 0;
    int m_ShadowTexSize = 0;
    void SetupShadowMap(int size);
    // Орто-камера солнца, подогнанная под габариты 3D-контента
    glm::mat4 LightViewProj(const std::vector<Entity>& entities, float& outRadius);
    void RenderShadowMap(const std::vector<Entity>& entities, const glm::mat4& lightVP);
    std::unique_ptr<Shader> m_Line3DShader;

    struct Mesh3D {
        GLuint vao = 0, vbo = 0, ebo = 0;
        int indexCount = 0;
        glm::vec3 boundsMin = glm::vec3(0.0f);
        glm::vec3 boundsMax = glm::vec3(0.0f);
    };
    Mesh3D m_PrimCube, m_PrimPlane, m_PrimSphere;
    std::unordered_map<std::string, Mesh3D> m_ObjCache;
    Mesh3D MakeMesh3D(const std::vector<float>& verts, const std::vector<uint32_t>& idx);
    const Mesh3D& GetObjMesh(const std::string& path);
    const Mesh3D* MeshForEntity(const Entity& e);

    GLuint m_GridVAO = 0, m_GridVBO = 0;
    GLuint m_QuadVAO = 0, m_QuadVBO = 0, m_QuadEBO = 0;
    GLuint m_BatchVAO = 0, m_BatchVBO = 0;
    GLuint m_GizmoVAO = 0, m_GizmoVBO = 0;
    GLuint m_Gizmo3DVAO = 0, m_Gizmo3DVBO = 0;
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
    void SetupBatch();
    void Setup3D();
    void SetupGizmo3DBuffers();
    void RenderLines3D(const std::vector<float>& xyz, const glm::vec3& color, Camera* camera);
    void SetupGizmoBuffers();
    void SetupColliderBuffers();
    void SetupWhiteTexture();
    void CreateShaders();
};
