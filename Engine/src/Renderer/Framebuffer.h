#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace Rapture
{
	enum class FramebufferTextureFormat
	{
		None = 0,
		
		// Color formats
		RGBA8,
		RGB8,
		RED_INTEGER,
		
		// Depth/stencil formats
		DEPTH24STENCIL8,
		DEPTH32F,
		
		// Defaults
		Depth = DEPTH24STENCIL8
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: textureFormat(format) {}
			
		FramebufferTextureFormat textureFormat = FramebufferTextureFormat::None;
		// TODO: Add filtering/wrap options if needed
	};


	struct FramebufferSpecification
	{
        FramebufferSpecification() = default;
		FramebufferSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: attachments(attachments) {}

		uint32_t width = 1280;
		uint32_t height = 720;
		uint32_t samples = 1;  // Multisampling: 1 = no multisampling
		std::vector<FramebufferTextureSpecification> attachments;
		bool swapChainTarget = false;  // Whether this framebuffer is the main screen target
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer();

		void invalidate();
		
		void bind();
		void unbind();
		
		void resize(uint32_t width, uint32_t height);
		
		uint32_t getColorAttachmentRendererID(uint32_t index = 0) const;
		uint32_t getDepthAttachmentRendererID() const { return m_depthAttachmentID; }
		
		// For backward compatibility
		uint32_t getColorAID() const { return getColorAttachmentRendererID(); }
		void setResolution(unsigned int width, unsigned int height) { resize(width, height); }
		void unBind() { unbind(); }

		const FramebufferSpecification& getSpecification() const { return m_specification; }

		static std::shared_ptr<Framebuffer> create(const FramebufferSpecification& spec);
		
	private:
		FramebufferSpecification m_specification;
		
		uint32_t m_framebufferID = 0;
		std::vector<uint32_t> m_colorAttachments;
		uint32_t m_depthAttachmentID = 0;
	};
}