#include "utils/Texture.h"
#include "utils/AssetIO.h"
#include <stb_image.h>
#include <iostream>
#include <vector>

Texture::Texture(Texture&& other) noexcept
    : m_ID(other.m_ID), m_Width(other.m_Width), m_Height(other.m_Height) {
    other.m_ID = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Free();
        m_ID = other.m_ID;
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        other.m_ID = 0;
    }
    return *this;
}

bool Texture::LoadFromFile(const std::string& path) {
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    std::vector<unsigned char> bytes;
    if (!AssetIO::ReadBytes(path, bytes)) {
        std::cerr << "Failed to read texture: " << path << "\n";
        return false;
    }
    unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                &m_Width, &m_Height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << " (" << stbi_failure_reason() << ")\n";
        return false;
    }

    if (m_ID == 0) glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void Texture::Free() {
    if (m_ID != 0) {
        glDeleteTextures(1, &m_ID);
        m_ID = 0;
    }
}
