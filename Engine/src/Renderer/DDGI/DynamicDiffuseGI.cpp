#include "DynamicDiffuseGI.h"

#include "../../Scenes/Components/Components.h"
#include "../../Sorting/SpatialSorting/BVH/LBVH/LBVH.h"

#include "../../WindowContext/Application.h"

namespace Rapture {

    DynamicDiffuseGI::DynamicDiffuseGI()
      : m_ProbeInfoBuffer(nullptr),
        m_MeshInfoBuffer(nullptr),
        m_BufferMetadataBuffer(nullptr),
        m_RadianceTexture(nullptr),
        m_VisibilityTexture(nullptr),
        m_probesPerRow(0)
    {

        auto& app = Application::getInstance();
        auto project = app.getProject();
        auto shaderPath = project->getConfig().shaderPath;

        auto [shader, handle] = AssetManager::importAsset<Shader>(shaderPath / "DDGI/PopulateProbesDDGI2.cs.glsl");
        m_DDGI_PopulateProbesShader = shader;

    }

    DynamicDiffuseGI::~DynamicDiffuseGI()
    {
    }

    void DynamicDiffuseGI::populateProbes(std::shared_ptr<Scene> scene)
    {
        // get all the meshes in the scene
        auto& reg = scene->getRegistry();
        auto view = reg.view<MeshComponent, TransformComponent, MaterialComponent>();

        std::vector<MeshInfo> meshInfos;
        std::vector<BufferMetadata> bufferMetadata;

        // bind the lbvh buffer
        // get all of the bvh roots, currently just use a "bad" solution until we have a tlas
        auto meshBVHNodes = LBVHManager::getLBVH()->getAllCPUBVHNodes();

        // get the needed buffer metadata
        // just use the handles for comparisons and set/get the correct index based on this


        //probe info
        ProbeInfo probeInfo;
        probeInfo.probeGridDimensions = glm::uvec3(16, 16, 16);
        probeInfo.probeResolution = glm::uvec2(8, 8);
        probeInfo.probeSpacing = glm::vec3(2.0f, 2.0f, 2.0f);

        // probe positions, rotation does not matter since we place porbes around the camera, even behind
        probeInfo.probeOrigin = glm::vec3(0.0f, 0.0f, 0.0f);

        // Calculate the total size of the grid and the starting offset
        glm::vec3 totalGridSize = glm::vec3(probeInfo.probeGridDimensions) * probeInfo.probeSpacing;
        glm::vec3 startOffset = probeInfo.probeOrigin - (totalGridSize / 2.0f) + (probeInfo.probeSpacing / 2.0f); // Offset by half spacing to center probes

        
        for (int x = 0; x < probeInfo.probeGridDimensions.x; x++) {
            for (int y = 0; y < probeInfo.probeGridDimensions.y; y++) {
                for (int z = 0; z < probeInfo.probeGridDimensions.z; z++) {
                    // Calculate the position based on the starting offset and index
                    auto probePosition = startOffset + glm::vec3(x, y, z) * probeInfo.probeSpacing;
                    m_DebugProbePositions.push_back(probePosition);
                    //GE_CORE_TRACE("DynamicDiffuseGI::populateProbes - Probe Index: ({0}, {1}, {2}), Position: ({3}, {4}, {5})", x, y, z, probePosition.x, probePosition.y, probePosition.z);
                }
            }
        }
        

        

        // get the mesh info data for the buffer
        for (auto entity : view)
        {
            auto [mesh, transform, material] = view.get<MeshComponent, TransformComponent, MaterialComponent>(entity);

            if (mesh.mesh)
            {

                auto meshData = mesh.mesh->getMeshData();

                uint32_t vertexOffsetBytes = meshData.vertexAllocation->offsetBytes;
                uint32_t indexOffsetBytes = meshData.indexAllocation->offsetBytes;

                EntityID entityID = Entity::enttHandleToEntityID(entity);

                uint32_t rootIndex = meshBVHNodes[entityID].absoluteRootIndex;

                uint32_t vaoID = meshData.vao->getID();

                // kind of scuffed, i could return the index needed from the create function
                // but that seems scuffed aswell
                int bufferMetadataIDX = getBufferMetadataIndex(vaoID);
                if (bufferMetadataIDX == -1) {
                    bufferMetadataIDX = createBufferMetadata(meshData.vao);
                }

                auto materialHandle = material.material;
                auto albedoTextureParameter = materialHandle->getParameter(ParameterID::TEXTURE_ALBEDO_BINDLESS);
            
                auto normalTextureParameter = materialHandle->getParameter(ParameterID::TEXTURE_NORMAL_BINDLESS);
                auto roughnessTextureParameter = materialHandle->getParameter(ParameterID::TEXTURE_ROUGHNESS_BINDLESS);

                uint64_t albedoTextureHandle = 0;
                uint64_t normalTextureHandle = 0;
                uint64_t roughnessTextureHandle = 0;

                if (albedoTextureParameter.getType() != MaterialParameterType::NONE) {
                    albedoTextureHandle = albedoTextureParameter.asTextureBindless();
                }

                if (normalTextureParameter.getType() != MaterialParameterType::NONE) {
                    normalTextureHandle = normalTextureParameter.asTextureBindless();
                }

                if (roughnessTextureParameter.getType() != MaterialParameterType::NONE) {
                    roughnessTextureHandle = roughnessTextureParameter.asTextureBindless();
                }

                if (albedoTextureHandle == 0 || normalTextureHandle == 0 || roughnessTextureHandle == 0) {
                    GE_CORE_WARN("DynamicDiffuseGI::populateProbes - Mesh {0} has no albedo, normal, or roughness texture", entityID);   
                }
                GE_CORE_TRACE("DynamicDiffuseGI::populateProbes - Mesh {0} has albedo texture handle: {1}, normal texture handle: {2}, roughness texture handle: {3}", entityID, albedoTextureHandle, normalTextureHandle, roughnessTextureHandle);


                MeshInfo meshInfo;
                meshInfo.RootIndex = rootIndex;
                meshInfo.AlbedoTextureHandle = albedoTextureHandle;
                meshInfo.NormalTextureHandle = normalTextureHandle;
                meshInfo.MetallicRoughnessTextureHandle = roughnessTextureHandle;
                meshInfo.Transform = transform.transformMatrix();
                meshInfo.bufferMetadataIDX = bufferMetadataIDX;
                meshInfo.vertexOffsetBytes = vertexOffsetBytes;
                meshInfo.indexOffsetBytes = indexOffsetBytes;
                meshInfos.push_back(meshInfo);

            }



        }

        std::vector<BufferMetadata> bufferMetadataOnly;

        for (auto& bufferMetadataPair : m_BufferMetadataMap) {
            bufferMetadataOnly.push_back(bufferMetadataPair.second);
        }

        // create all buffers and texture(s)
        // probe ubo
        m_ProbeInfoBuffer = std::make_shared<UniformBuffer>(sizeof(ProbeInfo), BufferUsage::Static, &probeInfo);
        // mesh info ssbo
        m_MeshInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(MeshInfo) * meshInfos.size(), BufferUsage::Static, meshInfos.data());
        m_meshCount = meshInfos.size();
        // buffer buffermetadata ssbo
        m_BufferMetadataBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(BufferMetadata) * bufferMetadataOnly.size(), BufferUsage::Static, bufferMetadataOnly.data());

        // create the textures

        // Calculate total probes
        uint32_t total_probes = probeInfo.probeGridDimensions.x * probeInfo.probeGridDimensions.y * probeInfo.probeGridDimensions.z;

        // Calculate probes per row/col to make atlas as square as possible
        float probeAspect = (float)probeInfo.probeResolution.y / (float)probeInfo.probeResolution.x;
        m_probesPerRow = (uint32_t)glm::ceil(glm::sqrt((float)total_probes * probeAspect));
        uint32_t probesPerCol = (uint32_t)glm::ceil((float)total_probes / m_probesPerRow);

        // Calculate final atlas dimensions
        glm::uvec2 texture_dims;
        texture_dims.x = m_probesPerRow * probeInfo.probeResolution.x;
        texture_dims.y = probesPerCol * probeInfo.probeResolution.y;

        GE_CORE_INFO("DDGI Atlas Config: Total Probes: {}, Probes Per Row: {}, Probes Per Col: {}, Atlas Dims: ({}, {})", total_probes, m_probesPerRow, probesPerCol, texture_dims.x, texture_dims.y);

        TextureSpecification radianceSpec;
        radianceSpec.width = texture_dims.x;
        radianceSpec.height = texture_dims.y;
        radianceSpec.format = TextureFormat::R11G11B10F;

        TextureSpecification visibilitySpec;
        visibilitySpec.width = texture_dims.x;
        visibilitySpec.height = texture_dims.y;
        visibilitySpec.format = TextureFormat::RG16F;

        m_RadianceTexture = Texture2D::create(radianceSpec);
        m_VisibilityTexture = Texture2D::create(visibilitySpec);

        m_RadianceTexture->setMinFilter(TextureFilter::Nearest); // Linear interpolation within a cascade is valid
        m_RadianceTexture->setMagFilter(TextureFilter::Nearest);
        m_RadianceTexture->setWrapS(TextureWrap::ClampToEdge);
        m_RadianceTexture->setWrapT(TextureWrap::ClampToEdge);

        m_RadianceTexture->makeResident();
        m_VisibilityTexture->makeResident();

        populateProbesCompute();


    }
    void DynamicDiffuseGI::populateProbesCompute()
    {

        auto completeBVHNodesBuffer = LBVHManager::getLBVH()->getCompleteBVHNodesBuffer();
        auto twidth = m_RadianceTexture->getWidth();
        auto theight = m_RadianceTexture->getHeight();

        m_DDGI_PopulateProbesShader->bind();

        m_ProbeInfoBuffer->bindBase(0);
        m_MeshInfoBuffer->bindBase(3);
        m_BufferMetadataBuffer->bindBase(2);
        completeBVHNodesBuffer->bindBase(1);

        m_DDGI_PopulateProbesShader->setVec2("u_AtlasTextureResolution", glm::vec2(twidth, theight));
        m_DDGI_PopulateProbesShader->setUint("u_meshCount", m_meshCount);
        m_DDGI_PopulateProbesShader->setUint("u_probesPerRow", m_probesPerRow);

        // bind the texture(s)
        // texture(s)
        m_RadianceTexture->bindCompute(0);
        m_VisibilityTexture->bindCompute(1);

        // dispatch the compute shader
        m_DDGI_PopulateProbesShader->dispatchCompute(twidth / 8, theight / 8, 1);

        // unbind shader
        m_DDGI_PopulateProbesShader->unBind();
    }


    int DynamicDiffuseGI::createBufferMetadata(std::shared_ptr<VertexArray> vao)
    {

        auto bufferMetadataIDX = getBufferMetadataIndex(vao->getID());
        if (bufferMetadataIDX != -1) {
            GE_CORE_WARN("DynamicDiffuseGI::createBufferMetadata - BufferMetadata already exists for VAO ID: {0}", vao->getID());
            return bufferMetadataIDX;
        }

        BufferMetadata bufferMetadata;

        bufferMetadata.positionAttributeOffsetBytes = vao->getBufferLayout().getAttribute(AttributeType::POSITION).offset; 
        bufferMetadata.texCoordAttributeOffsetBytes = vao->getBufferLayout().getAttribute(AttributeType::TEXCOORD_0).offset;
        bufferMetadata.vertexStrideBytes = vao->getBufferLayout().vertexSize;           
        bufferMetadata.indexType = vao->getIndexBuffer()->getIndexType();                   

        bufferMetadata.VBOHandle = vao->getVertexBuffer()->getBufferHandle();
        bufferMetadata.IBOHandle = vao->getIndexBuffer()->getBufferHandle();

        bufferMetadataIDX = m_BufferMetadataMap.size();
        m_BufferMetadataMap.push_back(std::make_pair(vao->getID(), bufferMetadata));

        GE_CORE_TRACE("DynamicDiffuseGI::createBufferMetadata - BufferMetadata created for VAO ID: {0} | Handles: {1} {2} {3} {4} {5} {6}", 
        vao->getID(), bufferMetadata.VBOHandle, bufferMetadata.IBOHandle, bufferMetadata.indexType, bufferMetadata.positionAttributeOffsetBytes, bufferMetadata.texCoordAttributeOffsetBytes, bufferMetadata.vertexStrideBytes);
        return bufferMetadataIDX;
    }

    int DynamicDiffuseGI::getBufferMetadataIndex(uint32_t vaoID)
    {
        for (int i = 0; i < m_BufferMetadataMap.size(); i++)
        {
            if (m_BufferMetadataMap[i].first == vaoID)
            {
                return i;
            }
        }
        return -1;
    }
}
