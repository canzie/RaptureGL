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
        m_VisibilityTexture(nullptr)
    {

        auto& app = Application::getInstance();
        auto project = app.getProject();
        auto shaderPath = project->getConfig().shaderPath;

        auto [shader, handle] = AssetManager::importAsset<Shader>(shaderPath / "DDGI/PopulateProbesDDGI.cs.glsl");
        m_DDGI_PopulateProbesShader = shader;

    }

    DynamicDiffuseGI::~DynamicDiffuseGI()
    {
    }

    void DynamicDiffuseGI::populateProbes(std::shared_ptr<Scene> scene)
    {
        // get all the meshes in the scene
        auto& reg = scene->getRegistry();
        auto view = reg.view<MeshComponent, TransformComponent>();

        std::vector<MeshInfo> meshInfos;
        std::vector<BufferMetadata> bufferMetadata;

        // bind the lbvh buffer
        // get all of the bvh roots, currently just use a "bad" solution until we have a tlas
        auto meshBVHNodes = LBVHManager::getLBVH()->getAllCPUBVHNodes();
        auto completeBVHNodesBuffer = LBVHManager::getLBVH()->getCompleteBVHNodesBuffer();

        // get the needed buffer metadata
        // just use the handles for comparisons and set/get the correct index based on this


        //probe info
        ProbeInfo probeInfo;
        probeInfo.probeGridDimensions = glm::uvec3(16, 8, 16);
        probeInfo.probeResolution = glm::uvec2(4, 4);
        probeInfo.probeSpacing = glm::vec3(1.0f, 1.0f, 1.0f);

        // camera positions, rotation does not matter since we place porbes around the camera, even behind
        probeInfo.probeOrigin = glm::vec3(0.0f, 0.0f, 0.0f);

        

        // get the mesh info data for the buffer
        for (auto entity : view)
        {
            auto [mesh, transform] = view.get<MeshComponent, TransformComponent>(entity);

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



                MeshInfo meshInfo;
                meshInfo.RootIndex = rootIndex;
                //meshInfo.AlbedoTextureHandle = ...;
                //meshInfo.NormalTextureHandle = ...;
                //meshInfo.MetallicRoughnessTextureHandle = ...;
                meshInfo.Transform = transform.transformMatrix();
                meshInfo.bufferMetadataIDX = bufferMetadataIDX;
                meshInfo.vertexOffsetBytes = vertexOffsetBytes;
                meshInfo.indexOffsetBytes = indexOffsetBytes;
                meshInfos.push_back(meshInfo);
            }
        }

        std::vector<BufferMetadata> bufferMetadataOnly(m_BufferMetadataMap.size());

        for (auto& bufferMetadata : m_BufferMetadataMap) {
            bufferMetadataOnly.push_back(bufferMetadata.second);
        }

        // create all buffers and texture(s)
        // probe ubo
        m_ProbeInfoBuffer = std::make_shared<UniformBuffer>(sizeof(ProbeInfo), BufferUsage::Static, &probeInfo);
        // mesh info ssbo
        m_MeshInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(MeshInfo) * meshInfos.size(), BufferUsage::Static, meshInfos.data());
        // buffer buffermetadata ssbo
        m_BufferMetadataBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(BufferMetadata) * bufferMetadataOnly.size(), BufferUsage::Static, bufferMetadataOnly.data());

        // create the textures

        glm::uvec2 texture_dims;
        uint32_t total_probes = probeInfo.probeGridDimensions.x * probeInfo.probeGridDimensions.y * probeInfo.probeGridDimensions.z;
        texture_dims.y = probeInfo.probeResolution.y;
        texture_dims.x = probeInfo.probeResolution.x * total_probes;


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

        // start the compute shader

        m_DDGI_PopulateProbesShader->bind();

        m_ProbeInfoBuffer->bindBase(0);
        m_MeshInfoBuffer->bindBase(3);
        m_BufferMetadataBuffer->bindBase(2);
        completeBVHNodesBuffer->bindBase(1);


        // bind the texture(s)
        // texture(s)
        m_RadianceTexture->bindCompute(0);
        m_VisibilityTexture->bindCompute(1);

        // dispatch the compute shader
        //m_DDGI_PopulateProbesShader->dispatchCompute(...);

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
