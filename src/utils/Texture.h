#pragma once
#include <glad/glad.h>
#include <string>

class Texture {
public:
    Texture() = default;
    ~Texture() { Free(); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    bool LoadFromFile(const std::string& path);
    void Free();

    GLuint GetID() const { return m_ID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

private:
    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
};
