#pragma once

#include "../AssetsManager/AssetManager.h"
#include "../Buffers/OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h"

#include <memory>
#include <vector>
#include <filesystem>
#include <string>

namespace Rapture {


    // stores shader used and creates/holds the buffers used for sorting
    // and contains a reference to the buffer with the sorted indices
    class RadixSort {
        public:
            RadixSort(uint32_t maxTriangleCount);
            ~RadixSort();

            void sort(const MeshBufferData& meshBufferData);
            bool testSort();
            bool testPrefixSum();
            bool testMortonCodeConversion();

        private:
            void updateMortonCodes(const MeshBufferData& meshBufferData);
            void logBufferOutput(const std::shared_ptr<ShaderStorageBuffer>& buffer, uint32_t numElements);

        private:
            uint32_t m_maxTriangleCount;

            std::shared_ptr<Shader> m_Shader;
            std::shared_ptr<Shader> m_MortonShader;
            std::shared_ptr<Shader> m_PrefixSumShader;
            std::shared_ptr<Shader> m_HistogramShader;

            std::filesystem::path m_baseShaderPath = "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL/Sorting";
            std::string m_RadixShaderPath = "RadixSort/RadixSort.cs.glsl";
            std::string m_MortonShaderPath = "TrianglesToMorton.cs.glsl";
            std::string m_PrefixSumShaderPath = "PrefixSum.cs.glsl";
            std::string m_HistogramShaderPath = "RadixSort/HistogramBuilder.cs.glsl";

            std::shared_ptr<ShaderStorageBuffer> m_SortedIndicesBuffer;

            std::shared_ptr<ShaderStorageBuffer> m_TriangleInfoBuffer;
            std::shared_ptr<ShaderStorageBuffer> m_MortonCodesBuffersA;
            std::shared_ptr<ShaderStorageBuffer> m_MortonCodesBuffersB;
            std::shared_ptr<ShaderStorageBuffer> m_GlobalHistogramBuffer;
            std::shared_ptr<ShaderStorageBuffer> m_GlobalOffsetsBuffer;
    };


// Information about each mesh needed by the GPU shader
// Assumes vertex/index data is already in large global GPU buffers
struct GpuMeshMetadata {
    uint32_t vertexOffset;          // Starting vertex index in the global vertex buffer
    uint32_t indexOffset;           // Starting index in the global index buffer (in terms of index count, not bytes)
    uint32_t triangleCount;         // Number of triangles in this mesh

    // buffer data index

    uint32_t positionAttributeOffsetBytes; // Offset of position within the stride
    uint32_t vertexStride;                  // Stride of the vertex buffer
    uint32_t indexType;                   // Type of index buffer (16-bit or 32-bit)
    uint32_t meshIndex;             // The ID of this mesh (0, 1, 2...)
};

// will store all of the buffers handles and metadata
struct BufferMetadata {
    uint64_t vertexBufferAddress;   // 64-bit GPU address of the VBO
    uint64_t indexBufferAddress;    // 64-bit GPU address of the IBO

    // indextype
    // vertexstride
    // positionattributeoffsetbytes
};

// Output structure containing Morton code and mesh identifier
// This buffer will be the input for the actual radix sort later.
struct GpuOutputMortonElement {
    uint64_t mortonCode;            // 64-bit Morton code (using 30 bits based on paper)
    uint32_t originalTriangleIndex; // Triangle index *within its mesh* (0 to triangleCount-1)
    uint32_t meshIndex;             // Index of the mesh this triangle belongs to

};

}