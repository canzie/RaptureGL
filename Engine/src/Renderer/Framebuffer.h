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
		
		// High precision formats for G-buffer
		RGB16F,
		RGB32F,
		RGBA16F,
		
		// Depth/stencil formats
		DEPTH24STENCIL8,
		DEPTH32F,
		
		// Defaults
		Depth = DEPTH24STENCIL8
	};

	// G-buffer attachment types
	enum class GBufferAttachmentType
	{
		POSTITION,    // RGB32F or RGB16F for world space positions
		NORMAL,      // RGB16F for normals
		ALBEDO,      // RGBA8 for albedo (RGB) and specular (A)
		MATERIAL,    // RGBA8 or RGBA16F for material properties (roughness, metallic, etc.)
		DEPTH        // DEPTH24STENCIL8
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
		
		// G-buffer creation helper
		static std::shared_ptr<Framebuffer> createGBuffer(uint32_t width, uint32_t height, bool useHighPrecision = true);
		
	private:
		FramebufferSpecification m_specification;
		
		uint32_t m_framebufferID = 0;
		std::vector<uint32_t> m_colorAttachments;
		uint32_t m_depthAttachmentID = 0;
	};
}