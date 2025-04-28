#pragma once

#include "../../AssetsManager/AssetManager.h"
#include "../../Buffers/OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h"

#include <memory>
#include <vector>
#include <filesystem>
#include <string>

namespace Rapture {


    // Information about each mesh needed by the GPU shader
    // Assumes vertex/index data is already in large global GPU buffers
    struct GpuMeshMetadata {
        alignas(4) uint32_t vertexOffset;          // Starting vertex index in the global vertex buffer
        alignas(4) uint32_t indexOffset;           // Starting index in the global index buffer (in terms of index count, not bytes)
        alignas(4) uint32_t triangleCount;         // Number of triangles in this mesh

        // buffer data index

        alignas(4) uint32_t positionAttributeOffsetBytes; // Offset of position within the stride
        alignas(4) uint32_t vertexStride;                  // Stride of the vertex buffer
        alignas(4) uint32_t indexType;                   // Type of index buffer (16-bit or 32-bit)
        
        alignas(4) uint32_t meshIndex;             // The ID of this mesh (0, 1, 2...)
    };

    struct Element {
        uint32_t primitiveIdx;// the id of the primitive; this primitive id is copied to the leaf nodes of the  LBVHNode
        float aabbMinX;// aabb of the primitive
        float aabbMinY;
        float aabbMinZ;
        float aabbMaxX;
        float aabbMaxY;
        float aabbMaxZ;
    };


    // Output structure containing Morton code and mesh identifier
    // This buffer will be the input for the actual radix sort later.
    struct GpuOutputMortonElement {
        alignas(4) uint32_t mortonCode;            // 64-bit Morton code (using 30 bits based on paper)
        alignas(4) uint32_t originalTriangleIndex; // Triangle index *within its mesh* (0 to triangleCount-1)
        alignas(4) uint32_t meshIndex;             // Index of the mesh this triangle belongs to

    };

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

            std::shared_ptr<ShaderStorageBuffer> getSortedIndicesBuffer() const { return m_SortedIndicesBuffer; }
            std::shared_ptr<ShaderStorageBuffer> getPrimitiveAABBsBuffer() const { return m_PrimitiveAABBsBuffer; }

        private:
            void updateMortonCodes(const MeshBufferData& meshBufferData);
            void logBufferOutput(const std::shared_ptr<ShaderStorageBuffer>& buffer, uint32_t numElements);

            std::vector<GpuOutputMortonElement> cpuSort(const std::shared_ptr<ShaderStorageBuffer>& buffer, uint32_t numElements);

        private:
            uint32_t m_maxTriangleCount;

            std::shared_ptr<Shader> m_Shader;
            std::shared_ptr<Shader> m_MortonShader;
            //std::shared_ptr<Shader> m_PrefixSumShader;
            std::shared_ptr<Shader> m_HistogramShader;

            std::filesystem::path m_baseShaderPath = "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL/Sorting";
            std::string m_RadixShaderPath = "RadixSort/RadixSort.cs.glsl";
            std::string m_RadixMultiShaderPath = "RadixSort/RadixSortMulti.cs.glsl";

            std::string m_MortonShaderPath = "TrianglesToMorton.cs.glsl";
            //std::string m_PrefixSumShaderPath = "PrefixSum.cs.glsl";
            std::string m_HistogramShaderPath = "RadixSort/RadixSortHistogramMulti.cs.glsl";

            std::shared_ptr<ShaderStorageBuffer> m_SortedIndicesBuffer;

            std::shared_ptr<ShaderStorageBuffer> m_TriangleInfoBuffer;
            std::shared_ptr<ShaderStorageBuffer> m_MortonCodesBuffersA;
            std::shared_ptr<ShaderStorageBuffer> m_MortonCodesBuffersB;
            std::shared_ptr<ShaderStorageBuffer> m_GlobalHistogramBuffer;
            //std::shared_ptr<ShaderStorageBuffer> m_GlobalOffsetsBuffer;

            std::shared_ptr<ShaderStorageBuffer> m_PrimitiveAABBsBuffer;


            uint32_t NUM_BLOCKS_PER_WORKGROUP = 32;
            uint32_t globalInvocationSize;

            uint32_t WORKGROUP_SIZE = 256;
            uint32_t NUMBER_OF_WORKGROUPS;
            uint32_t RADIX_SORT_BINS = 256;
    };




}