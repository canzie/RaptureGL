#pragma once

#include "../../Scenes/Scene.h"
#include <memory>
#include "GBuffer.h"
#include "../Framebuffer.h"
#include "../RenderQueue.h"
#include "../../AssetsManager/AssetManager.h"
#include "../PrimitiveShapes.h"



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

        static void geometryPassRender(const RenderCommand& cmd);
        static inline void lightingPassRender(const LightingPassCommand& cmd);

        static std::shared_ptr<GBuffer> getGBuffer() { return s_gBuffer; }
        static std::shared_ptr<Framebuffer> getLightingBuffer() { return s_lightingBuffer; }

    private:
        // Helper method for light setup
        static void setupLightsUniforms(const std::shared_ptr<Scene> s);
        // Helper methods for lighting pass
        static void setupFullscreenQuad();
        static void renderFullscreenQuad();

    private:
        // deferred shading
        static std::shared_ptr<GBuffer> s_gBuffer;
        static std::shared_ptr<Framebuffer> s_lightingBuffer;
        static std::shared_ptr<UniformBuffer> s_cameraUBO;
        static std::shared_ptr<UniformBuffer> s_lightsUBO;

        static std::weak_ptr<Shader> s_lightingPassShader;
        static AssetHandle s_lightingPassShaderHandle;

        // fullscreen quad for lighting pass
        static std::shared_ptr<Quad> s_fullscreenQuad;

        // Persistent pointer for lights UBO mapping
        static void* s_persistentLightsBufferPtr;

        // Caching for lights data
        static bool s_lightsDirty;
        static std::vector<entt::entity> s_cachedLightEntities;
        static uint32_t s_cachedLightCount;

    };
}
