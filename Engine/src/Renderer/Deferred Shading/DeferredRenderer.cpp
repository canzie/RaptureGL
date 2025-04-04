#include "DeferredRenderer.h"

#include "../../Debug/TracyProfiler.h"
#include "../../Logger/Log.h"
#include "../Raycast.h"


namespace Rapture
{

    void DeferredRenderer::init()
    {
		RAPTURE_PROFILE_FUNCTION();
		GE_RENDER_INFO("Renderer: Initializing renderer");

		Raycast::init();
		
		// Initialize the CommandQueueBuilder with a thread pool
		CommandQueueBuilder::init(2); // Create 2 worker threads by default
		

        // Initialize G-buffer with current window size
        uint32_t width = 1280;
        uint32_t height = 720;
        
        s_gBuffer = std::make_shared<GBuffer>(width, height, true);
        
        // Create framebuffer for lighting pass
        FramebufferSpecification lightingBufferSpec;
        lightingBufferSpec.width = width;
        lightingBufferSpec.height = height;
        lightingBufferSpec.attachments = { FramebufferTextureFormat::RGBA16F };
        s_lightingBuffer = Framebuffer::create(lightingBufferSpec);
        
        // Load deferred shaders
        // ...
        
        GE_CORE_INFO("Deferred rendering initialized");



    }

    void DeferredRenderer::shutdown()
    {
		Raycast::shutdown();

		// Shutdown worker threads first to prevent accessing released resources
		CommandQueueBuilder::shutdownWorkers();

    }

    void DeferredRenderer::onFrameBegin()
    {

    }

    void DeferredRenderer::renderQueueAsync(std::shared_ptr<RenderQueue> queue)
    {
        RAPTURE_PROFILE_GPU_SCOPE("Executing Async RenderQueue");
        RAPTURE_PROFILE_SCOPE("Executing Async RenderQueue");
        
        // Process commands until the queue is both empty and marked as done
        while (!queue->isDone()) {
            // Try to get a command, process it if available
            CommandVariant cmd;
            if (queue->tryServe(cmd)) {
                if (std::holds_alternative<GeometryPassCommand>(cmd)) {
                    geometryPassRender(std::get<GeometryPassCommand>(cmd));
                }
                else if (std::holds_alternative<LightingPassCommand>(cmd)) {
                    lightingPassRender(std::get<LightingPassCommand>(cmd));
                }
            }
        }
    }

    void DeferredRenderer::geometryPass(const std::shared_ptr<Scene> s)
    {
        // Bind G-buffer
        s_gBuffer->bind();


        auto geometryQueue = CommandQueueBuilder::buildGeometryCommandQueueAsync(s);

        renderQueueAsync(geometryQueue);

        // Unbind G-buffer
        s_gBuffer->unbind();
    }

void DeferredRenderer::lightingPass(const std::shared_ptr<Scene> s)
{
    // Bind lighting buffer
    s_lightingBuffer->bind();

    // Clear lighting buffer
    
    s_lightingBuffer->unbind();

}

void DeferredRenderer::geometryPassRender(const GeometryPassCommand &cmd)
{
    RAPTURE_PROFILE_GPU_SCOPE("Executing geometryPassRender");
    RAPTURE_PROFILE_SCOPE("Executing geometryPassRender");
    
    // Process commands until the queue is both empty and marked as done
    
}

void DeferredRenderer::lightingPassRender(const LightingPassCommand &cmd)
{
}
}
