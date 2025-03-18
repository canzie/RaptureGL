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
		}

		GE_CORE_ERROR("Unknown framebuffer data format!");
		return 0;
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
					
					// Force alpha to be fully opaque (1.0) to prevent transparency issues
					glTexImage2D(GL_TEXTURE_2D, 0, glFormat, m_specification.width, m_specification.height, 
						0, dataFormat, GL_UNSIGNED_BYTE, nullptr);
					
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