#pragma once
#include <glad/glad.h>

class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();

    void Resize(int width, int height);
    void Bind();
    void Unbind();

    GLuint GetTexture() const { return m_Texture; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

private:
    GLuint m_FBO = 0;
    GLuint m_Texture = 0;
    GLuint m_RBO = 0;
    int m_Width, m_Height;

    void Create();
};
