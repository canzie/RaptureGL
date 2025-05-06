#include "OpenGLTexture.h"
#include "../../Logger/Log.h"
#include "../../Debug/TracyProfiler.h"
#include <stb_image.h>

#include "../../Utils/GLCapabilities.h"

#include "OpenGLFormatConverters.h"

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
        
        // Generate handle for bindless textures if supported
        if (GLCapabilities::hasBindlessTextures()) {
            generateTextureHandle();
        }
    }
    else {
        GE_CORE_ERROR("OpenGLTexture2D::OpenGLTexture2D - Failed to load texture '{0}'", path);
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
    
    // Generate handle for bindless textures if supported
    if (GLCapabilities::hasBindlessTextures()) {
        generateTextureHandle();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    
    GE_CORE_INFO("Created blank texture ({0}x{1})", m_width, m_height);
}

OpenGLTexture2D::OpenGLTexture2D(TextureSpecification specification)
{
    RAPTURE_PROFILE_FUNCTION();

    m_internalFormat = TextureFormatToGL(specification.format);
    m_dataFormat = TextureFormatToGLDataFormat(specification.format);

    if(specification.width == 0 || specification.height == 0)
    {
        GE_CORE_ERROR("OpenGLTexture2D: Width and height must be greater than 0");
        return;
    }

    m_width = specification.width;
    m_height = specification.height;

    glGenTextures(1, &m_rendererID);
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, m_dataFormat, TextureFormatToGLDataType(specification.format), nullptr);
    
    // Generate handle for bindless textures if supported
    if (GLCapabilities::hasBindlessTextures()) {
        generateTextureHandle();
    }


    
    GE_CORE_INFO("Created blank texture ({0}x{1})", m_width, m_height);
}

OpenGLTexture2D::~OpenGLTexture2D()
{
    // Make the texture non-resident if it is resident
    if (m_isResident) {
        makeNonResident();
    }
    
    glDeleteTextures(1, &m_rendererID);
}

void OpenGLTexture2D::bind(uint32_t slot) const
{
    RAPTURE_PROFILE_GPU_SCOPE("OpenGLTexture2D::bind");
    if (m_isCubemap) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_rendererID);
    } else {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_rendererID);
    }
}

void OpenGLTexture2D::unbind() const
{
    if (m_isCubemap) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void OpenGLTexture2D::bindCompute(uint32_t slot) const
{
    glBindImageTexture(slot, m_rendererID, 0, GL_FALSE, 0, GL_READ_WRITE, m_internalFormat);
}

void OpenGLTexture2D::unbindCompute() const
{
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, 0);
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
    
    // Re-generate the texture handle if bindless is supported
    if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
        // Make non-resident first if needed
        if (m_isResident) {
            makeNonResident();
        }
        
        // Generate a new handle
        generateTextureHandle();
        
        // Make resident again if it was resident before
        if (m_isResident) {
            makeResident();
        }
    }
}

void OpenGLTexture2D::barrier() const
{
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void OpenGLTexture2D::setMinFilter(TextureFilter filter)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convertFilterToGL(filter));
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Re-generate handle if using bindless
    if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
        bool wasResident = m_isResident;
        if (wasResident) makeNonResident();
        generateTextureHandle();
        if (wasResident) makeResident();
    }
}

void OpenGLTexture2D::setMagFilter(TextureFilter filter)
{
    // Note: Mag filter can only be GL_NEAREST or GL_LINEAR
    GLenum glFilter = convertFilterToGL(filter);
    if (glFilter != GL_NEAREST && glFilter != GL_LINEAR) {
        GE_CORE_WARN("OpenGLTexture2D: Mag filter can only be Nearest or Linear. Using Linear instead.");
        glFilter = GL_LINEAR;
    }
    
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Re-generate handle if using bindless
    if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
        bool wasResident = m_isResident;
        if (wasResident) makeNonResident();
        generateTextureHandle();
        if (wasResident) makeResident();
    }
}

void OpenGLTexture2D::setWrapS(TextureWrap wrap)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, convertWrapToGL(wrap));
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Re-generate handle if using bindless
    if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
        bool wasResident = m_isResident;
        if (wasResident) makeNonResident();
        generateTextureHandle();
        if (wasResident) makeResident();
    }
}

void OpenGLTexture2D::setWrapT(TextureWrap wrap)
{
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, convertWrapToGL(wrap));
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Re-generate handle if using bindless
    if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
        bool wasResident = m_isResident;
        if (wasResident) makeNonResident();
        generateTextureHandle();
        if (wasResident) makeResident();
    }
}

bool OpenGLTexture2D::makeResident()
{
    if (!GLCapabilities::hasBindlessTextures()) {
        GE_CORE_WARN("OpenGLTexture2D::makeResident - Bindless textures not supported");
        return false;
    }
    
    if (m_textureHandle == 0) {
        generateTextureHandle();
    }
    
    if (m_textureHandle == 0) {
        GE_CORE_ERROR("OpenGLTexture2D::makeResident - Failed to generate texture handle");
        return false;
    }
    
    if (!m_isResident) {
        // Use regular GL command instead of ARB extension directly
        glMakeTextureHandleResidentARB(m_textureHandle);
        m_isResident = true;
    }
    
    return true;
}

void OpenGLTexture2D::makeNonResident()
{
    if (!GLCapabilities::hasBindlessTextures() || m_textureHandle == 0 || !m_isResident) {
        return;
    }
    
    // Use regular GL command instead of ARB extension directly
    glMakeTextureHandleNonResidentARB(m_textureHandle);
    m_isResident = false;
}

void OpenGLTexture2D::generateTextureHandle()
{
    if (!GLCapabilities::hasBindlessTextures()) {
        GE_CORE_WARN("OpenGLTexture2D::generateTextureHandle - Bindless textures not supported");
        return;
    }
    
    // Make sure the texture is bound to generate a handle
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
    
    // Use regular GL command instead of ARB extension directly
    m_textureHandle = glGetTextureHandleARB(m_rendererID);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    if (m_textureHandle == 0) {
        GE_CORE_ERROR("OpenGLTexture2D::generateTextureHandle - Failed to generate texture handle");
    }
}



OpenGLTexture2D::OpenGLTexture2D(const std::vector<std::string>& filepaths)
{
    RAPTURE_PROFILE_FUNCTION();

    glGenTextures(1, &m_rendererID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_rendererID);

    stbi_set_flip_vertically_on_load(0);


    int width, height, nrChannels;
    unsigned char *data;  
    for(unsigned int i = 0; i < filepaths.size(); i++)
    {
        data = stbi_load(filepaths[i].c_str(), &width, &height, &nrChannels, 0);
        
        GLenum internalFormat = 0, dataFormat = 0;
        if (nrChannels == 4) {
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
        }
        else if (nrChannels == 3) {
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
        }
        
        m_internalFormat = internalFormat;
        m_dataFormat = dataFormat;
        
        if (internalFormat == 0 || dataFormat == 0) {
            GE_CORE_ERROR("OpenGLTexture2D: Unsupported format! Channels: {0}", nrChannels);
            stbi_image_free(data);
            return;
        }

        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
            0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data
        );
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); 

    m_width = width;
    m_height = height;
    m_isCubemap = true;

    // Generate handle for bindless textures if supported
    if (GLCapabilities::hasBindlessTextures()) {
        generateTextureHandle();
    }
    
    stbi_image_free(data);
 
}


// Static methods for bindless texture functionality
uint64_t OpenGLTexture2D::generateTextureHandleFromID(uint32_t textureID)
{
    if (!GLCapabilities::hasBindlessTextures()) {
        GE_CORE_WARN("OpenGLTexture2D::generateTextureHandleFromID - Bindless textures not supported");
        return 0;
    }

    if (textureID == 0) {
        GE_CORE_ERROR("OpenGLTexture2D::generateTextureHandleFromID - Invalid texture ID");
        return 0;
    }
    
    // Make sure the texture is bound to generate a handle
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Get the texture handle
    uint64_t handle = glGetTextureHandleARB(textureID);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    if (handle == 0) {
        GE_CORE_ERROR("OpenGLTexture2D::generateTextureHandleFromID - Failed to generate texture handle for ID {0}", textureID);
    }
    
    return handle;
}

bool OpenGLTexture2D::makeTextureResident(uint64_t textureHandle)
{
    if (!GLCapabilities::hasBindlessTextures()) {
        GE_CORE_WARN("OpenGLTexture2D::makeTextureResident - Bindless textures not supported");
        return false;
    }
    
    if (textureHandle == 0) {
        GE_CORE_ERROR("OpenGLTexture2D::makeTextureResident - Invalid texture handle");
        return false;
    }
    
    // Make the texture resident
    glMakeTextureHandleResidentARB(textureHandle);
    
    return true;
}

void OpenGLTexture2D::makeTextureNonResident(uint64_t textureHandle)
{
    if (!GLCapabilities::hasBindlessTextures() || textureHandle == 0) {
        return;
    }
    
    // Make the texture non-resident
    glMakeTextureHandleNonResidentARB(textureHandle);
}


// Static factory methods
std::shared_ptr<OpenGLTexture2D> OpenGLTexture2D::createFromPath(const std::string& path)
{
    return std::make_shared<OpenGLTexture2D>(path);
}

std::shared_ptr<OpenGLTexture2D> OpenGLTexture2D::createBlank(uint32_t width, uint32_t height, uint32_t channels)
{
    return std::make_shared<OpenGLTexture2D>(width, height, channels);
}

std::shared_ptr<OpenGLTexture2D> OpenGLTexture2D::createCubemap(const std::vector<std::string>& filepaths)
{
    return std::make_shared<OpenGLTexture2D>(filepaths);
}






std::shared_ptr<Texture2D> Texture2D::create(const std::string& path)
{
    return OpenGLTexture2D::createFromPath(path);
}

std::shared_ptr<Texture2D> Texture2D::create(uint32_t width, uint32_t height, uint32_t channels)
{
    return OpenGLTexture2D::createBlank(width, height, channels);
}

std::shared_ptr<Texture2D> Texture2D::createCubemap(const std::vector<std::string> &filepaths)
{
    return OpenGLTexture2D::createCubemap(filepaths);
}

uint64_t Texture2D::generateTextureHandleFromID(uint32_t textureID)
{
    return OpenGLTexture2D::generateTextureHandleFromID(textureID);
}

bool Texture2D::makeTextureResident(uint64_t textureHandle)
{
    return OpenGLTexture2D::makeTextureResident(textureHandle);
}

void Texture2D::makeTextureNonResident(uint64_t textureHandle)
{
    OpenGLTexture2D::makeTextureNonResident(textureHandle);
}

std::shared_ptr<Texture2D> Texture2D::create(TextureSpecification specification)
{
    return std::make_shared<OpenGLTexture2D>(specification);
}

} // namespace Rapture