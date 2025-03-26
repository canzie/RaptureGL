#include "OpenGLTexture.h"
#include "../../Logger/Log.h"
#include "../../Debug/Profiler.h"
#include <stb_image.h>

namespace Rapture {

OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
    : m_path(path)
{
    RAPTURE_PROFILE_FUNCTION();
    
    int width, height, channels;
    
    // Profile stbi_load specifically
    unsigned char* data = nullptr;
    {
        RAPTURE_PROFILE_SCOPE("stbi_load - Texture Loading");
        stbi_set_flip_vertically_on_load(0);
        data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    }

    if (data) {
        // Profile the OpenGL texture creation
        RAPTURE_PROFILE_SCOPE("OpenGL Texture Creation");
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
    }
    else {
        GE_CORE_ERROR("Failed to load texture '{0}'", path);
    }
}

OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, uint32_t channels)
    : m_width(width), m_height(height), m_internalFormat(0), m_dataFormat(0)
{
    RAPTURE_PROFILE_FUNCTION();

    if (channels == 4) {
        m_internalFormat = GL_RGBA8;
        m_dataFormat = GL_RGBA;
    }
    else if (channels == 3) {
        m_internalFormat = GL_RGB8;
        m_dataFormat = GL_RGB;
    }
    else {
        GE_CORE_ERROR("OpenGLTexture2D: Unsupported format! Channels: {0}", channels);
        return;
    }

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
    RAPTURE_PROFILE_FUNCTION();

    uint32_t bytesPerPixel = m_dataFormat == GL_RGBA ? 4 : 3;
    if (size != m_width * m_height * bytesPerPixel) {
        GE_CORE_ERROR("OpenGLTexture2D::setData: Data size doesn't match texture size!");
        return;
    }
    
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);



}

void OpenGLTexture2D::setMinFilter(TextureFilter filter)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convertFilterToGL(filter));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLTexture2D::setMagFilter(TextureFilter filter)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    // Note: Mag filter can only be GL_NEAREST or GL_LINEAR
    GLenum glFilter = convertFilterToGL(filter);
    if (glFilter != GL_NEAREST && glFilter != GL_LINEAR) {
        GE_CORE_WARN("OpenGLTexture2D: Mag filter can only be Nearest or Linear. Using Linear instead.");
        glFilter = GL_LINEAR;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLTexture2D::setWrapS(TextureWrap wrap)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, convertWrapToGL(wrap));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLTexture2D::setWrapT(TextureWrap wrap)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, convertWrapToGL(wrap));
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLenum OpenGLTexture2D::convertFilterToGL(TextureFilter filter)
{
    switch (filter) {
        case TextureFilter::Nearest:              return GL_NEAREST;
        case TextureFilter::Linear:               return GL_LINEAR;
        case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
        case TextureFilter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
        case TextureFilter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
        case TextureFilter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
        default:
            GE_CORE_WARN("OpenGLTexture2D: Unknown filter type, defaulting to Linear");
            return GL_LINEAR;
    }
}

GLenum OpenGLTexture2D::convertWrapToGL(TextureWrap wrap)
{
    switch (wrap) {
        case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case TextureWrap::Repeat:         return GL_REPEAT;
        default:
            GE_CORE_WARN("OpenGLTexture2D: Unknown wrap type, defaulting to Repeat");
            return GL_REPEAT;
    }
}

// Implement the static create functions from Texture2D
std::shared_ptr<Texture2D> Texture2D::create(const std::string& path)
{
    return std::make_shared<OpenGLTexture2D>(path);
}

std::shared_ptr<Texture2D> Texture2D::create(uint32_t width, uint32_t height, uint32_t channels)
{
    return std::make_shared<OpenGLTexture2D>(width, height, channels);
}

} // namespace Rapture 