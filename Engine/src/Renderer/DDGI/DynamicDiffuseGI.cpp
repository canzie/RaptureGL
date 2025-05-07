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
        m_DebugBuffer(nullptr),
        m_PrevRadianceTexture(nullptr),
        m_PrevVisibilityTexture(nullptr),
        m_probesPerRow(0),
        m_isEvenFrame(true),
        m_Hysteresis(0.96f),
        m_SunLightBuffer(nullptr),
        m_isPopulated(false)
    {


        auto& app = Application::getInstance();
        auto project = app.getProject();
        auto shaderPath = project->getConfig().shaderPath;

        auto [shader, handle] = AssetManager::importAsset<Shader>(shaderPath / "DDGI/PopulateProbesDDGI2.cs.glsl");
        m_DDGI_PopulateProbesShader = shader;

        initTextures();

    }

    DynamicDiffuseGI::~DynamicDiffuseGI()
    {
    }

    void DynamicDiffuseGI::populateProbes(std::shared_ptr<Scene> scene)
    {
        if (m_isPopulated)
        {
            auto& skybox = scene->getSkyBox();
            
            auto skyboxTexture = skybox.texture;

            
            populateProbesCompute(skyboxTexture);
            return;
        }

        // get all the meshes in the scene
        auto& reg = scene->getRegistry();
        auto view = reg.view<MeshComponent, TransformComponent, MaterialComponent>();

        std::vector<MeshInfo> meshInfos;
        std::vector<BufferMetadata> bufferMetadata;

        // bind the lbvh buffer
        // get all of the bvh roots, currently just use a "bad" solution until we have a tlas
        if (!LBVHManager::isInitialized()) {
            GE_CORE_ERROR("DynamicDiffuseGI::populateProbes - LBVHManager is not initialized");
            return;
            //LBVHManager::init(scene, false);
        }

        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true}); // Add a memory barrier before reading


        auto meshBVHNodes = LBVHManager::getLBVH()->getAllCPUBVHNodes();

        // get the needed buffer metadata
        // just use the handles for comparisons and set/get the correct index based on this



        // Calculate the total size of the grid and the starting offset
        glm::vec3 totalGridSize = glm::vec3(m_ProbeConfig.probeGridDimensions) * m_ProbeConfig.probeSpacing;
        glm::vec3 startOffset = m_ProbeConfig.probeOrigin - (totalGridSize / 2.0f) + (m_ProbeConfig.probeSpacing / 2.0f); // Offset by half spacing to center probes

        
        for (int x = 0; x < m_ProbeConfig.probeGridDimensions.x; x++) {
            for (int y = 0; y < m_ProbeConfig.probeGridDimensions.y; y++) {
                for (int z = 0; z < m_ProbeConfig.probeGridDimensions.z; z++) {
                    // Calculate the position based on the starting offset and index
                    auto probePosition = startOffset + glm::vec3(x, y, z) * m_ProbeConfig.probeSpacing;
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

        auto lightView = reg.view<LightComponent, TransformComponent>();
        DirectionalLightBufferInfo directionalLightBufferInfo = {glm::vec3(0.0f), 0.0f};

        for (auto entity : lightView) {
            auto [lightComp, transformComp] = lightView.get<LightComponent, TransformComponent>(entity);
            if (lightComp.type == LightType::Directional) {
                directionalLightBufferInfo.direction = transformComp.rotation();
                directionalLightBufferInfo.intensity = lightComp.intensity;
            }
        }

        // create all buffers and texture(s)
        // probe ubo
        m_ProbeInfoBuffer = std::make_shared<UniformBuffer>(sizeof(ProbeInfo), BufferUsage::Static, &m_ProbeConfig);
        // mesh info ssbo
        m_MeshInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(MeshInfo) * meshInfos.size(), BufferUsage::Static, meshInfos.data());
        m_meshCount = meshInfos.size();
        // buffer buffermetadata ssbo
        m_BufferMetadataBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(BufferMetadata) * bufferMetadataOnly.size(), BufferUsage::Static, bufferMetadataOnly.data());

        // sun light ubo
        m_SunLightBuffer = std::make_shared<UniformBuffer>(sizeof(DirectionalLightBufferInfo), BufferUsage::Static, &directionalLightBufferInfo);

        m_DebugBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(DebugData) * meshInfos.size(), BufferUsage::Dynamic, nullptr);

        m_isPopulated = true;

        populateProbesCompute();

        //readDebugBuffer();

    }

    void DynamicDiffuseGI::populateProbesCompute(std::shared_ptr<Texture2D> skyboxTexture)
    {
        if (!m_isPopulated) {
            GE_CORE_WARN("DynamicDiffuseGI::populateProbesCompute - DynamicDiffuseGI is not populated yet, please call 'populateProbes(Scene)' first");
            return;
        }

        auto completeBVHNodesBuffer = LBVHManager::getLBVH()->getCompleteBVHNodesBuffer();
        auto twidth = m_RadianceTexture->getWidth();
        auto theight = m_RadianceTexture->getHeight();

        m_DDGI_PopulateProbesShader->bind();

        m_ProbeInfoBuffer->bindBase(0); // UBO
        m_SunLightBuffer->bindBase(1); // UBO

        m_DebugBuffer->bindBase(4); // SSBO

        m_MeshInfoBuffer->bindBase(3); // SSBO
        m_BufferMetadataBuffer->bindBase(2); // SSBO
        completeBVHNodesBuffer->bindBase(1); // SSBO

        m_DDGI_PopulateProbesShader->setVec2("u_AtlasTextureResolution", glm::vec2(twidth, theight));
        m_DDGI_PopulateProbesShader->setUint("u_meshCount", m_meshCount);
        m_DDGI_PopulateProbesShader->setUint("u_probesPerRow", m_probesPerRow);
        m_DDGI_PopulateProbesShader->setFloat("u_hysteresis", m_Hysteresis);

        // bind the texture(s)
        // texture(s)
        if (m_isEvenFrame) {
            m_RadianceTexture->bindCompute(0);
            m_VisibilityTexture->bindCompute(1);
            m_PrevRadianceTexture->bindCompute(2);
            m_PrevVisibilityTexture->bindCompute(3);
        } else {
            m_PrevRadianceTexture->bindCompute(0);
            m_PrevVisibilityTexture->bindCompute(1);
            m_RadianceTexture->bindCompute(2);
            m_VisibilityTexture->bindCompute(3);
        }

        if (skyboxTexture) {
            skyboxTexture->bind(4);
        }

        m_isEvenFrame = !m_isEvenFrame;

        // dispatch the compute shader
        m_DDGI_PopulateProbesShader->dispatchCompute(twidth / 8, theight / 8, 1);

        // unbind shader
        m_ProbeInfoBuffer->unbind();
        m_SunLightBuffer->unbind();
        m_MeshInfoBuffer->unbind();
        m_BufferMetadataBuffer->unbind();
        completeBVHNodesBuffer->unbind();
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

        // NOTE: this could lead to unintended behavior if the attrib is not present, since it will default to 0
        bufferMetadata.positionAttributeOffsetBytes = vao->getBufferLayout().getAttribute(AttributeType::POSITION).offset; 
        bufferMetadata.texCoordAttributeOffsetBytes = vao->getBufferLayout().getAttribute(AttributeType::TEXCOORD_0).offset;
        bufferMetadata.normalAttributeOffsetBytes = vao->getBufferLayout().getAttribute(AttributeType::NORMAL).offset;
        bufferMetadata.tangentAttributeOffsetBytes = vao->getBufferLayout().getAttribute(AttributeType::TANGENT).offset; 
        bufferMetadata.vertexStrideBytes = vao->getBufferLayout().vertexSize;
        bufferMetadata.indexType = vao->getIndexBuffer()->getIndexType();                   

        bufferMetadata.VBOHandle = vao->getVertexBuffer()->getBufferHandle();
        bufferMetadata.IBOHandle = vao->getIndexBuffer()->getBufferHandle();

        bufferMetadataIDX = m_BufferMetadataMap.size();
        m_BufferMetadataMap.push_back(std::make_pair(vao->getID(), bufferMetadata));

        GE_CORE_TRACE("DynamicDiffuseGI::createBufferMetadata - BufferMetadata created for VAO ID: {0} | idx {1} | Handles: VBO:{2} IBO:{3} | indexType:{4} | posAttrOff:{5} | texCoordAttrOff:{6} | vertexStride:{7}", 
        vao->getID(), bufferMetadataIDX, bufferMetadata.VBOHandle, bufferMetadata.IBOHandle, bufferMetadata.indexType, bufferMetadata.positionAttributeOffsetBytes, bufferMetadata.texCoordAttributeOffsetBytes, bufferMetadata.vertexStrideBytes);
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

    void DynamicDiffuseGI::readDebugBuffer()
    {
        if (!m_DebugBuffer) {
            GE_CORE_WARN("DynamicDiffuseGI::readDebugBuffer - DebugBuffer is not set");
            return;
        }

        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true}); // Add a memory barrier before reading
            
        uint32_t bufferSize = m_meshCount;

        std::vector<DebugData> debugData(bufferSize);

        // Map the buffer to read data
        void* mappedData = m_DebugBuffer->map(0, bufferSize * sizeof(DebugData));
        if (mappedData) {
            // Copy the data from the mapped buffer to the CPU vector
            memcpy(debugData.data(), mappedData, bufferSize * sizeof(DebugData));
            // Unmap the buffer now that we're done reading
            m_DebugBuffer->unmap();
        } else {
            GE_CORE_ERROR("Failed to map DebugBuffer for reading.");
            // Clear the vector to indicate failure, or handle error appropriately
            debugData.clear();
        }



        for (int i = 0; i < debugData.size(); i++) {

            GE_CORE_TRACE("DynamicDiffuseGI::readDebugBuffer - Mesh Index: {0}", debugData[i].meshIndex);
            GE_CORE_TRACE("DynamicDiffuseGI::readDebugBuffer - Transform: {0} {1} {2} {3}", debugData[i].transform[0][0], debugData[i].transform[0][1], debugData[i].transform[0][2], debugData[i].transform[0][3]);
            GE_CORE_TRACE("DynamicDiffuseGI::readDebugBuffer - Transform: {0} {1} {2} {3}", debugData[i].transform[1][0], debugData[i].transform[1][1], debugData[i].transform[1][2], debugData[i].transform[1][3]);
            GE_CORE_TRACE("DynamicDiffuseGI::readDebugBuffer - Transform: {0} {1} {2} {3}", debugData[i].transform[2][0], debugData[i].transform[2][1], debugData[i].transform[2][2], debugData[i].transform[2][3]);
            GE_CORE_TRACE("DynamicDiffuseGI::readDebugBuffer - Transform: {0} {1} {2} {3}", debugData[i].transform[3][0], debugData[i].transform[3][1], debugData[i].transform[3][2], debugData[i].transform[3][3]);

            //GE_CORE_TRACE("DynamicDiffuseGI::readDebugBuffer - Probe {0} - Mesh Index: {1}, Trans form: {2}", debugData[i].meshIndex, debugData[i].transform);
        }

        debugData.clear();
        
    }
    void DynamicDiffuseGI::initTextures()
    {

        // Calculate total probes
        uint32_t total_probes = m_ProbeConfig.probeGridDimensions.x * m_ProbeConfig.probeGridDimensions.y * m_ProbeConfig.probeGridDimensions.z;

        // Calculate probes per row/col to make atlas as square as possible
        float probeAspect = (float)m_ProbeConfig.probeResolution.y / (float)m_ProbeConfig.probeResolution.x;
        m_probesPerRow = (uint32_t)glm::ceil(glm::sqrt((float)total_probes * probeAspect));
        uint32_t probesPerCol = (uint32_t)glm::ceil((float)total_probes / m_probesPerRow);

        // Calculate final atlas dimensions
        glm::uvec2 texture_dims;
        texture_dims.x = m_probesPerRow * m_ProbeConfig.probeResolution.x;
        texture_dims.y = probesPerCol * m_ProbeConfig.probeResolution.y;



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

        m_PrevRadianceTexture = Texture2D::create(radianceSpec);
        m_PrevVisibilityTexture = Texture2D::create(visibilitySpec);


        m_RadianceTexture->setMinFilter(TextureFilter::Nearest); // Linear interpolation within a cascade is valid
        m_RadianceTexture->setMagFilter(TextureFilter::Nearest);
        m_RadianceTexture->setWrapS(TextureWrap::ClampToEdge);
        m_RadianceTexture->setWrapT(TextureWrap::ClampToEdge);

        m_VisibilityTexture->setMinFilter(TextureFilter::Nearest); // Linear interpolation within a cascade is valid
        m_VisibilityTexture->setMagFilter(TextureFilter::Nearest);
        m_VisibilityTexture->setWrapS(TextureWrap::ClampToEdge);
        m_VisibilityTexture->setWrapT(TextureWrap::ClampToEdge);

        m_PrevRadianceTexture->setMinFilter(TextureFilter::Nearest); // Linear interpolation within a cascade is valid
        m_PrevRadianceTexture->setMagFilter(TextureFilter::Nearest);
        m_PrevRadianceTexture->setWrapS(TextureWrap::ClampToEdge);
        m_PrevRadianceTexture->setWrapT(TextureWrap::ClampToEdge);

        m_PrevVisibilityTexture->setMinFilter(TextureFilter::Nearest); // Linear interpolation within a cascade is valid
        m_PrevVisibilityTexture->setMagFilter(TextureFilter::Nearest);
        m_PrevVisibilityTexture->setWrapS(TextureWrap::ClampToEdge);
        m_PrevVisibilityTexture->setWrapT(TextureWrap::ClampToEdge);

        m_RadianceTexture->makeResident();
        m_VisibilityTexture->makeResident();

        m_PrevRadianceTexture->makeResident();
        m_PrevVisibilityTexture->makeResident();

    }
}
