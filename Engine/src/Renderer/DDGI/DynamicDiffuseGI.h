#pragma once

#include <cstdint>


namespace Rapture {


struct BufferMetadata {
    uint32_t vertexOffsetBytes; // Starting *byte* offset in the global vertex buffer
    uint32_t texCoordOffsetBytes;
    uint32_t indexOffsetBytes;  // Starting *byte* offset in the global index buffer
    uint32_t triangleCount;     // Number of triangles in this mesh

    uint32_t positionAttributeOffsetBytes; // Offset of position *within* the stride
    uint32_t texCoordAttributeOffsetBytes;
    uint32_t vertexStrideBytes;            // Stride of the vertex buffer in bytes
    uint32_t indexType;                    // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)

    uint64_t VBOHandle;
    uint64_t IBOHandle;
};


class DynamicDiffuseGI {
public:
    DynamicDiffuseGI();
    ~DynamicDiffuseGI();
};

}

