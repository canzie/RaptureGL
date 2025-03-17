#include "Framebuffer.h"

#include "../logger/Log.h"

#include <glad/glad.h>



namespace Rapture
{

	Framebuffer::Framebuffer()
	{
		Invalidate();
	}

	Framebuffer::~Framebuffer()
	{
		glDeleteFramebuffers(1, &m_framebufferID);
	}

	void Framebuffer::Invalidate()
	{
		glCreateFramebuffers(1, &m_framebufferID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_colorAttachmentID);
		glBindTexture(GL_TEXTURE_2D, m_colorAttachmentID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			m_colorAttachmentID, 0);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_depthAttachmentID);
		glBindTexture(GL_TEXTURE_2D, m_depthAttachmentID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_width, m_height, 0,
			GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
			GL_TEXTURE_2D, m_depthAttachmentID, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
		{
			GE_CORE_INFO("---Framebuffer Successfully Created---");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}

	void Framebuffer::setResolution(unsigned int width, unsigned int height)
	{
		m_width = width;
		m_height = height;
	}

	void Framebuffer::bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
	}

	void Framebuffer::unBind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

}