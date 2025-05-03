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

    uint32_t positionAttributeOffsetBytes; // Offset of position *within* the stride
    uint32_t texCoordAttributeOffsetBytes;
    uint32_t vertexStrideBytes;            // Stride of the vertex buffer in bytes
    uint32_t indexType;                    // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)

    uint64_t VBOHandle;
    uint64_t IBOHandle;
};

struct MeshInfo {
    uint32_t RootIndex; // index of the root node in the BVH
    uint64_t AlbedoTextureHandle;
    uint64_t NormalTextureHandle;
    uint64_t MetallicRoughnessTextureHandle;
    uint32_t bufferMetadataIDX; // index for BufferMetadata array
    uint32_t triangleOffset; // offset of the first triangle

    // offset of the mesh's vertex and index data
    uint32_t vertexOffsetBytes;
    uint32_t indexOffsetBytes;

    glm::mat4 Transform;
};

struct ProbeInfo {
    glm::uvec3 probeGridDimensions; // Number of probes in each dimension (X, Y, Z)
    glm::uvec2 probeResolution; // Resolution of each probe texture (e.g., 8x8)
    glm::vec3 probeSpacing;
    glm::vec3 probeOrigin; // will probably be camera position
};

class DynamicDiffuseGI {
public:
    DynamicDiffuseGI();
    ~DynamicDiffuseGI();

    void populateProbes(std::shared_ptr<Scene> scene);

private:
    int createBufferMetadata(std::shared_ptr<VertexArray> vao);
    int getBufferMetadataIndex(uint32_t vaoID);

private:
    std::shared_ptr<Shader> m_DDGI_PopulateProbesShader;

    // (vaoID, BufferMetadata)
    // we need to retain the order of the buffermetadata, when using a map we can lose this order and the buffermetadata indices might not match the ssbo
    std::vector<std::pair<uint32_t, BufferMetadata>> m_BufferMetadataMap;
    
    std::shared_ptr<ShaderStorageBuffer> m_MeshInfoBuffer;
    std::shared_ptr<ShaderStorageBuffer> m_BufferMetadataBuffer;
    std::shared_ptr<UniformBuffer> m_ProbeInfoBuffer;

    std::shared_ptr<Texture2D> m_RadianceTexture;
    std::shared_ptr<Texture2D> m_VisibilityTexture;


    
};

}

