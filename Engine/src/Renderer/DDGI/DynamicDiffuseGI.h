#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "../../Shaders/Shader.h"
#include "../../Buffers/OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h"
#include "../../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "../../Scenes/Scene.h"
#include "../../Scenes/Entity.h"
#include "../../Buffers/VertexArray.h"
#include "../../Textures/Texture.h"
#include "../../Sorting/SpatialSorting/BVH/LBVH/LBVH.h"

#include "../ShadowMapping/CascadedShadowMapping.h"

#include <cstdint>

#include "ProbeVolumeGPU.h"

namespace Rapture {

struct BufferMetadata {
    alignas(4) uint32_t positionAttributeOffsetBytes; // Offset of position *within* the stride
    alignas(4) uint32_t texCoordAttributeOffsetBytes;
    alignas(4) uint32_t normalAttributeOffsetBytes;
    alignas(4) uint32_t tangentAttributeOffsetBytes;

    alignas(4) uint32_t vertexStrideBytes;            // Stride of the vertex buffer in bytes
    alignas(4) uint32_t indexType;                    // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)

    alignas(8) uint64_t VBOHandle;
    alignas(8) uint64_t IBOHandle;
};

struct MeshInfo {
    alignas(4) uint32_t RootIndex; // index of the root node in the BVH
    alignas(8) uint64_t AlbedoTextureHandle;
    alignas(8) uint64_t NormalTextureHandle;
    alignas(8) uint64_t MetallicRoughnessTextureHandle;
    alignas(4) uint32_t bufferMetadataIDX; // index for BufferMetadata array

    // offset of the mesh's vertex and index data
    alignas(4) uint32_t vertexOffsetBytes;
    alignas(4) uint32_t indexOffsetBytes;

    alignas(16) glm::mat4 Transform;
    alignas(16) glm::mat4 InvTransform;
};



struct SunProperties {
    alignas(16) glm::mat4 sunLightSpaceMatrix;    // Light-space matrices for each cascade
    alignas(16) glm::vec3 sunDirectionWorld;                       // Normalized direction FROM fragment TO sun
    alignas(16) glm::vec3 sunColor;

    alignas(4) float sunIntensity;
    alignas(8) uint64_t sunShadowTextureArrayHandle; // Bindless handle for sampler2DArrayShadow
};



struct DebugData { // Must match GLSL Profile struct
    alignas(8) uint64_t TOTAL_INVOCATION_TIME;
    alignas(8) uint64_t TRACE_TLAS_TIME;
    alignas(8) uint64_t TOTAL_BVH_TIME_PER_TLAS_CALL;
    alignas(8) uint64_t SUM_TLAS_INTERNAL_RBBOX_TIME_PER_TLAS_CALL;
    alignas(8) uint64_t AVG_TLAS_INTERNAL_RBBOX_CALL_TIME;
    alignas(8) uint64_t SUM_RAY_SETUP_TRANSFORM_TIME_PER_TLAS_CALL;
    alignas(8) uint64_t SUM_HIT_TRANSFORM_TIME_PER_TLAS_CALL;

    alignas(8) uint64_t BVH_TRACE_TIME; // Avg time for one full traceBVH call
    alignas(8) uint64_t BVH_TRAVERSAL_OVERHEAD_TIME;

    // Average time per individual call of these functions (workgroup-wide)
    alignas(8) uint64_t AVG_TIME_PER_GTVERTS_CALL;
    alignas(8) uint64_t AVG_TIME_PER_ITRI_CALL;
    alignas(8) uint64_t AVG_TIME_PER_RBBOX_CALL;

    // New: Average sum of time spent in these functions *during one average traceBVH call*
    alignas(8) uint64_t AVG_SUM_GTVERTS_TIME_IN_BVHCALL;
    alignas(8) uint64_t AVG_SUM_ITRI_TIME_IN_BVHCALL;
    alignas(8) uint64_t AVG_SUM_RBBOX_TIME_IN_BVHCALL;

    alignas(8) uint64_t GET_TRIANGLE_EXTRAS_TIME; 
    alignas(8) uint64_t DIRECT_DIFFUSE_LIGHTING_TIME;
    alignas(8) uint64_t GET_VOLUME_IRRADIANCE_TIME;
};

class DynamicDiffuseGI {
public:
    DynamicDiffuseGI();
    ~DynamicDiffuseGI();

    void populateProbes(std::shared_ptr<Scene> scene);
    void populateProbesCompute(std::shared_ptr<Scene> scene);

    std::shared_ptr<Texture2D> getRadianceTexture() { return m_RadianceTexture; } 
    std::shared_ptr<Texture2D> getVisibilityTexture() { return m_VisibilityTexture; }
    std::shared_ptr<Texture2D> getRadianceTextureFlattened() { return m_IrradianceTextureFlattened; } 
    std::shared_ptr<Texture2D> getVisibilityTextureFlattened() { return m_DistanceTextureFlattened; } 

    std::vector<glm::vec3>& getDebugProbePositions() { return m_DebugProbePositions; }

    std::shared_ptr<UniformBuffer> getProbeInfoBuffer() { return m_ProbeInfoBuffer; }
    uint32_t getProbesPerRow() { return m_probesPerRow; }

private:
    void castRays(std::shared_ptr<Scene> scene);
    void blendTextures();

    int createBufferMetadata(std::shared_ptr<VertexArray> vao);
    int getBufferMetadataIndex(uint32_t vaoID);
    void readDebugBuffer();
    void initTextures();
    void updateSunProperties(std::shared_ptr<Scene> scene);
    void initProbeInfoBuffer();

private:
    std::shared_ptr<Shader> m_DDGI_ProbeTraceShader;
    std::shared_ptr<Shader> m_DDGI_ProbeIrradianceBlendingShader;
    std::shared_ptr<Shader> m_DDGI_ProbeDistanceBlendingShader;
    std::shared_ptr<Shader> m_Flatten2dArrayShader;

    ProbeVolume m_ProbeVolume;
    
    SunProperties m_SunShadowProps;

    // (vaoID, BufferMetadata)
    // we need to retain the order of the buffermetadata, when using a map we can lose this order and the buffermetadata indices might not match the ssbo
    std::vector<std::pair<uint32_t, BufferMetadata>> m_BufferMetadataMap;
    
    std::shared_ptr<ShaderStorageBuffer> m_MeshInfoBuffer;
    std::shared_ptr<ShaderStorageBuffer> m_BufferMetadataBuffer;

    std::shared_ptr<ShaderStorageBuffer> m_DebugBuffer;

    std::shared_ptr<UniformBuffer> m_SunLightBuffer;
    std::shared_ptr<UniformBuffer> m_ProbeInfoBuffer;


    // is actually irradiance but iam retarted, will need to update this everywhere :(
    std::shared_ptr<Texture2D> m_RadianceTexture;
    std::shared_ptr<Texture2D> m_VisibilityTexture;

    std::shared_ptr<Texture2D> m_PrevRadianceTexture;
    std::shared_ptr<Texture2D> m_PrevVisibilityTexture;

    std::shared_ptr<Texture2D> m_RayDataTexture;

    std::shared_ptr<Texture2D> m_IrradianceTextureFlattened;
    std::shared_ptr<Texture2D> m_DistanceTextureFlattened;


    std::vector<glm::vec3> m_DebugProbePositions;


    // used to alternate between the textures each frame
    bool m_isEvenFrame;

    bool m_isPopulated;

    float m_Hysteresis;

    uint32_t m_meshCount;
    uint32_t m_probesPerRow; // Number of probes along the X-axis of the atlas texture

    
};

}

