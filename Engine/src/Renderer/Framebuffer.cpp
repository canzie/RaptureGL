#include "Framebuffer.h"

#include "../logger/Log.h"

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
			useHighPrecision ? FramebufferTextureFormat::RGB32F : FramebufferTextureFormat::RGB16F
		);
		
		// Normal buffer (RGB16F)
		gBufferSpec.attachments.push_back(FramebufferTextureFormat::RGB16F);
		
		// Albedo + Specular buffer (RGBA8)
		gBufferSpec.attachments.push_back(FramebufferTextureFormat::RGBA8);
		
		// Material properties buffer (RGBA8 or RGBA16F)
		gBufferSpec.attachments.push_back(
			useHighPrecision ? FramebufferTextureFormat::RGBA16F : FramebufferTextureFormat::RGBA8
		);
		
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
		
		// If no attachments specified, add default color attachment
		if (spec.attachments.empty())
		{
			m_specification.attachments.push_back(FramebufferTextureFormat::RGBA8);
		}
		
		invalidate();
	}

	Framebuffer::~Framebuffer()
	{
		if (m_framebufferID)
		{
			glDeleteFramebuffers(1, &m_framebufferID);
			
			glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
			
			if (m_depthAttachmentID)
				glDeleteTextures(1, &m_depthAttachmentID);
		}
	}

	void Framebuffer::invalidate()
	{
		// Cleanup existing framebuffer if it exists
		if (m_framebufferID)
		{
			glDeleteFramebuffers(1, &m_framebufferID);
			
			glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
			m_colorAttachments.clear();
			
			if (m_depthAttachmentID)
				glDeleteTextures(1, &m_depthAttachmentID);
			
			m_depthAttachmentID = 0;
		}

		// Create framebuffer
		glCreateFramebuffers(1, &m_framebufferID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);

		// Create color attachments
		bool multisample = m_specification.samples > 1;
		
		if (m_specification.attachments.size())
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
		}

			// Add default depth attachment
        FramebufferTextureFormat depthFormat = FramebufferTextureFormat::DEPTH24STENCIL8;

		for (auto& attachment : m_specification.attachments)
		{
			if (IsDepthFormat(attachment.textureFormat))
			{
				depthFormat = attachment.textureFormat;
				break;
			}
		}
		

			
			// Create depth texture
			glCreateTextures(multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &m_depthAttachmentID);
			glBindTexture(multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_depthAttachmentID);
			
			if (multisample)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_specification.samples, 
					TextureFormatToGL(depthFormat), m_specification.width, m_specification.height, GL_FALSE);
			}
			else
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, TextureFormatToGL(depthFormat), 
					m_specification.width, m_specification.height);
			}
			
			// Attach depth texture to framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 
				multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_depthAttachmentID, 0);
		
		
		// Verify framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			GE_CORE_ERROR("Framebuffer is incomplete!");
		}
		else
		{
			GE_CORE_INFO("Framebuffer successfully created ({0}x{1}, {2} samples)", 
				m_specification.width, m_specification.height, m_specification.samples);
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
		
		GE_CORE_INFO("Framebuffer resized to ({0}, {1})", width, height);
	}

	void Framebuffer::bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
		glViewport(0, 0, m_specification.width, m_specification.height);
		
		// Enable depth testing and ensure proper depth buffer behavior
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE); // Ensure depth writing is enabled
		
		// Clear both color and depth buffers to ensure a clean start
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
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
	
	uint32_t Framebuffer::getColorAttachmentRendererID(uint32_t index) const
	{
		if (index >= m_colorAttachments.size())
		{
			GE_CORE_ERROR("Color attachment index out of range: {0}", index);
			return 0;
		}
		
		return m_colorAttachments[index];
	}
}