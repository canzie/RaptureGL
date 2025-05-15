#include "DynamicDiffuseGI.h"

#include "../../Scenes/Components/Components.h"
//#include "../../Sorting/SpatialSorting/BVH/LBVH/LBVH.h"

#include "../../WindowContext/Application.h"
#include <glm/gtx/string_cast.hpp>

#include "../../Shaders/OpenGLShaders/OpenGLShader.h"

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
        m_isPopulated(false),
        m_RayDataTexture(nullptr)
    {


        auto& app = Application::getInstance();
        auto project = app.getProject();
        auto shaderPath = project->getConfig().shaderPath;

        auto [traceShader, traceShaderHandle] = AssetManager::importAsset<Shader>(shaderPath / "DDGI/ProbeTraceRGS.cs.glsl");
        m_DDGI_ProbeTraceShader = traceShader;

        auto [flatten2dArrayShader, shaderHandle] = AssetManager::importAsset<Shader>(shaderPath / "Flatten2dArray.cs.glsl");
        m_Flatten2dArrayShader = flatten2dArrayShader;

        auto [blendingShader, blendingShaderHandle] = AssetManager::importAsset<Shader>(shaderPath / "DDGI/ProbeBlending.cs.glsl");
        m_DDGI_ProbeIrradianceBlendingShader = blendingShader;
        // NOTE: this is very criminal, but making it better would derail too much from the ddgi,
        // ok solution would be to create some custom shader file which stores the glsl filepath and the variants, 
        // then give this file to the assetmanager
        // or a way to ask the assetamanger for a copy of a loaded object
        m_DDGI_ProbeDistanceBlendingShader = std::make_shared<OpenGLShader>(shaderPath / "DDGI/ProbeBlending.cs.glsl");

        ShaderVariant radianceVariant;
        radianceVariant.defines.push_back("DDGI_BLEND_RADIANCE");
        radianceVariant.name = "DDGI_BLEND_RADIANCE";
        ShaderVariant distanceVariant;
        distanceVariant.defines.push_back("DDGI_BLEND_DISTANCE");
        distanceVariant.name = "DDGI_BLEND_DISTANCE";

        m_DDGI_ProbeIrradianceBlendingShader->addVariant(radianceVariant);
        m_DDGI_ProbeDistanceBlendingShader->addVariant(distanceVariant);

        m_DDGI_ProbeIrradianceBlendingShader->compile(radianceVariant.name);   
        m_DDGI_ProbeDistanceBlendingShader->compile(distanceVariant.name);

        initProbeInfoBuffer();
        initTextures();

    }


    DynamicDiffuseGI::~DynamicDiffuseGI()
    {
    }

    void DynamicDiffuseGI::populateProbes(std::shared_ptr<Scene> scene)
    {
        if (m_isPopulated)
        {

            updateSunProperties(scene);

            populateProbesCompute(scene);
            return;
        }

        // get all the meshes in the scene
        auto& reg = scene->getRegistry();
        auto view = reg.view<MeshComponent, TransformComponent, MaterialComponent, TagComponent>();

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

        
        for (int x = 0; x < m_ProbeVolume.gridDimensions.x; x++) {
            for (int y = 0; y < m_ProbeVolume.gridDimensions.y; y++) {
                for (int z = 0; z < m_ProbeVolume.gridDimensions.z; z++) {
                    // Calculate the position based on the starting offset and index
                    auto probePosition = m_ProbeVolume.origin + (glm::vec3(x, y, z) - (glm::vec3(m_ProbeVolume.gridDimensions - 1u) / 2.0f)) * m_ProbeVolume.spacing;
                    m_DebugProbePositions.push_back(probePosition);
                    //GE_CORE_TRACE("DynamicDiffuseGI::populateProbes - Probe Index: ({0}, {1}, {2}), Position: ({3}, {4}, {5})", x, y, z, probePosition.x, probePosition.y, probePosition.z);
                }
            }
        }


        
        // get the mesh info data for the buffer
        for (auto entity : view)
        {
            auto [mesh, transform, material, tag] = view.get<MeshComponent, TransformComponent, MaterialComponent, TagComponent>(entity);



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
                meshInfo.InvTransform = glm::inverse(meshInfo.Transform);
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
        //m_ProbeInfoBuffer = std::make_shared<UniformBuffer>(sizeof(ProbeConfig), BufferUsage::Static, &m_ProbeConfig);
        // mesh info ssbo
        m_MeshInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(MeshInfo) * meshInfos.size(), BufferUsage::Static, meshInfos.data());
        m_meshCount = meshInfos.size();
        // buffer buffermetadata ssbo
        m_BufferMetadataBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(BufferMetadata) * bufferMetadataOnly.size(), BufferUsage::Static, bufferMetadataOnly.data());

        // sun light ubo
        m_SunLightBuffer = std::make_shared<UniformBuffer>(sizeof(SunProperties), BufferUsage::Stream, &m_SunShadowProps);

        m_DebugBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(DebugData) * 4864 * 2, BufferUsage::Dynamic, nullptr);

        m_isPopulated = true;

        populateProbesCompute(scene);

        //readDebugBuffer();

    }

    void DynamicDiffuseGI::populateProbesCompute(std::shared_ptr<Scene> scene)
    {
        if (!m_isPopulated) {
            GE_CORE_WARN("DynamicDiffuseGI::populateProbesCompute - DynamicDiffuseGI is not populated yet, please call 'populateProbes(Scene)' first");
            return;
        }


        castRays(scene);
        blendTextures();

        // flatten irradiance texture
        m_Flatten2dArrayShader->bind();
        m_RadianceTexture->bind(0);
        m_IrradianceTextureFlattened->bindCompute(1);

        m_Flatten2dArrayShader->setInt("layerCount", m_RadianceTexture->getDepth());
        m_Flatten2dArrayShader->setInt("layerWidth", m_RadianceTexture->getWidth());
        m_Flatten2dArrayShader->setInt("layerHeight", m_RadianceTexture->getHeight());
        m_Flatten2dArrayShader->setInt("tilesPerRow", static_cast<uint32_t>(ceil(sqrt(m_RadianceTexture->getDepth()))));
        m_Flatten2dArrayShader->dispatchCompute(m_IrradianceTextureFlattened->getWidth()/16, m_IrradianceTextureFlattened->getHeight()/16, 1);

        m_IrradianceTextureFlattened->unbind();
        m_Flatten2dArrayShader->unBind();

        // flatten distance texture
        m_Flatten2dArrayShader->bind();
        m_VisibilityTexture->bind(0);
        m_DistanceTextureFlattened->bindCompute(1);

        m_Flatten2dArrayShader->setInt("layerCount", m_VisibilityTexture->getDepth());
        m_Flatten2dArrayShader->setInt("layerWidth", m_VisibilityTexture->getWidth());
        m_Flatten2dArrayShader->setInt("layerHeight", m_VisibilityTexture->getHeight());
        m_Flatten2dArrayShader->setInt("tilesPerRow", static_cast<uint32_t>(ceil(sqrt(m_VisibilityTexture->getDepth()))));
        m_Flatten2dArrayShader->dispatchCompute(m_DistanceTextureFlattened->getWidth()/16, m_DistanceTextureFlattened->getHeight()/16, 1);

        m_DistanceTextureFlattened->unbind();
        m_Flatten2dArrayShader->unBind();

        m_isEvenFrame = !m_isEvenFrame;

    }

    // phase 1
    void DynamicDiffuseGI::castRays(std::shared_ptr<Scene> scene)
    {
        if (!m_isPopulated) {
            GE_CORE_WARN("DynamicDiffuseGI::castRays - DynamicDiffuseGI is not populated yet, please call 'populateProbes(Scene)' first");
            return;
        }

        auto completeBVHNodesBuffer = LBVHManager::getLBVH()->getCompleteBVHNodesBuffer();

        // bindings
        m_DDGI_ProbeTraceShader->bind();

        m_ProbeInfoBuffer->bindBase(0); // UBO
        m_SunLightBuffer->bindBase(1); // UBO

        completeBVHNodesBuffer->bindBase(0); // SSBO
        m_BufferMetadataBuffer->bindBase(1); // SSBO
        m_MeshInfoBuffer->bindBase(2); // SSBO

        m_RayDataTexture->bindCompute(0);

        if (m_isEvenFrame) {
            m_PrevRadianceTexture->bind(1);
            m_PrevVisibilityTexture->bind(2);
        } else {

            m_RadianceTexture->bind(1);
            m_VisibilityTexture->bind(2);
        }

        auto& skybox = scene->getSkyBox();
        auto skyboxTexture = skybox.texture;

        if (skyboxTexture) {
            skyboxTexture->bind(3);
        }


        m_DDGI_ProbeTraceShader->setUint("u_meshCount", m_meshCount);

        // dispatch the compute shader
        m_DDGI_ProbeTraceShader->dispatchCompute(m_ProbeVolume.gridDimensions.x, m_ProbeVolume.gridDimensions.z, m_ProbeVolume.gridDimensions.y);

        // wait for the raydata to be up to date
        // could also put this before it is used instead but this will probably not cause performance issues now, so we just do it here so we dotn forget it and pull my hair out
        m_RayDataTexture->barrier();

        // unbinds
        m_RayDataTexture->unbind();

        m_ProbeInfoBuffer->unbind();
        m_SunLightBuffer->unbind();
        completeBVHNodesBuffer->unbind();
        m_BufferMetadataBuffer->unbind();
        m_MeshInfoBuffer->unbind();

        m_ProbeInfoBuffer->bindBase(0); // UBO
        m_DDGI_ProbeTraceShader->unBind();

    }


    // phase 2
    void DynamicDiffuseGI::blendTextures()
    {
        // irradiance blending
        m_DDGI_ProbeIrradianceBlendingShader->bind();

        m_RayDataTexture->bindCompute(0);

        if (m_isEvenFrame) {
            m_PrevRadianceTexture->bindCompute(1); // ingore that both are this for now, we are just debugging
            m_PrevVisibilityTexture->bindCompute(2);
            m_RadianceTexture->bindCompute(3); // ingore that both are this for now, we are just debugging
            m_VisibilityTexture->bindCompute(4);
        } else {
            m_RadianceTexture->bindCompute(1);
            m_VisibilityTexture->bindCompute(2);
            m_PrevRadianceTexture->bindCompute(3);
            m_PrevVisibilityTexture->bindCompute(4);
        }      

        m_ProbeInfoBuffer->bindBase(0); // UBO
        m_DDGI_ProbeIrradianceBlendingShader->dispatchCompute(m_ProbeVolume.gridDimensions.x, m_ProbeVolume.gridDimensions.z, m_ProbeVolume.gridDimensions.y);


        m_DDGI_ProbeIrradianceBlendingShader->unBind();


        // distance blending
        m_DDGI_ProbeDistanceBlendingShader->bind();

        m_DDGI_ProbeDistanceBlendingShader->dispatchCompute(m_ProbeVolume.gridDimensions.x, m_ProbeVolume.gridDimensions.z, m_ProbeVolume.gridDimensions.y);

        // the other buffers and textures should still be bound


        m_DDGI_ProbeDistanceBlendingShader->unBind();

        m_ProbeInfoBuffer->unbind();


        m_RadianceTexture->barrier();


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
            
        uint32_t bufferSize = 4864*2;

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



        uint32_t sameCount = 0;
        uint32_t diffCount = 0;
        uint32_t totals = 0;

        std::vector<uint32_t> countersBVH(4864);
        std::vector<uint32_t> countersAll(4864);

        for (int i = 0; i < bufferSize; i++) {
            if (i < 4864) {
                countersBVH[debugData[i].idx]++;
            } else {
                countersAll[debugData[i].idx]++;
            }
        }

        for (int i = 0; i < 4864; i++) {
            if (countersBVH[i] == 1 && countersAll[i] != 1) {
                GE_CORE_ERROR("DynamicDiffuseGI::readDebugBuffer - primitive {0} only present in BVH", i);
            } else if (countersBVH[i] == 1 && countersAll[i] == 1) {
                GE_CORE_INFO("DynamicDiffuseGI::readDebugBuffer - primitive {0} present in BVH and All", i);
            } else if (countersBVH[i] != 1 && countersAll[i] == 1) {
                GE_CORE_ERROR("DynamicDiffuseGI::readDebugBuffer - primitive {0} only present in All", i);
            }

        }

        uint32_t totalinBVH = 0;
        uint32_t totalinAll = 0;

        for (int i = 0; i < 4864; i++) {
            if (countersBVH[i] == 1) {
                totalinBVH++;
            }
            if (countersAll[i] == 1) {
                totalinAll++;
            }
        }

        GE_CORE_INFO("DynamicDiffuseGI::readDebugBuffer - total in BVH: {0} | total in All: {1}", totalinBVH, totalinAll);



        GE_CORE_INFO("DynamicDiffuseGI::readDebugBuffer - same: {0} | diff: {1} | total: {2}, ratio: {3}", sameCount, diffCount, totals, (float)(diffCount) / (float)totals);

        debugData.clear();
        
    }

    void DynamicDiffuseGI::initTextures()
    {

        

        TextureSpecification radianceSpec;
        radianceSpec.width = m_ProbeVolume.gridDimensions.x * m_ProbeVolume.probeNumIrradianceInteriorTexels;
        radianceSpec.height = m_ProbeVolume.gridDimensions.z * m_ProbeVolume.probeNumIrradianceInteriorTexels;
        radianceSpec.depth = m_ProbeVolume.gridDimensions.y;
        radianceSpec.format = TextureFormat::R11G11B10F;

        TextureSpecification DistanceSpec;
        DistanceSpec.width = m_ProbeVolume.gridDimensions.x * m_ProbeVolume.probeNumDistanceInteriorTexels;
        DistanceSpec.height = m_ProbeVolume.gridDimensions.z * m_ProbeVolume.probeNumDistanceInteriorTexels;
        DistanceSpec.depth = m_ProbeVolume.gridDimensions.y;
        DistanceSpec.format = TextureFormat::RG16F;

        TextureSpecification rayDataSpec;
        rayDataSpec.width = m_ProbeVolume.probeNumRays;  // Just the number of rays per probe
        rayDataSpec.height = m_ProbeVolume.gridDimensions.x * m_ProbeVolume.gridDimensions.z;  // Probes per plane
        rayDataSpec.depth = m_ProbeVolume.gridDimensions.y;  // Number of planes
        rayDataSpec.format = TextureFormat::RGBA32F;

        TextureSpecification rayDataFlattenedSpec;
        // Calculate dimensions for the flattened texture
        // We'll arrange the layers in a row-major grid
        uint32_t tilesPerRow = static_cast<uint32_t>(ceil(sqrt(rayDataSpec.depth))); // Number of tiles horizontally


        TextureSpecification irradianceFlattenedSpec;
        // Calculate dimensions for the flattened irradiance texture
        irradianceFlattenedSpec.width = radianceSpec.width * tilesPerRow;
        irradianceFlattenedSpec.height = radianceSpec.height * static_cast<uint32_t>(ceil(float(radianceSpec.depth) / tilesPerRow));
        irradianceFlattenedSpec.format = TextureFormat::R11G11B10F;

        TextureSpecification distanceFlattenedSpec;
        // Calculate dimensions for the flattened distance texture
        distanceFlattenedSpec.width = DistanceSpec.width * tilesPerRow;
        distanceFlattenedSpec.height = DistanceSpec.height * static_cast<uint32_t>(ceil(float(DistanceSpec.depth) / tilesPerRow));
        distanceFlattenedSpec.format = TextureFormat::RG16F;

        TextureFilter samplingType = TextureFilter::Linear;


        m_RadianceTexture = Texture2D::create(radianceSpec);
        m_VisibilityTexture = Texture2D::create(DistanceSpec);

        m_PrevRadianceTexture = Texture2D::create(radianceSpec);
        m_PrevVisibilityTexture = Texture2D::create(DistanceSpec);

        m_RayDataTexture = Texture2D::create(rayDataSpec);
    

        m_IrradianceTextureFlattened = Texture2D::create(irradianceFlattenedSpec);
        m_DistanceTextureFlattened = Texture2D::create(distanceFlattenedSpec);

        m_RayDataTexture->setMinFilter(TextureFilter::Nearest);
        m_RayDataTexture->setMagFilter(TextureFilter::Nearest);
        m_RayDataTexture->setWrapS(TextureWrap::ClampToEdge);
        m_RayDataTexture->setWrapT(TextureWrap::ClampToEdge);

        m_RadianceTexture->setMinFilter(samplingType); // Linear interpolation within a cascade is valid
        m_RadianceTexture->setMagFilter(samplingType);
        m_RadianceTexture->setWrapS(TextureWrap::ClampToEdge);
        m_RadianceTexture->setWrapT(TextureWrap::ClampToEdge);

        m_VisibilityTexture->setMinFilter(samplingType); // Linear interpolation within a cascade is valid
        m_VisibilityTexture->setMagFilter(samplingType);
        m_VisibilityTexture->setWrapS(TextureWrap::ClampToEdge);
        m_VisibilityTexture->setWrapT(TextureWrap::ClampToEdge);

        m_PrevRadianceTexture->setMinFilter(samplingType); // Linear interpolation within a cascade is valid
        m_PrevRadianceTexture->setMagFilter(samplingType);
        m_PrevRadianceTexture->setWrapS(TextureWrap::ClampToEdge);
        m_PrevRadianceTexture->setWrapT(TextureWrap::ClampToEdge);

        m_PrevVisibilityTexture->setMinFilter(samplingType); // Linear interpolation within a cascade is valid
        m_PrevVisibilityTexture->setMagFilter(samplingType);
        m_PrevVisibilityTexture->setWrapS(TextureWrap::ClampToEdge);
        m_PrevVisibilityTexture->setWrapT(TextureWrap::ClampToEdge);

        m_RadianceTexture->makeResident();
        m_VisibilityTexture->makeResident();

        m_PrevRadianceTexture->makeResident();
        m_PrevVisibilityTexture->makeResident();
        
        m_RayDataTexture->makeResident();

        m_IrradianceTextureFlattened->makeResident();
        m_DistanceTextureFlattened->makeResident();

        // dont know if this is needed but i cba
        //m_PrevRadianceTexture->clear(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        //m_PrevVisibilityTexture->clear(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        //m_RadianceTexture->clear(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        //m_VisibilityTexture->clear(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    }
    void DynamicDiffuseGI::updateSunProperties(std::shared_ptr<Scene> scene)
    {
        auto& reg = scene->getRegistry();
        auto lightView = reg.view<LightComponent, TransformComponent>();
        

        for (auto entity : lightView) {
            auto [lightComp, transformComp] = lightView.get<LightComponent, TransformComponent>(entity);
            if (lightComp.type == LightType::Directional) {
                Entity ent = Entity(entity, scene.get());

                auto sm = ent.tryGetComponent<ShadowComponent>();
                if (sm->shadowMap) {
                    m_SunShadowProps.sunLightSpaceMatrix = sm->shadowMap->getWVPMatrix();
                    if (sm->shadowMap->getShadowMapHandle() == 0) {
                        GE_CORE_WARN("DynamicDiffuseGI::populateProbes - Entity has no shadow mapping component");
                    }

                    m_SunShadowProps.sunShadowTextureArrayHandle = sm->shadowMap->getShadowMapHandle();

                } else {
                    GE_CORE_WARN("DynamicDiffuseGI::populateProbes - Entity has no cascaded shadow mapping component");
                    // If no CSM, ensure cascade count is 0 so shader returns shadowFactor = 1.0
                    m_SunShadowProps.sunShadowTextureArrayHandle = 0;
                }


                glm::quat rotationQuat = transformComp.transforms.getRotationQuat();
                m_SunShadowProps.sunDirectionWorld = glm::normalize(rotationQuat * glm::vec3(0, 0, -1));
                m_SunShadowProps.sunColor = lightComp.color;

                m_SunShadowProps.sunIntensity = lightComp.intensity;

                break;
            }
        }

        m_SunLightBuffer->setData(&m_SunShadowProps, sizeof(SunProperties));

    }
    void DynamicDiffuseGI::initProbeInfoBuffer()
    {
        ProbeVolume probeVolume;

        probeVolume.origin = glm::vec3(-0.4f, 5.4f, -0.25f);
        
        probeVolume.rotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        probeVolume.probeRayRotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        
        probeVolume.spacing = glm::vec3(1.02f, 1.5f, 1.02f);
        probeVolume.gridDimensions = glm::uvec3(24, 8, 24);
        
        probeVolume.probeNumRays = 256;
        probeVolume.probeNumIrradianceInteriorTexels = 8;
        probeVolume.probeNumDistanceInteriorTexels = 16;

        probeVolume.probeHysteresis = 0.97f;
        probeVolume.probeMaxRayDistance = 10000.0f;
        probeVolume.probeNormalBias = 0.1f;
        probeVolume.probeViewBias = 0.1f;
        probeVolume.probeDistanceExponent = 2.0f;
        probeVolume.probeIrradianceEncodingGamma = 2.2f;

        probeVolume.probeBrightnessThreshold = 0.1f;

        probeVolume.probeMinFrontfaceDistance = 0.1f;
    
        probeVolume.probeRandomRayBackfaceThreshold = 0.1f;
        probeVolume.probeFixedRayBackfaceThreshold = 0.25f;

        m_ProbeVolume = probeVolume;

        m_ProbeInfoBuffer = std::make_shared<UniformBuffer>(sizeof(ProbeVolume), BufferUsage::Static, &probeVolume);
        

        

    }
}
