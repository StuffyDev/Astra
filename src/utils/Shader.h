#pragma once
#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    // Прямая компиляция из исходников в памяти (системные шейдеры)
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();

    void Use() const;

    // false, если программа не слинковалась (ошибка уже залогирована)
    bool IsValid() const { return m_ID != 0; }

    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetInt(const std::string& name, int value) const;

private:
    GLuint m_ID;

    std::string LoadFile(const std::string& path);
    GLuint CompileShader(GLenum type, const std::string& source);
    void LinkProgram(GLuint vertex, GLuint fragment);
};
