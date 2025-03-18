#include "OpenGLTexture.h"
#include "../../Logger/Log.h"
#include <stb_image.h>

namespace Rapture {

OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
    : m_path(path)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    
    if (data) {
        m_width = width;
        m_height = height;
        
        GLenum internalFormat = 0, dataFormat = 0;
        if (channels == 4) {
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
        }
        else if (channels == 3) {
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
        }
        
        m_internalFormat = internalFormat;
        m_dataFormat = dataFormat;
        
        if (internalFormat == 0 || dataFormat == 0) {
            GE_CORE_ERROR("OpenGLTexture2D: Unsupported format! Channels: {0}", channels);
            stbi_image_free(data);
            return;
        }
        
        glGenTextures(1, &m_rendererID);
        glBindTexture(GL_TEXTURE_2D, m_rendererID);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        stbi_image_free(data);
        
        GE_CORE_INFO("Loaded texture '{0}' ({1}x{2}, {3} channels)", path, m_width, m_height, channels);
    }
    else {
        GE_CORE_ERROR("Failed to load texture '{0}'", path);
    }
}

OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
    : m_width(width), m_height(height), m_internalFormat(GL_RGBA8), m_dataFormat(GL_RGBA)
{
    glGenTextures(1, &m_rendererID);
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, m_dataFormat, GL_UNSIGNED_BYTE, nullptr);
    
    GE_CORE_INFO("Created blank texture ({0}x{1})", m_width, m_height);
}

OpenGLTexture2D::~OpenGLTexture2D()
{
    glDeleteTextures(1, &m_rendererID);
}

void OpenGLTexture2D::bind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
}

void OpenGLTexture2D::unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLTexture2D::setData(void* data, uint32_t size)
{
    uint32_t bytesPerPixel = m_dataFormat == GL_RGBA ? 4 : 3;
    if (size != m_width * m_height * bytesPerPixel) {
        GE_CORE_ERROR("OpenGLTexture2D::setData: Data size doesn't match texture size!");
        return;
    }
    
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_dataFormat, GL_UNSIGNED_BYTE, data);
}

// Implement the static create functions from Texture2D
std::shared_ptr<Texture2D> Texture2D::create(const std::string& path)
{
    return std::make_shared<OpenGLTexture2D>(path);
}

std::shared_ptr<Texture2D> Texture2D::create(uint32_t width, uint32_t height)
{
    return std::make_shared<OpenGLTexture2D>(width, height);
}

} // namespace Rapture 