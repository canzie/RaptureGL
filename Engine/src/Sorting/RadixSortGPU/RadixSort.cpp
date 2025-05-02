#include "RadixSort.h"

#include "../../AssetsManager/AssetManager.h"
#include "../../Shaders/Shader.h"

#include "../../Debug/TracyProfiler.h"

#include <algorithm>

#include "../../WindowContext/Application.h"

namespace Rapture {




    RadixSort::RadixSort(uint32_t maxTriangleCount)
    : m_maxTriangleCount(maxTriangleCount)
    {
        auto& app = Application::getInstance();
        auto project = app.getProject();
        if (!project) {
            GE_RENDER_ERROR("RadixSort::RadixSort - Project not found, unable to start RadixSort");
            return;
        }
        auto shaderPath = project->getConfig().shaderPath;

        auto [shader, handle] = AssetManager::importAsset<Shader>(shaderPath / m_RadixMultiShaderPath);
        auto [mortonShader, mortonHandle] = AssetManager::importAsset<Shader>(shaderPath / m_MortonShaderPath);
        auto [histogramShader, histogramHandle] = AssetManager::importAsset<Shader>(shaderPath / m_HistogramShaderPath);

        m_Shader = shader;
        m_MortonShader = mortonShader;
        m_HistogramShader = histogramShader;

        NUM_BLOCKS_PER_WORKGROUP = 32;
        globalInvocationSize = 100000 / NUM_BLOCKS_PER_WORKGROUP;

        uint32_t remainder = 100000 % NUM_BLOCKS_PER_WORKGROUP;
        globalInvocationSize += remainder > 0 ? 1 : 0;

        WORKGROUP_SIZE = 256;
        NUMBER_OF_WORKGROUPS = (globalInvocationSize + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        RADIX_SORT_BINS = 256;


        //m_SortedIndicesBuffer = std::make_shared<ShaderStorageBuffer>();
        m_TriangleInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(GpuMeshMetadata), BufferUsage::Dynamic);

        m_MortonCodesBuffersA = std::make_shared<ShaderStorageBuffer>(sizeof(GpuOutputMortonElement) * maxTriangleCount, BufferUsage::Dynamic);
        m_MortonCodesBuffersB = std::make_shared<ShaderStorageBuffer>(sizeof(GpuOutputMortonElement) * maxTriangleCount, BufferUsage::Dynamic);
        
        m_PrimitiveAABBsBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(Element) * maxTriangleCount, BufferUsage::Dynamic);
        m_GlobalHistogramBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(uint32_t) * RADIX_SORT_BINS * NUMBER_OF_WORKGROUPS, BufferUsage::Dynamic);

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
    auto sortedMortonCodes = cpuSort(m_MortonCodesBuffersA, numElements);
    const uint32_t NUM_PASSES = 4;
    m_MortonCodesBuffersA->setData(sortedMortonCodes.data(), sortedMortonCodes.size() * sizeof(GpuOutputMortonElement));

    m_SortedIndicesBuffer = m_MortonCodesBuffersA;

    //uint32_t numWorkGroups = (numElements + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

    /*
    std::shared_ptr<ShaderStorageBuffer> currentInputBuffer = m_MortonCodesBuffersA;
    std::shared_ptr<ShaderStorageBuffer> currentOutputBuffer = m_MortonCodesBuffersB;

    uint32_t shift = 0;

    for (uint32_t pass = 0; pass < NUM_PASSES; pass++) {
        shift = pass * 8;

        m_HistogramShader->bind();

        m_HistogramShader->setUint("g_num_elements", numElements);
        m_HistogramShader->setUint("g_shift", shift);
        m_HistogramShader->setUint("g_num_workgroups", NUMBER_OF_WORKGROUPS);
        m_HistogramShader->setUint("g_num_blocks_per_workgroup", NUM_BLOCKS_PER_WORKGROUP);
        
        currentInputBuffer->bindBase(0);
        m_GlobalHistogramBuffer->bindBase(1);

        m_HistogramShader->dispatchCompute(globalInvocationSize, 1, 1);
        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});
        m_HistogramShader->unBind();

        // sort dispatch
        m_Shader->bind();
        m_Shader->setUint("g_num_elements", numElements);
        m_Shader->setUint("g_shift", shift);
        m_Shader->setUint("g_num_workgroups", NUMBER_OF_WORKGROUPS);
        m_Shader->setUint("g_num_blocks_per_workgroup", NUM_BLOCKS_PER_WORKGROUP);

        currentInputBuffer->bindBase(0);
        currentOutputBuffer->bindBase(1);
        m_GlobalHistogramBuffer->bindBase(2);

        m_Shader->dispatchCompute(globalInvocationSize, 1, 1);
        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});

        m_Shader->unBind();

    }




    m_SortedIndicesBuffer = currentInputBuffer;
*/

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

std::vector<GpuOutputMortonElement> RadixSort::cpuSort(const std::shared_ptr<ShaderStorageBuffer> &buffer, uint32_t numElements)
{
    ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});

    size_t bufferSize = numElements * sizeof(GpuOutputMortonElement);
    std::vector<GpuOutputMortonElement> mortonCodes(numElements); // Allocate CPU memory

    // Map the GPU buffer for reading
    void* mappedPtr = buffer->map(0, bufferSize);

    if (mappedPtr) {
        // Copy data from GPU to CPU
        memcpy(mortonCodes.data(), mappedPtr, bufferSize);
        // Unmap the buffer once done
        
        buffer->unmap();
        

        std::sort(mortonCodes.begin(), mortonCodes.end(), [](const GpuOutputMortonElement &a, const GpuOutputMortonElement &b) {
            return a.mortonCode < b.mortonCode;
        });

    } else {
        GE_CORE_ERROR("RadixSort::sort - Failed to map sorted indices buffer for reading.");
    }

    return mortonCodes;
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