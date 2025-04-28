#include "RadixSort.h"

#include "../../AssetsManager/AssetManager.h"
#include "../../Shaders/Shader.h"

#include "../../Debug/TracyProfiler.h"

namespace Rapture {
    RadixSort::RadixSort(uint32_t maxTriangleCount)
    : m_maxTriangleCount(maxTriangleCount)
    {
        auto [shader, handle] = AssetManager::importAsset<Shader>(m_baseShaderPath / m_RadixShaderPath);
        auto [mortonShader, mortonHandle] = AssetManager::importAsset<Shader>(m_baseShaderPath / m_MortonShaderPath);

        m_Shader = shader;
        m_MortonShader = mortonShader;

        

        //m_SortedIndicesBuffer = std::make_shared<ShaderStorageBuffer>();
        m_TriangleInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(GpuMeshMetadata), BufferUsage::Dynamic);

        m_MortonCodesBuffersA = std::make_shared<ShaderStorageBuffer>(sizeof(GpuOutputMortonElement) * maxTriangleCount, BufferUsage::Dynamic);
        m_MortonCodesBuffersB = std::make_shared<ShaderStorageBuffer>(sizeof(GpuOutputMortonElement) * maxTriangleCount, BufferUsage::Dynamic);
        
        m_PrimitiveAABBsBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(Element) * maxTriangleCount, BufferUsage::Dynamic);
    }

    RadixSort::~RadixSort()
    {
        m_Shader.reset();
        m_MortonShader.reset();
        m_TriangleInfoBuffer.reset();
        m_MortonCodesBuffersA.reset();
        m_MortonCodesBuffersB.reset();

    }

void RadixSort::sort(const MeshBufferData &meshBufferData) {
    RAPTURE_PROFILE_FUNCTION();


    uint32_t numElements = meshBufferData.triangleCount;

    if (numElements == 0 || !m_Shader || !m_MortonShader) return;
    if (numElements > m_maxTriangleCount) {
        GE_CORE_ERROR("RadixSort::sort called with numElements ({}) > maxElementCount ({})", numElements, m_maxTriangleCount);
        return;
    }


    updateMortonCodes(meshBufferData);

    const uint32_t WORKGROUP_SIZE = 256;
    const uint32_t NUM_PASSES = 16;

    uint32_t numWorkGroups = (numElements + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;


    std::shared_ptr<ShaderStorageBuffer> currentInputBuffer = m_MortonCodesBuffersA;
    std::shared_ptr<ShaderStorageBuffer> currentOutputBuffer = m_MortonCodesBuffersB;

    // sort dispatch
    m_Shader->bind();
    m_Shader->setUint("u_numElements", numElements);

    currentInputBuffer->bindBase(0);
    currentOutputBuffer->bindBase(1);

    m_Shader->dispatchCompute(256, 1, 1);
    ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});

    m_Shader->unBind();


    m_SortedIndicesBuffer = currentOutputBuffer;


    //logBufferOutput(m_SortedIndicesBuffer, numElements);
}


void RadixSort::updateMortonCodes(const MeshBufferData &meshBufferData)
{

    GpuMeshMetadata meshMetadata;
    meshMetadata.vertexOffset = meshBufferData.vertexAllocation->offsetBytes;
    meshMetadata.indexOffset = meshBufferData.indexAllocation->offsetBytes;
    meshMetadata.triangleCount = meshBufferData.triangleCount;
    meshMetadata.positionAttributeOffsetBytes = meshBufferData.vao->getBufferLayout().getAttribute(AttributeType::POSITION).offset;
    meshMetadata.meshIndex = 0; // only 1 mesh, so index 0
    meshMetadata.indexType = meshBufferData.indexType;
    meshMetadata.vertexStride = meshBufferData.vao->getBufferLayout().vertexSize;

    uint32_t numWorkGroups = (meshMetadata.triangleCount + 256 - 1) / 256;


    m_MortonShader->bind();    
    
    meshBufferData.vao->getVertexBuffer()->bindBase(0);
    meshBufferData.vao->getIndexBuffer()->bindBase(1);

    m_TriangleInfoBuffer->setData(&meshMetadata, sizeof(GpuMeshMetadata));
    m_TriangleInfoBuffer->bindBase(2);

    m_MortonCodesBuffersA->bindBase(3);
    m_PrimitiveAABBsBuffer->bindBase(4);

    m_MortonShader->setVec3("u_sceneAABBMin", meshBufferData.AABBMin);
    m_MortonShader->setVec3("u_sceneAABBMax", meshBufferData.AABBMax);

    m_MortonShader->dispatchCompute(256, 1, 1);
    ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});
    m_MortonShader->unBind();

}

void RadixSort::logBufferOutput(const std::shared_ptr<ShaderStorageBuffer> &buffer, uint32_t numElements)
{

    ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});

    size_t bufferSize = numElements * sizeof(GpuOutputMortonElement);
    std::vector<GpuOutputMortonElement> sortedData(numElements); // Allocate CPU memory

    // Map the GPU buffer for reading
    void* mappedPtr = buffer->map(0, bufferSize);

    if (mappedPtr) {
        // Copy data from GPU to CPU
        memcpy(sortedData.data(), mappedPtr, bufferSize);
        // Unmap the buffer once done
        
        buffer->unmap();
        

        // Log the first few elements for verification
        GE_CORE_INFO("--- Data (First up to 10 elements) ---");
        size_t count = std::min(static_cast<size_t>(10), static_cast<size_t>(numElements));
        for (size_t i = 0; i < count; ++i) {
            GE_CORE_INFO("  Element {}: Index = {}, Morton Code = {}", i, sortedData[i].originalTriangleIndex, sortedData[i].mortonCode);
        }
        if (numElements > count) {
             GE_CORE_INFO("  ... ({} total elements)", numElements);
        }
        GE_CORE_INFO("---------------------------------------------");

    } else {
        GE_CORE_ERROR("RadixSort::sort - Failed to map sorted indices buffer for reading.");
    }

}

// Test the sort
bool RadixSort::testSort()
{
    return false;
}

// Test the prefix sum
bool RadixSort::testPrefixSum()
{
    return false;
}

// Test the morton code conversion
bool RadixSort::testMortonCodeConversion()
{

    GpuMeshMetadata meshMetadata;
    meshMetadata.vertexOffset = 0;
    meshMetadata.indexOffset = 0;
    meshMetadata.triangleCount = 1;
    meshMetadata.positionAttributeOffsetBytes = 0;
    meshMetadata.meshIndex = 0;
    meshMetadata.indexType = 5125;
    meshMetadata.vertexStride = sizeof(glm::vec3);




    return false;
}
}