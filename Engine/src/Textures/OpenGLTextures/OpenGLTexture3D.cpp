#include "OpenGLTexture3D.h"

#include "../../Logger/Log.h"
#include "../../Utils/GLCapabilities.h"
#include "../../Debug/TracyProfiler.h"
#include "OpenGLFormatConverters.h"


namespace Rapture {

    OpenGLTexture3D::OpenGLTexture3D(TextureSpecification specification)
    {
        RAPTURE_PROFILE_FUNCTION();

        if (specification.width == 0 || specification.height == 0 || specification.depth == 0) {
            GE_CORE_ERROR("OpenGLTexture3D::OpenGLTexture3D - Invalid texture specification");
            return;
        }

        m_specification = specification;

        m_internalFormat = TextureFormatToGL(specification.format);
        m_dataFormat = TextureFormatToGLDataFormat(specification.format);
        m_dataType = TextureFormatToGLDataType(specification.format);

        glGenTextures(1, &m_rendererID);
        glBindTexture(GL_TEXTURE_3D, m_rendererID);
        
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

        glTexImage3D(GL_TEXTURE_3D, 0, m_internalFormat, 
                m_specification.width, 
                m_specification.height, 
                m_specification.depth, 
                0, m_dataFormat, m_dataType, nullptr);

        //glGenerateMipmap(GL_TEXTURE_3D);


        glBindTexture(GL_TEXTURE_3D, 0);

        GE_CORE_INFO("OpenGLTexture3D::OpenGLTexture3D - Created blank texture ({0}x{1}x{2})", m_specification.width, m_specification.height, m_specification.depth);

    }

    OpenGLTexture3D::~OpenGLTexture3D()
    {
        // Make the texture non-resident if it is resident
        if (m_isResident) {
            makeNonResident();
        }
        
        glDeleteTextures(1, &m_rendererID);
    }

    void OpenGLTexture3D::bind(uint32_t slot) const
    {
        RAPTURE_PROFILE_GPU_SCOPE("OpenGLTexture3D::bind");
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_3D, m_rendererID);
    }

    void OpenGLTexture3D::unbind() const
    {
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    void OpenGLTexture3D::bindCompute(uint32_t slot) const
    {
        glBindImageTexture(slot, m_rendererID, 0, GL_FALSE, 0, GL_READ_WRITE, m_internalFormat);

    }

    void OpenGLTexture3D::barrier() const
    {
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void OpenGLTexture3D::clear(glm::vec4 color)
    {
        glBindTexture(GL_TEXTURE_3D, m_rendererID);
        glClearTexImage(m_rendererID, 0, m_dataFormat, GL_FLOAT, &color[0]);
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    bool OpenGLTexture3D::makeResident()
    {
        if (!GLCapabilities::hasBindlessTextures()) {
            GE_CORE_WARN("OpenGLTexture3D::makeResident - Bindless textures not supported");
            return false;
        }
        
        if (m_textureHandle == 0) {
            generateTextureHandle();
        }
        
        if (m_textureHandle == 0) {
            GE_CORE_ERROR("OpenGLTexture3D::makeResident - Failed to generate texture handle");
            return false;
        }
        
        if (!m_isResident) {
            // Use regular GL command instead of ARB extension directly
            glMakeTextureHandleResidentARB(m_textureHandle);
            m_isResident = true;
        }
        
        return true;
    }

    void OpenGLTexture3D::makeNonResident()
    {
        if (!GLCapabilities::hasBindlessTextures() || m_textureHandle == 0 || !m_isResident) {
            return;
        }
        
        // Use regular GL command instead of ARB extension directly
        glMakeTextureHandleNonResidentARB(m_textureHandle);
        m_isResident = false;
    }


    void OpenGLTexture3D::generateTextureHandle()
    {
        if (!GLCapabilities::hasBindlessTextures()) {
            GE_CORE_WARN("OpenGLTexture3D::generateTextureHandle - Bindless textures not supported");
            return;
        }
        
        // Make sure the texture is bound to generate a handle
        glBindTexture(GL_TEXTURE_3D, m_rendererID);
        
        // Use regular GL command instead of ARB extension directly
        m_textureHandle = glGetTextureHandleARB(m_rendererID);
        
        glBindTexture(GL_TEXTURE_3D, 0);
        
        if (m_textureHandle == 0) {
            GE_CORE_ERROR("OpenGLTexture3D::generateTextureHandle - Failed to generate texture handle");
        }
    }


    void OpenGLTexture3D::setWrap(TextureWrap wrap, GLenum pname)
    {

        if (GLCapabilities::hasDSA()) {
            glTextureParameteri(m_rendererID, pname, convertWrapToGL(wrap));
        } else {
            glBindTexture(GL_TEXTURE_3D, m_rendererID);
            glTexParameteri(GL_TEXTURE_3D, pname, convertWrapToGL(wrap));
            glBindTexture(GL_TEXTURE_3D, 0);
        }
        
        // Re-generate handle if using bindless
        if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
            bool wasResident = m_isResident;
            if (wasResident) makeNonResident();
            generateTextureHandle();
            if (wasResident) makeResident();
        }
    }

    void OpenGLTexture3D::setMinFilter(TextureFilter filter)
    {
        if (GLCapabilities::hasDSA()) {
            glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, convertFilterToGL(filter));
        } else {
            glBindTexture(GL_TEXTURE_3D, m_rendererID);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, convertFilterToGL(filter));
            glBindTexture(GL_TEXTURE_3D, 0);
        }
        
        // Re-generate handle if using bindless
        if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
            bool wasResident = m_isResident;
            if (wasResident) makeNonResident();
            generateTextureHandle();
            if (wasResident) makeResident();
        }
    }

    void OpenGLTexture3D::setMagFilter(TextureFilter filter)
    {
        
        // Note: Mag filter can only be GL_NEAREST or GL_LINEAR
        GLenum glFilter = convertFilterToGL(filter);
        if (glFilter != GL_NEAREST && glFilter != GL_LINEAR) {
            GE_CORE_WARN("OpenGLTexture3D: Mag filter can only be Nearest or Linear. Using Linear instead.");
            glFilter = GL_LINEAR;
        }
        
        if (GLCapabilities::hasDSA()) {
            glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, glFilter);
        } else {
            glBindTexture(GL_TEXTURE_3D, m_rendererID);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, glFilter);
            glBindTexture(GL_TEXTURE_3D, 0);
        }
        
        // Re-generate handle if using bindless
        if (GLCapabilities::hasBindlessTextures() && m_textureHandle != 0) {
            bool wasResident = m_isResident;
            if (wasResident) makeNonResident();
            generateTextureHandle();
            if (wasResident) makeResident();
        }
    }

    void OpenGLTexture3D::setWrapS(TextureWrap wrap)
    {
        setWrap(wrap, GL_TEXTURE_WRAP_S);
    }

    void OpenGLTexture3D::setWrapT(TextureWrap wrap)
    {
        setWrap(wrap, GL_TEXTURE_WRAP_T);
    }

    void OpenGLTexture3D::setWrapR(TextureWrap wrap)
    {
        setWrap(wrap, GL_TEXTURE_WRAP_R);

    }

    std::shared_ptr<Texture3D> Texture3D::create(uint32_t width, uint32_t height, uint32_t depth, uint32_t channels)
    {
        if (channels != 3 && channels != 4) {
            GE_CORE_ERROR("OpenGLTexture3D::create - Invalid number of channels, expected 3 or 4, got {0}. For a specific format, use the TextureSpecification.", channels);
            return nullptr;
        }

        TextureSpecification spec;  
        spec.width = width;
        spec.height = height;
        spec.depth = depth;
        spec.format = channels == 3 ? TextureFormat::RGB8 : TextureFormat::RGBA8;
        return std::make_shared<OpenGLTexture3D>(spec);
    }

    std::shared_ptr<Texture3D> Texture3D::create(TextureSpecification specification)
    {
        return std::make_shared<OpenGLTexture3D>(specification);
    }
}
