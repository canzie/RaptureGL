#pragma once

#include "../../Scenes/Scene.h"
#include <memory>
#include "GBuffer.h"
#include "../Framebuffer.h"
#include "../RenderQueue.h"


namespace Rapture
{
    class DeferredRenderer
    {
    public:
     	// Initialize the renderer and its subsystems
		static void init();
		
		// Shutdown the renderer and its subsystems
		static void shutdown();

        static void onFrameBegin();

        static void onFrameEnd();

        static void sumbitScene(const std::shared_ptr<Scene> s);


        static void renderQueueAsync(std::shared_ptr<RenderQueue> queue);


        static void geometryPass(const std::shared_ptr<Scene> s);
        static void lightingPass(const std::shared_ptr<Scene> s);

        static void geometryPassRender(const GeometryPassCommand& cmd);
        static void lightingPassRender(const LightingPassCommand& cmd);


    private:

        // deferred shading
        static std::shared_ptr<GBuffer> s_gBuffer;
        static std::shared_ptr<Framebuffer> s_lightingBuffer;
	
    };
}
