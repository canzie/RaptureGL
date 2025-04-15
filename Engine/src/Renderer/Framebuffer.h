#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <map>

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
		RGBA32F,

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
        bool isBindless = false;
        bool isShadowMap = false;
        bool isTextureArray = false;
        uint32_t arrayLayers = 1;
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

		void invalidate(bool isDepthBufferOnly = false);
		
		void bind(bool clear = true);
		void unbind();

        void disableDepthTesting();

        bool isValid() const { return m_framebufferID != 0; }
		
		void resize(uint32_t width, uint32_t height);
		
		uint32_t getColorAttachmentRendererID(uint32_t index = 0) const;
		uint32_t getDepthAttachmentRendererID() const { return m_depthAttachment; }

        const uint32_t& getFramebufferID() const { return m_framebufferID; }

        void clearAttachments();
		
		// For backward compatibility
		uint32_t getColorAID() const { return getColorAttachmentRendererID(); }
		void setResolution(unsigned int width, unsigned int height) { resize(width, height); }
		void unBind() { unbind(); }

		const FramebufferSpecification& getSpecification() const { return m_specification; }

		static std::shared_ptr<Framebuffer> create(const FramebufferSpecification& spec);
		
		// G-buffer creation helper
		static std::shared_ptr<Framebuffer> createGBuffer(uint32_t width, uint32_t height, bool useHighPrecision = true);
		
		// Bindless texture support
		uint64_t getColorAttachmentTextureHandle(uint32_t index = 0) const;
		uint64_t getDepthAttachmentTextureHandle() const;
		
		// Texture array support
		bool hasDepthTextureArray() const { return m_depthTextureArray != 0; }
		uint32_t getDepthTextureArrayID() const { return m_depthTextureArray; }
		uint64_t getDepthTextureArrayHandle() const { return m_depthTextureArrayHandle; }
		
		bool makeAllTexturesResident();
		void makeAllTexturesNonResident();

	private:
		FramebufferSpecification m_specification;
		
		uint32_t m_framebufferID = 0;
		std::vector<uint32_t> m_colorAttachments;
        std::map<uint32_t, uint64_t> m_colorAttachmentsHandlesMap;
		uint32_t m_depthAttachment = 0;
        uint64_t m_depthAttachmentHandle = 0;
        
        // Texture array support
        uint32_t m_depthTextureArray = 0;
        uint64_t m_depthTextureArrayHandle = 0;
	};
}