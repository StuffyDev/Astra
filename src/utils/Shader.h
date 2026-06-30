#pragma once
#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void Use() const;

    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetFloat(const std::string& name, float value) const;

private:
    GLuint m_ID;

    std::string LoadFile(const std::string& path);
    GLuint CompileShader(GLenum type, const std::string& source);
    void LinkProgram(GLuint vertex, GLuint fragment);
};
