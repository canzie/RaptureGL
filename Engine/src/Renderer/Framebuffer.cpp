#include "Framebuffer.h"

#include "../logger/Log.h"
#include "../Textures/Texture.h"

#include <glad/glad.h>

namespace Rapture
{
	static const uint32_t s_MaxFramebufferSize = 8192;
	
	static GLenum TextureFormatToGL(FramebufferTextureFormat format)
	{
		switch (format)
		{
			case FramebufferTextureFormat::RGBA8:       return GL_RGBA8;
			case FramebufferTextureFormat::RGB8:        return GL_RGB8;
			case FramebufferTextureFormat::RED_INTEGER: return GL_R32I;
			case FramebufferTextureFormat::RGB16F:      return GL_RGB16F;
			case FramebufferTextureFormat::RGB32F:      return GL_RGB32F;
			case FramebufferTextureFormat::RGBA16F:     return GL_RGBA16F;
			case FramebufferTextureFormat::RGBA32F:     return GL_RGBA32F;
			case FramebufferTextureFormat::DEPTH24STENCIL8: return GL_DEPTH24_STENCIL8;
			case FramebufferTextureFormat::DEPTH32F:    return GL_DEPTH_COMPONENT32F;
		}

		GE_CORE_ERROR("Unknown framebuffer texture format!");
		return 0;
	}
	
	static GLenum TextureFormatToGLDataFormat(FramebufferTextureFormat format)
	{
		switch (format)
		{
			case FramebufferTextureFormat::RGBA8:       return GL_RGBA;
			case FramebufferTextureFormat::RGB8:        return GL_RGB;
			case FramebufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
			case FramebufferTextureFormat::RGB16F:      return GL_RGB;
			case FramebufferTextureFormat::RGB32F:      return GL_RGB;
			case FramebufferTextureFormat::RGBA16F:     return GL_RGBA;
			case FramebufferTextureFormat::RGBA32F:     return GL_RGBA;
		}

		GE_CORE_ERROR("Unknown framebuffer data format!");
		return 0;
	}
	
	static GLenum TextureFormatToGLDataType(FramebufferTextureFormat format)
	{
		switch (format)
		{
			case FramebufferTextureFormat::RGBA8:       return GL_UNSIGNED_BYTE;
			case FramebufferTextureFormat::RGB8:        return GL_UNSIGNED_BYTE;
			case FramebufferTextureFormat::RED_INTEGER: return GL_INT;
			case FramebufferTextureFormat::RGB16F:      return GL_FLOAT;
			case FramebufferTextureFormat::RGB32F:      return GL_FLOAT;
			case FramebufferTextureFormat::RGBA16F:     return GL_FLOAT;
			case FramebufferTextureFormat::RGBA32F:     return GL_FLOAT;
		}

		GE_CORE_ERROR("Unknown framebuffer data type!");
		return GL_UNSIGNED_BYTE;
	}
	
	static bool IsDepthFormat(FramebufferTextureFormat format)
	{
		switch (format)
		{
			case FramebufferTextureFormat::DEPTH24STENCIL8:
			case FramebufferTextureFormat::DEPTH32F:
				return true;
		}
		
		return false;
	}

	// Helper function to create a G-buffer with multiple render targets
	std::shared_ptr<Framebuffer> Framebuffer::createGBuffer(uint32_t width, uint32_t height, bool useHighPrecision)
	{
		FramebufferSpecification gBufferSpec;
		gBufferSpec.width = width;
		gBufferSpec.height = height;
		gBufferSpec.samples = 1; // G-buffer typically doesn't use MSAA (would require resolve)
		
		// Position buffer (RGB32F or RGB16F)
		gBufferSpec.attachments.push_back(
			useHighPrecision ? FramebufferTextureFormat::RGBA32F : FramebufferTextureFormat::RGB16F
		);
		
		// Normal buffer (RGB16F)
		gBufferSpec.attachments.push_back(FramebufferTextureFormat::RGB16F);
		
		// Albedo + Specular buffer (RGBA8)
		gBufferSpec.attachments.push_back(FramebufferTextureFormat::RGBA8);
		
		// Material properties buffer (RGBA8 or RGBA16F)
		gBufferSpec.attachments.push_back(
			useHighPrecision ? FramebufferTextureFormat::RGBA16F : FramebufferTextureFormat::RGBA8
		);
        
        gBufferSpec.attachments.push_back(FramebufferTextureFormat::Depth);
		
		// Depth buffer is added automatically in the Framebuffer::invalidate() method
		
		GE_CORE_INFO("Creating G-buffer ({0}x{1}) with {2} precision", 
			width, height, useHighPrecision ? "high" : "standard");
			
		return create(gBufferSpec);
	}

	std::shared_ptr<Framebuffer> Framebuffer::create(const FramebufferSpecification& spec)
	{
		return std::make_shared<Framebuffer>(spec);
	}

	Framebuffer::Framebuffer(const FramebufferSpecification& spec)
		: m_specification(spec)
	{
		// Validate spec
		if (spec.width == 0 || spec.height == 0 || spec.width > s_MaxFramebufferSize || spec.height > s_MaxFramebufferSize)
		{
			GE_CORE_ERROR("Invalid framebuffer size: ({0}, {1})", spec.width, spec.height);
			return;
		}
		
        bool isDepthBufferOnly = false;
        if (spec.attachments.size() == 1){
            if (IsDepthFormat(spec.attachments[0].textureFormat))
            {
                isDepthBufferOnly = true;
            }
        }

        invalidate(isDepthBufferOnly);
        makeAllTexturesResident();

		
	}

	Framebuffer::~Framebuffer()
	{
        m_colorAttachments.clear();
        m_colorAttachmentsHandlesMap.clear();
        
		if (m_framebufferID)
		{
            makeAllTexturesNonResident();

			glDeleteFramebuffers(1, &m_framebufferID);
			
			glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
			
			if (m_depthAttachment)
				glDeleteTextures(1, &m_depthAttachment);
		}
	}

	void Framebuffer::invalidate(bool isDepthBufferOnly)
	{
		// Cleanup existing framebuffer if it exists
		if (m_framebufferID)
		{
			glDeleteFramebuffers(1, &m_framebufferID);
			
			glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
			m_colorAttachments.clear();
			
			if (m_depthAttachment)
				glDeleteTextures(1, &m_depthAttachment);
			
			m_depthAttachment = 0;
            
            if (m_depthTextureArray)
                glDeleteTextures(1, &m_depthTextureArray);
            
            m_depthTextureArray = 0;
		}

		// Create framebuffer
		glCreateFramebuffers(1, &m_framebufferID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);

		// Create color attachments
		bool multisample = m_specification.samples > 1;
        
		// Check if we need to create a depth texture array
		bool createDepthTextureArray = false;
		uint32_t arrayLayers = 1;
		for (auto& attachment : m_specification.attachments)
		{
			if (IsDepthFormat(attachment.textureFormat) && attachment.isTextureArray)
			{
				createDepthTextureArray = true;
				arrayLayers = attachment.arrayLayers;
				break;
			}
		}
		
		if (createDepthTextureArray)
		{
            // Find the depth attachment specification
            FramebufferTextureSpecification depthSpec;
            for (auto& attachment : m_specification.attachments)
            {
                if (IsDepthFormat(attachment.textureFormat) && attachment.isTextureArray)
                {
                    depthSpec = attachment;
                    break;
                }
            }
            
            // Create depth texture array
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_depthTextureArray);
            glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthTextureArray);
            
            // Allocate storage for the texture array
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 
                           1, // mipmap levels
                           TextureFormatToGL(depthSpec.textureFormat),
                           m_specification.width, m_specification.height,
                           arrayLayers);
            
            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            if (depthSpec.isShadowMap)
            {
                // Set proper wrapping for shadow map
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                
                // Set border color to white (1.0) - fragments beyond shadow map will be considered lit
                float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
                glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
                
                // Enable hardware PCF with linear filtering
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);	
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            
            // Attach depth texture array to framebuffer
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthTextureArray, 0);
            
            // Generate bindless handle if needed
            if (depthSpec.isBindless)
            {
                m_depthTextureArrayHandle = Texture2D::generateTextureHandleFromID(m_depthTextureArray);
            }
            
            GE_CORE_INFO("Created depth texture array with {0} layers, size {1}x{2}", 
                  arrayLayers, m_specification.width, m_specification.height);
            
            // For depth-only framebuffer, disable color attachments
            glNamedFramebufferDrawBuffer(m_framebufferID, GL_NONE);
            glNamedFramebufferReadBuffer(m_framebufferID, GL_NONE);
		}
		else if (m_specification.attachments.size())
		{
			m_colorAttachments.resize(m_specification.attachments.size());
			
			// Create textures
			for (size_t i = 0; i < m_colorAttachments.size(); i++)
			{
				auto& format = m_specification.attachments[i].textureFormat;
				
				if (IsDepthFormat(format))
					continue;
					
				glCreateTextures(multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &m_colorAttachments[i]);
				glBindTexture(multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_colorAttachments[i]);
				
				if (multisample)
				{
					glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_specification.samples, TextureFormatToGL(format), 
						m_specification.width, m_specification.height, GL_FALSE);
				}
				else
				{
					GLenum glFormat = TextureFormatToGL(format);
					GLenum dataFormat = TextureFormatToGLDataFormat(format);
					GLenum dataType = TextureFormatToGLDataType(format);
					
					// Force alpha to be fully opaque (1.0) to prevent transparency issues
					glTexImage2D(GL_TEXTURE_2D, 0, glFormat, m_specification.width, m_specification.height, 
						0, dataFormat, dataType, nullptr);
					
					// Set texture parameters
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    

				}
				
				// Attach texture to framebuffer
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, 
					multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_colorAttachments[i], 0);

                if (m_specification.attachments[i].isBindless)
                {
                    m_colorAttachmentsHandlesMap[m_colorAttachments[i]] = Texture2D::generateTextureHandleFromID(m_colorAttachments[i]);
                }
			}
			
			// Set up draw buffers for multiple render targets (MRT)
			if (m_colorAttachments.size() > 1)
			{
				GLenum drawBuffers[8] = { GL_NONE }; // Max 8 color attachments in OpenGL
				for (size_t i = 0; i < m_colorAttachments.size(); i++)
					drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
					
				glDrawBuffers(m_colorAttachments.size(), drawBuffers);
                GE_CORE_INFO("Set up {0} draw buffers for multiple render targets", m_colorAttachments.size());
			}
			
			// Add default depth attachment if we don't have a texture array
			if (!createDepthTextureArray)
			{
				FramebufferTextureSpecification depthAttachmentSpec = FramebufferTextureSpecification();

				for (auto& attachment : m_specification.attachments)
				{
					if (IsDepthFormat(attachment.textureFormat))
					{
						depthAttachmentSpec = attachment;
						break;
					}
				}
					
				// Create depth texture
				glCreateTextures(multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &m_depthAttachment);
				glBindTexture(multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_depthAttachment);
					
				if (multisample)
				{
					glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_specification.samples, 
						TextureFormatToGL(depthAttachmentSpec.textureFormat), m_specification.width, m_specification.height, GL_FALSE);
				}
				else
				{
					glTexStorage2D(GL_TEXTURE_2D, 1, TextureFormatToGL(depthAttachmentSpec.textureFormat), 
					m_specification.width, m_specification.height);

					// Set linear filtering for smoother shadows
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					if (depthAttachmentSpec.isShadowMap)
					{
						// Set proper wrapping for shadow map
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
						
						// Set border color to white (1.0) - fragments beyond shadow map will be considered lit
						float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
						glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
						
						// Enable hardware PCF with linear filtering
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
					} else {

						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);	
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					}
				}
					
				// Attach depth texture to framebuffer
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 
					multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_depthAttachment, 0);

				if (depthAttachmentSpec.isBindless)
				{
					m_depthAttachmentHandle = Texture2D::generateTextureHandleFromID(m_depthAttachment);
				}
			}
		}
        
        // Handle the case when it's a depth-only framebuffer with no texture array
        if (isDepthBufferOnly && !createDepthTextureArray)
        {
            glNamedFramebufferDrawBuffer(m_framebufferID, GL_NONE);
            glNamedFramebufferReadBuffer(m_framebufferID, GL_NONE);
        }

		// Verify framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			GE_CORE_ERROR("Framebuffer is incomplete!");

            // NOTE: dont know if this cleanup is legal, since if the framebuffer is incomplete, maybe the ids are not valid anymore and opengl might delete them itself,
            glDeleteFramebuffers(1, &m_framebufferID);
			
			glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
			
			if (m_depthAttachment)
				glDeleteTextures(1, &m_depthAttachment);
                
            if (m_depthTextureArray)
                glDeleteTextures(1, &m_depthTextureArray);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
		}
		else
		{
			GE_CORE_INFO("Framebuffer {0} successfully created ({1}x{2}, {3} samples)", 
				m_framebufferID, m_specification.width, m_specification.height, m_specification.samples);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}


	void Framebuffer::resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
		{
			GE_CORE_WARN("Attempted to resize framebuffer to invalid size: {0}, {1}", width, height);
			return;
		}
		
		m_specification.width = width;
		m_specification.height = height;
		
		invalidate();
		
		GE_CORE_INFO("Framebuffer::resize - Framebuffer resized to ({0}, {1})", width, height);
	}

	void Framebuffer::bind(bool clear)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
		glViewport(0, 0, m_specification.width, m_specification.height);
		
		// Enable depth testing and ensure proper depth buffer behavior
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE); // Ensure depth writing is enabled
		
        bool isDepthBufferOnly = false;
        if (m_specification.attachments.size() == 1){
            if (IsDepthFormat(m_specification.attachments[0].textureFormat))
            {
                isDepthBufferOnly = true;
            }
        }

		// Clear both color and depth buffers to ensure a clean start
		if(clear)
		{
			if (isDepthBufferOnly)
			{
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			else
			{
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			}
		}
		
		// Make sure blending is disabled for the framebuffer to prevent transparency issues
		glDisable(GL_BLEND);
	}

	void Framebuffer::unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		// Restore standard depth test setting when switching back to default framebuffer
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		
		// Restore previous blend state if needed
		// glEnable(GL_BLEND);
		// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

    void Framebuffer::bindTexturesCompute(uint32_t startSlot)
    {
        for (size_t i = 0; i < m_colorAttachments.size(); i++)
        {
            auto& format = m_specification.attachments[i].textureFormat;
            GLenum glFormat = TextureFormatToGL(format);
            glBindImageTexture(startSlot + i, m_colorAttachments[i], 0, GL_FALSE, 0, GL_READ_ONLY, glFormat);
        }
        
        
    }

    void Framebuffer::bindTextures(uint32_t startSlot)
    {        
        for (size_t i = 0; i < m_colorAttachments.size(); i++)
        {

            glActiveTexture(GL_TEXTURE0 + startSlot + i);
            glBindTexture(GL_TEXTURE_2D, m_colorAttachments[i]);
        }
        
    }

    void Framebuffer::disableDepthTesting()
    {
        // Disable depth testing and ensure proper depth buffer behavior
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE); // Ensure depth writing is disabled
    }

    uint32_t Framebuffer::getColorAttachmentRendererID(uint32_t index) const
	{
		if (index >= m_colorAttachments.size())
		{
			GE_CORE_ERROR("Color attachment index out of range: {0}", index);
			return 0;
		}
		
		return m_colorAttachments[index];
	}
    void Framebuffer::clearAttachments()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

	uint64_t Framebuffer::getColorAttachmentTextureHandle(uint32_t index) const
	{
		if (index >= m_colorAttachments.size())
		{
			GE_CORE_ERROR("Color attachment index out of range: {0}", index);
			return 0;
		}
		
		// If we have already generated the handle, return it
		uint32_t textureID = m_colorAttachments[index];
        uint64_t textureHandle = 0;
        if (textureID != 0)
		{
			textureHandle = m_colorAttachmentsHandlesMap.at(textureID);
		}
		
		
		return textureHandle;
	}

    // returns 64bit texture handle, if the spec specified it
    // otherwise returns 0, this will not create a new handle
	uint64_t Framebuffer::getDepthAttachmentTextureHandle() const
	{
		if (m_depthAttachment == 0)
		{
			GE_CORE_ERROR("No depth attachment available");
			return 0;
		}
		
		return m_depthAttachmentHandle;

	}

	bool Framebuffer::makeAllTexturesResident()
	{
		bool success = true;

		// Make color attachments resident
		for (auto [textureID, handle] : m_colorAttachmentsHandlesMap)
		{
			if (handle != 0)
			{
				success &= Texture2D::makeTextureResident(handle);
			}
		}
		
		// Make depth attachment resident if available
		if (m_depthAttachmentHandle != 0)
		{
			success &= Texture2D::makeTextureResident(m_depthAttachmentHandle);
		}
		
		// Make depth texture array resident if available
		if (m_depthTextureArrayHandle != 0)
		{
			success &= Texture2D::makeTextureResident(m_depthTextureArrayHandle);
		}
		
		return success;
	}

	void Framebuffer::makeAllTexturesNonResident()
	{
		// Make color attachments non-resident
		for (auto [textureID, handle] : m_colorAttachmentsHandlesMap)
		{
			if (handle != 0)
			{
				Texture2D::makeTextureNonResident(handle);
			}
		}
		
		// Make depth attachment non-resident if available
		if (m_depthAttachmentHandle != 0)
		{
			Texture2D::makeTextureNonResident(m_depthAttachmentHandle);
		}
		
		// Make depth texture array non-resident if available
		if (m_depthTextureArrayHandle != 0)
		{
			Texture2D::makeTextureNonResident(m_depthTextureArrayHandle);
		}
	}


}