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

#include <cstdint>


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
};

struct ProbeInfo {
    alignas(16) glm::uvec3 probeGridDimensions; // Number of probes in each dimension (X, Y, Z)
    alignas(8) glm::uvec2 probeResolution; // Resolution of each probe texture (e.g., 8x8)
    alignas(16) glm::vec3 probeSpacing;
    alignas(16) glm::vec3 probeOrigin; // will probably be camera position
};

struct DirectionalLightBufferInfo {
    alignas(16) glm::vec3 direction;
    alignas(4) float intensity;
};

struct DebugData {
    uint32_t leafHits;
    uint32_t triangleHits;
    float closestHit;
    uint32_t closestHitMeshIndex;
};

class DynamicDiffuseGI {
public:
    DynamicDiffuseGI();
    ~DynamicDiffuseGI();

    void populateProbes(std::shared_ptr<Scene> scene);
    void populateProbesCompute();

    std::shared_ptr<Texture2D> getRadianceTexture() { 
        if (m_isEvenFrame) {
            return m_RadianceTexture;
        } else {
            return m_PrevRadianceTexture;
        }
    }
    std::shared_ptr<Texture2D> getVisibilityTexture() { 
        if (m_isEvenFrame) {
            return m_VisibilityTexture;
        } else {
            return m_PrevVisibilityTexture;
        }
    }

    std::vector<glm::vec3>& getDebugProbePositions() { return m_DebugProbePositions; }

private:
    int createBufferMetadata(std::shared_ptr<VertexArray> vao);
    int getBufferMetadataIndex(uint32_t vaoID);
    void readDebugBuffer();

private:
    std::shared_ptr<Shader> m_DDGI_PopulateProbesShader;

    // (vaoID, BufferMetadata)
    // we need to retain the order of the buffermetadata, when using a map we can lose this order and the buffermetadata indices might not match the ssbo
    std::vector<std::pair<uint32_t, BufferMetadata>> m_BufferMetadataMap;
    
    std::shared_ptr<ShaderStorageBuffer> m_MeshInfoBuffer;
    std::shared_ptr<ShaderStorageBuffer> m_BufferMetadataBuffer;
    std::shared_ptr<UniformBuffer> m_ProbeInfoBuffer;

    std::shared_ptr<ShaderStorageBuffer> m_DebugBuffer;

    // is actually irradiance but iam retarted, will need to update this everywhere :(
    std::shared_ptr<Texture2D> m_RadianceTexture;
    std::shared_ptr<Texture2D> m_VisibilityTexture;

    std::shared_ptr<Texture2D> m_PrevRadianceTexture;
    std::shared_ptr<Texture2D> m_PrevVisibilityTexture;

    std::vector<glm::vec3> m_DebugProbePositions;

    std::shared_ptr<UniformBuffer> m_SunLightBuffer;

    // used to alternate between the textures each frame
    bool m_isEvenFrame;

    float m_Hysteresis;

    uint32_t m_meshCount;
    uint32_t m_probesPerRow; // Number of probes along the X-axis of the atlas texture

    
};

}

