#pragma once

#include "RadianceCascades.h" // Includes BuildParams, RadianceCascadeHierarchy etc.
#include "../../Textures/Texture.h" // For Texture3D definition
#include "../../Buffers/OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h" // For SSBO definition
#include "../../Shaders/Shader.h" // For Shader definition
#include "../../AssetsManager/AssetManager.h" // For loading shader
#include "../../Logger/Log.h"

#include <vector>
#include <memory>
#include <string>

namespace Rapture {

class RadianceCascadesManager {
public:
    static void init(BuildParams params);
    static void shutdown();

    // Updates the cascade grid parameters (origin, spacing, transforms) based on the current camera view.
    // Should be called each frame before the compute shader dispatch.
    static void updateGpuBuffers();

    static std::shared_ptr<ShaderStorageBuffer> getSSBO();
    static std::shared_ptr<Shader> getComputeShader();
    static std::shared_ptr<RadianceCascadeHierarchy> getHierarchy();

private:
    static void calculateCascadeTransforms(std::vector<RadianceCascadeShaderData>& shaderData);

    static std::shared_ptr<RadianceCascadeHierarchy> m_hierarchy;
    static std::shared_ptr<Shader> m_computeShader;
    static std::shared_ptr<ShaderStorageBuffer> m_cascadeInfoSSBO;

    static bool m_initialized;

    // Store the path for shader loading
    static std::string _testShaderName; // Placeholder shader
    static std::string _populateShaderName;
    
};

} // namespace Rapture
