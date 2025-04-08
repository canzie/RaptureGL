#include "glTF2Loader.h"

#include <fstream>
#include <iostream>
#include <type_traits>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../../Scenes/Components/Components.h"
#include "../../Logger/Log.h"
#include "../../Textures/Texture.h"
#include "../../Materials/Material.h"
#include "../../Debug/TracyProfiler.h"

#include "../../Scenes/Systems/BoundingBoxSystem.h"
#include "../../Utils/GLCapabilities.h"

#define DIRNAME "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/build/bin/Debug/assets/models/"

namespace Rapture {

    bool ModelLoadersCache::s_initialized = false;
    std::map<std::string, std::weak_ptr<glTF2Loader>> ModelLoadersCache::s_loaders;
    std::mutex ModelLoadersCache::s_mutex;

    glTF2Loader::glTF2Loader(std::shared_ptr<Scene> scene)
        : m_scene(scene)
    {
        if (!m_scene) {
            GE_CORE_WARN("glTF2Loader: Scene pointer is null");
        }
    }

    glTF2Loader::~glTF2Loader()
    {
        cleanUp();
    }

    
    bool glTF2Loader::initialize(const std::string &filepath)
    {

        m_isLoaded = false;
        if (m_isInitialized) return true;

        // Load the gltf file
        std::ifstream gltf_file(filepath);
        if (!gltf_file)
        {
            GE_CORE_ERROR("glTF2Loader: Couldn't load glTF file '{}'", filepath);
            return false;
        }
        
        m_filepath = filepath;

        // Parse the JSON file
        try {
            gltf_file >> m_glTFfile;
        }
        catch (const std::exception& e) {
            GE_CORE_ERROR("glTF2Loader: Failed to parse glTF JSON: {}", e.what());
            return false;
        }
        gltf_file.close();

        // Load references to major sections
        m_accessors = m_glTFfile.value("accessors", json::array());
        m_meshes = m_glTFfile.value("meshes", json::array());
        m_bufferViews = m_glTFfile.value("bufferViews", json::array());
        m_buffers = m_glTFfile.value("buffers", json::array());
        m_nodes = m_glTFfile.value("nodes", json::array());
        m_materials = m_glTFfile.value("materials", json::array());
        m_animations = m_glTFfile.value("animations", json::array());
        m_skins = m_glTFfile.value("skins", json::array());
        m_textures = m_glTFfile.value("textures", json::array());
        m_images = m_glTFfile.value("images", json::array());
        m_samplers = m_glTFfile.value("samplers", json::array());


        // Validate required sections
        if (m_accessors.empty() || m_meshes.empty() || m_bufferViews.empty() || m_buffers.empty()) {
            GE_CORE_ERROR("glTF2Loader: Missing required glTF sections");
            return false;
        }

        // Extract the directory path from the filepath
        m_basePath = "";
        size_t lastSlashPos = filepath.find_last_of("/\\");
        if (lastSlashPos != std::string::npos) {
            m_basePath = filepath.substr(0, lastSlashPos + 1);
        }

        // Load the bin file with all the buffer data
        std::string bufferURI = m_buffers[0].value("uri", "");
        if (bufferURI.empty()) {
            GE_CORE_ERROR("glTF2Loader: Buffer URI is missing");
            return false;
        }
        
        // Check if the buffer URI is a relative path
        if (bufferURI.find("://") == std::string::npos && !bufferURI.empty()) {
            // Combine the directory path with the buffer URI
            bufferURI = m_basePath + bufferURI;
        }
        std::ifstream binary_file(bufferURI, std::ios::binary);
        if (!binary_file)
        {
            GE_CORE_ERROR("glTF2Loader: Couldn't load binary file '{}'", bufferURI);
            return false;
        }
        
        // Get file size and reserve space
        binary_file.seekg(0, std::ios::end);
        size_t fileSize = binary_file.tellg();
        binary_file.seekg(0, std::ios::beg);
        
        m_binVec.resize(fileSize);
        RAPTURE_PROFILE_ALLOC(m_binVec.data(), fileSize); // Track allocation
        
        // Read the entire file at once for efficiency
        if (!binary_file.read(reinterpret_cast<char*>(m_binVec.data()), fileSize)) {
            GE_CORE_ERROR("glTF2Loader: Failed to read binary data");
            return false;
        }
        
        binary_file.close();

        m_isInitialized = true;

        return true;
    }

    bool glTF2Loader::loadModel(const std::string& filepath, bool isAbsolute, bool calculateBoundingBoxes)
    {

        if (m_scene == nullptr) {
            GE_CORE_ERROR("glTF2Loader: Scene pointer is null");
            return false;
        }

        // Reset state to ensure clean loading
        cleanUp();
        
        // Set the bounding box calculation flag
        m_calculateBoundingBoxes = true;
        
        // Report initial progress
        reportProgress(0.0f);
        
        std::string fullPath = isAbsolute ? filepath : DIRNAME + filepath;

        if (!initialize(fullPath)) {
            GE_CORE_ERROR("glTF2Loader: Failed to initialize loader for '{}'", fullPath);
            return false;
        }

        // Create a root entity for the model
        Entity rootEntity = m_scene->createEntity("glTF_Model");
        
        GE_CORE_INFO("glTF2Loader: Loading model from '{}'", fullPath);

        // Check if the model has animations
        if (!m_animations.empty()) {
            GE_CORE_INFO("glTF2Loader: Model has {} animations", m_animations.size());
        }

        // Process the default scene or the first scene if default not specified
        int defaultScene = m_glTFfile.value("scene", 0);
        if (m_glTFfile.contains("scenes") && !m_glTFfile["scenes"].empty()) {
            processScene(m_glTFfile["scenes"][defaultScene]);
        }
        else if (!m_nodes.empty()) {
            // If no scenes but has nodes, process the first node as root
            Entity nodeEntity = m_scene->createEntity("Root Node");
            nodeEntity.addComponent<RootComponent>();

            processNode(nodeEntity, m_nodes[0]);
        }
        
        // Clean up
        cleanUp();

        m_isLoaded = true;
        
        return true;
    }

    void glTF2Loader::processScene(json& sceneJSON)
    {
        // Create a root entity for the scene
        std::string sceneName = sceneJSON.value("name", "Scene");
        
        // Process each node in the scene
        if (sceneJSON.contains("nodes") && !sceneJSON["nodes"].empty()) {
            for (auto& nodeIdx : sceneJSON["nodes"]) {
                unsigned int nodeIndex = nodeIdx.get<unsigned int>();
                if (nodeIndex < m_nodes.size()) {
                    Entity nodeEntity = m_scene->createEntity("Node " + std::to_string(nodeIndex));
                    processNode(nodeEntity, m_nodes[nodeIndex]);
                }
            }
        }
    }

    glm::mat4 glTF2Loader::getNodeTransform(json& nodeJSON) {

        glm::mat4 transformMatrix = glm::mat4(1.0f);

        // Extract transform components if present
        if (nodeJSON.contains("matrix")) {
            // Use matrix directly
            float matrixValues[16];
            for (int i = 0; i < 16; i++) {
                matrixValues[i] = nodeJSON["matrix"][i];
            }

            transformMatrix = glm::make_mat4(matrixValues);

        }
        else {
            // Use TRS components
            glm::vec3 translation(0.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);
            
            if (nodeJSON.contains("translation"))
                translation = glm::vec3(
                    nodeJSON["translation"][0],
                    nodeJSON["translation"][1],
                    nodeJSON["translation"][2]
                );
                
            if (nodeJSON.contains("rotation")) {
                // glTF quaternions are [x,y,z,w], but glm::quat constructor takes [w,x,y,z]
                rotation = glm::quat(
                    nodeJSON["rotation"][3], // w
                    nodeJSON["rotation"][0], // x
                    nodeJSON["rotation"][1], // y
                    nodeJSON["rotation"][2]  // z
                );
            }
                
            if (nodeJSON.contains("scale"))
                scale = glm::vec3(
                    nodeJSON["scale"][0],
                    nodeJSON["scale"][1],
                    nodeJSON["scale"][2]
                );
            
            // Build transform matrix correctly using GLM
            
            transformMatrix = glm::translate(transformMatrix, translation);
            transformMatrix = transformMatrix * glm::mat4_cast(rotation);
            transformMatrix = glm::scale(transformMatrix, scale);
        }


    return transformMatrix;

    }

    NodeType glTF2Loader::processNode(Entity nodeEntity, json& nodeJSON)
    {

        if (!nodeEntity.hasComponent<EntityNodeComponent>()) {
            nodeEntity.addComponent<EntityNodeComponent>(nodeEntity);
        }

        // Create a new entity for this node
        std::string nodeName = nodeJSON.value("name", "");
        //Entity nodeEntity = m_scene->createEntity(nodeName);

        // Update the tag
        if (!nodeName.empty()) {
            nodeEntity.getComponent<TagComponent>().tag = nodeName;
        }
        auto& nodeEntityComp = nodeEntity.getComponent<EntityNodeComponent>();
        

        nodeEntity.addComponent<TransformComponent>();
        auto& transformComp = nodeEntity.getComponent<TransformComponent>();


        glm::mat4 nodeTransform = getNodeTransform(nodeJSON);

        std::shared_ptr<EntityNode> parent = nodeEntityComp.entity_node->getParent();
        if (parent != nullptr) {
            nodeTransform = parent->getEntity()->getComponent<TransformComponent>().transformMatrix() * nodeTransform;
        }

        transformComp.transforms.setTransform(nodeTransform);

        
        // If this node has a mesh, process it
        if (nodeJSON.contains("mesh")) {
            unsigned int meshIndex = nodeJSON["mesh"];
            if (meshIndex < m_meshes.size()) {
                processMesh(nodeEntity, m_meshes[meshIndex]);
            }
        }

        // If this node has a skin, process it
        if (nodeJSON.contains("skin")) {
            unsigned int skinIndex = nodeJSON["skin"];
            if (skinIndex < m_skins.size()) {
                processSkeleton(nodeEntity, m_skins[skinIndex]);
            }
        }

        
        bool hasMeshChild = false;
        // Process children
        if (nodeJSON.contains("children")) {
            for (auto& childIdx : nodeJSON["children"]) {
                unsigned int childIndex = childIdx.get<unsigned int>();
                if (childIndex < m_nodes.size()) {
                    Entity childEntity = m_scene->createEntity("Node " + std::to_string(childIndex));
                    childEntity.addComponent<EntityNodeComponent>(childEntity, nodeEntity.getComponent<EntityNodeComponent>().entity_node);

                    nodeEntity.getComponent<EntityNodeComponent>().entity_node->addChild(childEntity.getComponent<EntityNodeComponent>().entity_node);

                    NodeType nodeType = processNode(childEntity, m_nodes[childIndex]);

                    if (nodeType == NodeType::Bone) {
                        // use the child transform as the bone transform

                        // then remove the entity, as it is a bone and we just need the transform
                        m_scene->destroyEntity(childEntity); 

                    // child is either a mesh, or an empty node which has a mesh somewhere as its child
                    //    if the leaf node was not a mesh, it would be a bone type
                    //    and propagated up the tree until a mesh was found, then it will always be a mesh or empty type
                    } else if (nodeType == NodeType::Mesh || nodeType == NodeType::Empty){
                        hasMeshChild = true;
                    
                    }
                }
            }
        }

        // we are the mesh
        if (nodeJSON.contains("mesh")) {
            return NodeType::Mesh;
        
        // descendants have a mesh
        } else if (hasMeshChild) {
            return NodeType::Empty;
        
        // we have a skeleton
        } else if (nodeJSON.contains("skin")) {
            return NodeType::Skeleton;
        
        // we are a bone
        } else {
            return NodeType::Bone;
        }

    }

    void glTF2Loader::processSkeleton(Entity entity, json& skinJSON)
    {

        std::string skeletonName = skinJSON.value("name", "Skeleton");

        GE_CORE_TRACE("glTF2Loader: Processing skeleton: {}", skeletonName);

        // create a skeleton entity
        entity.addComponent<SkeletonComponent>(skeletonName);

        auto skeletonComp = entity.tryGetComponent<SkeletonComponent>();
        if (skeletonComp == nullptr) {
            GE_CORE_ERROR("glTF2Loader: Skeleton entity missing SkeletonComponent");
            return;
        }
        
        unsigned int skeletonIndex = skinJSON.value("skeleton", 0);
        unsigned int inverseBindIndex = skinJSON.value("inverseBindMatrices", std::numeric_limits<unsigned int>::max());

        // default root joint root index
        unsigned int rootIndex = skinJSON["joints"][0];

        // optional skeleton tag
        if (skinJSON.contains("skeleton") && skeletonIndex != rootIndex) {
            json& skeletonNodeJSON = m_nodes[skeletonIndex];

            glm::mat4 nodeTransform = getNodeTransform(skeletonNodeJSON);
            skeletonComp->skeleton->setRootBoneTransform(nodeTransform);
            
            auto transformComp = entity.tryGetComponent<TransformComponent>();
            auto entNodeComp = entity.tryGetComponent<EntityNodeComponent>();
            if (entNodeComp) {
                if (entNodeComp->entity_node->getParent() != nullptr) {
                    TransformComponent& parentTransformComp = entNodeComp->entity_node->getParentComponent<TransformComponent>();
                    transformComp->transforms.setTransform(parentTransformComp.transformMatrix() * nodeTransform);
                } else {
                    transformComp->transforms.setTransform(nodeTransform);
                }
                

            }

            // get the root index from the skeleton node
            rootIndex = skeletonNodeJSON["children"][0];
        }


        std::vector<std::string> boneNames;
        for (auto& bone : skinJSON["joints"]) {
            unsigned int boneIdx = bone.get<unsigned int>();
            boneNames.push_back(std::to_string(boneIdx));
        }

        // create empty bones in the order of the joints array
        skeletonComp->skeleton->createBones(boneNames);

        // process all of the bones via graph traversal
        processBone(entity, rootIndex);


        if (inverseBindIndex < m_accessors.size()) {
            std::vector<unsigned char> inverseBinds;
            loadAccessor(m_accessors[inverseBindIndex], inverseBinds);

            // convert the inverse binds to a vector of glm::mat4
            std::vector<glm::mat4> inverseBindMatrices;
            int j = 0;
            for (unsigned int i = 0; i < inverseBinds.size(); i += 16*4) {
                glm::mat4 matrix = glm::make_mat4(reinterpret_cast<float*>(inverseBinds.data() + i));
                inverseBindMatrices.push_back(matrix);

            }

            GE_CORE_TRACE("Inverse Bind Matrices Size: {0}", inverseBindMatrices.size());

            // apply the inverse bind matrices to the bones
            skeletonComp->skeleton->applyInverseBinds(inverseBindMatrices);
        }

        // Check if the file has animations, and if so, add them to the entity
        if (!m_animations.empty()) {
            std::vector<std::shared_ptr<Animation>> animations = processAnimations(m_animations);
            
            if (!animations.empty()) {
                GE_CORE_INFO("Found {} animations for skeleton: {}", animations.size(), skeletonName);
                
                // Add animation component with all loaded animations
                entity.addComponent<AnimationComponent>(animations);
                
                // If autoPlay is desired, start the first animation
                auto animComp = entity.getComponent<AnimationComponent>();
                if (animComp.autoPlay) {
                    animComp.playAnimation();
                }
            }
        }
    }

    void glTF2Loader::processBone(Entity entity, unsigned int boneIndex)
    {

        json& boneJSON = m_nodes[boneIndex];
        auto skeletonComp = entity.tryGetComponent<SkeletonComponent>();

        if (skeletonComp == nullptr) {
            GE_CORE_ERROR("Bone entity missing SkeletonComponent");
            return;
        }

        auto bone = skeletonComp->skeleton->getBone(std::to_string(boneIndex));
        if (bone == nullptr) {
            GE_CORE_ERROR("Bone not found");
            return;
        }


        bone->transform = getNodeTransform(boneJSON);



        if (boneJSON.contains("children")) {
            for (auto& childBoneIndex : boneJSON["children"]) {
                unsigned int childBoneIndexInt = childBoneIndex.get<unsigned int>();
                auto childBone = skeletonComp->skeleton->getBone(std::to_string(childBoneIndexInt));
                if (childBone != nullptr) {

                    // automatically updates the parent of the child bone
                    bone->addChild(childBone);
                    processBone(entity, childBoneIndex);
                }
            }
        }

    }

    Entity glTF2Loader::processMesh(Entity parent, json& meshJSON)
    {
        auto& parentTransform = parent.getComponent<TransformComponent>();


        std::string meshName = meshJSON.value("name", "Mesh");
        Entity meshEntity = m_scene->createEntity(meshName);
        // Create transform component that inherits parent transform
        meshEntity.addComponent<TransformComponent>(parentTransform.transformMatrix());
        
        
        // Check if parent entity has EntityNodeComponent
        if (!parent.hasComponent<EntityNodeComponent>()) {
            GE_CORE_ERROR("Parent entity '{}' missing EntityNodeComponent", meshName);
            return meshEntity;
        }
        
        meshEntity.addComponent<EntityNodeComponent>(meshEntity, parent.getComponent<EntityNodeComponent>().entity_node);
        
        // Check if mesh entity has EntityNodeComponent after adding it
        if (!meshEntity.hasComponent<EntityNodeComponent>()) {
            GE_CORE_ERROR("Mesh entity '{}' failed to add EntityNodeComponent", meshName);
            return meshEntity;
        }
        
        std::shared_ptr<EntityNode> mesh_entity_node = meshEntity.getComponent<EntityNodeComponent>().entity_node;
        
        parent.getComponent<EntityNodeComponent>().entity_node->addChild(mesh_entity_node);
    
        // Process primitives
        if (meshJSON.contains("primitives") && !meshJSON["primitives"].empty()) {
            int primitiveIndex = 0;
            for (auto& primitive : meshJSON["primitives"]) {
                // For each primitive, create a new entity
                Entity primitiveEntity = m_scene->createEntity("_Primitive_" + std::to_string(primitiveIndex) + "_" + meshName);
                primitiveEntity.addComponent<EntityNodeComponent>(primitiveEntity, mesh_entity_node);
                mesh_entity_node->addChild(primitiveEntity.getComponent<EntityNodeComponent>().entity_node);

                primitiveEntity.addComponent<TransformComponent>(parentTransform.transformMatrix());

                // Process the primitive data
                processPrimitive(primitiveEntity, primitive);
                primitiveIndex++;
            }
        }
        
        return meshEntity;
    }

    void glTF2Loader::processPrimitive(Entity entity, json& primitive)
    {
        // Add mesh component to the entity
        entity.addComponent<MeshComponent>(true);
        entity.addComponent<MaterialComponent>();
        
        // Check if entity has MeshComponent before accessing it
        if (!entity.hasComponent<MeshComponent>()) {
            GE_CORE_ERROR("Entity missing MeshComponent");
            return;
        }
        auto& meshComp = entity.getComponent<MeshComponent>();

        BufferLayout bufferLayout;
        
        // Gather attribute data and calculate attribute sizes
        std::vector<std::pair<std::string, std::vector<unsigned char>>> attributeData;
        
        std::vector<unsigned char> temp_indexData;
        unsigned int temp_indCount = 0;
        unsigned int temp_compType = 0;
        unsigned int vertexCount = 0;

        // Process vertex attributes
        if (primitive.contains("attributes")) {
            json& attribs = primitive["attributes"];
            
            // First pass: gather data and determine vertex count
            for (auto& attrib : attribs.items()) {
                std::string name = attrib.key();
                if (name == "COLOR_0") continue; // Skip color data for now
                
                unsigned int accessorIdx = attrib.value();
                json& accessor = m_accessors[accessorIdx];
                
                // Get vertex count from the first attribute (should be the same for all attributes)
                if (vertexCount == 0 && accessor.contains("count")) {
                    vertexCount = accessor["count"];
                }
                
                // Load attribute data
                std::vector<unsigned char> attrData;
                loadAccessor(m_accessors[accessorIdx], attrData);
                
                if (!attrData.empty()) {
                    attributeData.push_back({name, attrData});
                }
            }
        }

        // Early exit if no vertex data
        if (attributeData.empty() || vertexCount == 0) {
            GE_CORE_ERROR("No vertex data found for primitive");
            return;
        }

        // Calculate attribute sizes and strides
        size_t vertexStride = 0;
        std::vector<size_t> attrSizes;
        std::vector<size_t> attrOffsets;
        
        for (const auto& [name, data] : attributeData) {
            size_t attrSize = data.size() / vertexCount;
            attrSizes.push_back(attrSize);
            attrOffsets.push_back(vertexStride);
            vertexStride += attrSize;
        }
        
        // Create buffer layout for interleaved data
        size_t currentOffset = 0;
        size_t positionOffset = 0;
        bool foundPosition = false;
        
        for (size_t i = 0; i < attributeData.size(); i++) {
            const auto& [name, data] = attributeData[i];
            unsigned int accessorIdx = primitive["attributes"][name];
            json& accessor = m_accessors[accessorIdx];
            
            unsigned int componentType = accessor["componentType"];
            std::string type = accessor["type"];
            
            // For interleaved data, the offset is the relative position within a single vertex
            bufferLayout.buffer_attribs.push_back({name, componentType, type, attrOffsets[i]});
            
            // Find position attribute and record its offset
            if (name == "POSITION") {
                positionOffset = attrOffsets[i] / sizeof(float);
                foundPosition = true;
            }
        }

        // Set interleaved flag to true
        bufferLayout.isInterleaved = true;
        bufferLayout.vertexSize = vertexStride;
        
        // Create vertex buffer with the correct size
        size_t totalVertexDataSize = vertexCount * vertexStride;
        
        // Pre-allocate interleaved data with the exact known size
        std::vector<unsigned char> interleavedData;
        interleavedData.reserve(totalVertexDataSize);
        interleavedData.resize(totalVertexDataSize);
        
        // Fill the interleaved buffer using direct memory access
        for (unsigned int v = 0; v < vertexCount; v++) {
            unsigned char* vertexDest = interleavedData.data() + (v * vertexStride);
            for (size_t a = 0; a < attributeData.size(); a++) {
                const auto& [name, data] = attributeData[a];
                size_t attrSize = attrSizes[a];
                const unsigned char* attrSrc = data.data() + (v * attrSize);
                unsigned char* attrDest = vertexDest + attrOffsets[a];
                
                // Direct memory copy
                std::memcpy(attrDest, attrSrc, attrSize);
            }
        }
        
        // Calculate bounding box from the interleaved vertex data if position data is available
        BoundingBox localBoundingBox;
        if (m_calculateBoundingBoxes && foundPosition) {
            RAPTURE_PROFILE_SCOPE("Calculate Bounding Box");
            
            // Calculate bounding box directly from vertex data in RAM
            size_t floatStride = vertexStride / sizeof(float);
            localBoundingBox = BoundingBoxSystem::calculateFromVertexData(
                interleavedData.data(), 
                totalVertexDataSize, 
                floatStride, 
                positionOffset
            );
            
            if (localBoundingBox.isValid()) {
                GE_CORE_INFO("Calculated bounding box during mesh loading");
                localBoundingBox.logBounds();
            }
        }
        
        // Process indices if present
        std::vector<unsigned char> indexData;
        unsigned int compType = 0;
        unsigned int indCount = 0;

        if (primitive.contains("indices")) {
            unsigned int indicesIdx = primitive["indices"];
            
            // Pre-allocate index data to avoid reallocation
            indexData.reserve(m_accessors[indicesIdx].value("count", 0) * 4); // Worst case: 4 bytes per index
            
            // Load index data
            loadAccessor(m_accessors[indicesIdx], indexData);
            
            if (!indexData.empty()) {
                // Get index component type
                compType = m_accessors[indicesIdx]["componentType"];
                indCount = m_accessors[indicesIdx]["count"];
            }
        }
        
        {
            RAPTURE_PROFILE_SCOPE("Set Mesh Data");
            if (indexData.size() > 0) {
                meshComp.mesh->setMeshData(bufferLayout, 
                    interleavedData.data(), 
                    totalVertexDataSize, 
                    indexData.data(), 
                    indexData.size(), 
                    indCount, 
                    compType);
            } else {
                GE_CORE_ERROR("glTF2Loader: Vertex data only not supported yet");
                entity.removeComponent<MeshComponent>();
                return;
            }
        }
        
        // Set material if present
        if (primitive.contains("material")) {
            uint32_t materialIdx = primitive["material"];
            if (materialIdx < m_materials.size()) {
                if (!entity.hasComponent<MaterialComponent>()) {
                    GE_CORE_WARN("Entity missing MaterialComponent for material index {}", materialIdx);
                    entity.addComponent<MaterialComponent>();
                }

                json& materialJSON = m_materials[materialIdx];
                
                // Check if this material uses the KHR_materials_pbrSpecularGlossiness extension
                bool hasSpecularGlossiness = false;
                
                auto [material, handle] = AssetManager::importAsset<Material>(std::filesystem::path(m_filepath), {materialIdx}, AssetType::Material);
                if (material){
                    entity.getComponent<MaterialComponent>().material = material;
                    entity.getComponent<MaterialComponent>().materialName = material->getName();
                }

                //loadMaterialByIndex(materialIdx);

                /*
                if (materialJSON.contains("extensions") && 
                    materialJSON["extensions"].contains("KHR_materials_pbrSpecularGlossiness")) {
                    hasSpecularGlossiness = true;
                    
                    // Process the material as a specular-glossiness material
                    std::shared_ptr<Material> specGlossMaterial = processSpecularGlossinessMaterial(materialJSON);
                    if (specGlossMaterial) {
                        entity.getComponent<MaterialComponent>().material = specGlossMaterial;
                        entity.getComponent<MaterialComponent>().materialName = specGlossMaterial->getName();
                    } else {
                        // Fallback to PBR if specular-glossiness processing failed
                        auto material = processPBRMaterial(materialJSON);
                        entity.getComponent<MaterialComponent>().material = material;
                        entity.getComponent<MaterialComponent>().materialName = material->getName();
                    }
                } else {
                    // Create a standard PBR material
                    auto material = processPBRMaterial(materialJSON);
                    entity.getComponent<MaterialComponent>().material = material;
                    entity.getComponent<MaterialComponent>().materialName = material->getName();
                }
                */
            }
        }

        // Mark the mesh as loaded
        meshComp.isLoading = false;
        
        // Add the bounding box component if we calculated one
        if (m_calculateBoundingBoxes && localBoundingBox.isValid()) {
            
            BoundingBoxSystem::addBoundingBoxToEntity(entity, localBoundingBox);

        }
    }

    std::shared_ptr<Material> glTF2Loader::processSpecularGlossinessMaterial(json& materialJSON)
    {
        if (!materialJSON.contains("extensions") || 
            !materialJSON["extensions"].contains("KHR_materials_pbrSpecularGlossiness")) {
            GE_CORE_ERROR("Material does not contain KHR_materials_pbrSpecularGlossiness extension");
            return nullptr;
        }

        std::string materialName = materialJSON.value("name", "");

        
        json& specularGlossiness = materialJSON["extensions"]["KHR_materials_pbrSpecularGlossiness"];
        
        // Extract parameters from the specular-glossiness model
        glm::vec3 diffuse(0.5f, 0.5f, 0.5f); // Default diffuse color
        glm::vec3 specular(0.0f, 0.0f, 0.0f); // Default specular color
        float glossiness = 0.0f; // Default glossiness
        
        // Diffuse factor
        if (specularGlossiness.contains("diffuseFactor")) {
            diffuse = glm::vec3(
                specularGlossiness["diffuseFactor"][0],
                specularGlossiness["diffuseFactor"][1],
                specularGlossiness["diffuseFactor"][2]
            );
        }
        
        // Specular factor
        if (specularGlossiness.contains("specularFactor")) {
            specular = glm::vec3(
                specularGlossiness["specularFactor"][0],
                specularGlossiness["specularFactor"][1],
                specularGlossiness["specularFactor"][2]
            );
        }
        
        // Glossiness factor
        if (specularGlossiness.contains("glossinessFactor")) {
            glossiness = specularGlossiness["glossinessFactor"];
        }
        
        // Create a specular-glossiness material
        std::shared_ptr<Material> material = MaterialLibrary::createSpecularGlossinessMaterial(
            materialName,
            diffuse,
            specular,
            glossiness
        );
        
        // Handle textures
        // Diffuse texture
        if (specularGlossiness.contains("diffuseTexture")) {
            int texIndex = specularGlossiness["diffuseTexture"]["index"];
            loadAndSetTexture(material, ParameterID::TEXTURE_DIFFUSE, texIndex);
        }
        
        // Specular-glossiness texture
        if (specularGlossiness.contains("specularGlossinessTexture")) {
            int texIndex = specularGlossiness["specularGlossinessTexture"]["index"];
            loadAndSetTexture(material, ParameterID::TEXTURE_SPECULAR, texIndex);
        }
        
        // Process additional textures common to both workflows
        // Normal map
        if (materialJSON.contains("normalTexture")) {
            int texIndex = materialJSON["normalTexture"]["index"];
            loadAndSetTexture(material, ParameterID::TEXTURE_NORMAL, texIndex);
        }
        
        // Occlusion map
        if (materialJSON.contains("occlusionTexture")) {
            int texIndex = materialJSON["occlusionTexture"]["index"];
            loadAndSetTexture(material, ParameterID::TEXTURE_AO, texIndex);
        }
        
        // Emissive map
        if (materialJSON.contains("emissiveTexture")) {
            int texIndex = materialJSON["emissiveTexture"]["index"];
            loadAndSetTexture(material, ParameterID::TEXTURE_EMISSIVE, texIndex);
        }
        
        // Emissive factor
        if (materialJSON.contains("emissiveFactor")) {
            glm::vec3 emissiveFactor(
                materialJSON["emissiveFactor"][0],
                materialJSON["emissiveFactor"][1],
                materialJSON["emissiveFactor"][2]
            );
            material->setVec3(ParameterID::EMISSION, emissiveFactor);
        }
        
        return material;
    }

    std::shared_ptr<Material> glTF2Loader::processPBRMaterial(json& materialJSON)
    {
        // Get material name if available
        std::string materialName = materialJSON.value("name", "");
        
        // Extract PBR parameters from material JSON
        glm::vec3 baseColor(0.5f, 0.5f, 0.5f); // Default base color
        float metallic = 0.0f;                 // Default metallic
        float roughness = 0.5f;                // Default roughness
        float specular = 0.5f;                 // Default specular
        
        // First check for extensions
        bool hasSpecularGlossiness = false;
        
        if (materialJSON.contains("extensions")) {
            auto& extensions = materialJSON["extensions"];
            
            // Check for specular-glossiness extension, but now we handle it in the primitive processing
            if (extensions.contains("KHR_materials_pbrSpecularGlossiness")) {
                hasSpecularGlossiness = true;
            }
        }
        
        // If no specular-glossiness extension, process standard metallic-roughness
        if (!hasSpecularGlossiness && materialJSON.contains("pbrMetallicRoughness")) {
            json& pbrMetallicRoughness = materialJSON["pbrMetallicRoughness"];
            
            // Base color factor
            if (pbrMetallicRoughness.contains("baseColorFactor")) {
                baseColor = glm::vec3(
                    pbrMetallicRoughness["baseColorFactor"][0],
                    pbrMetallicRoughness["baseColorFactor"][1],
                    pbrMetallicRoughness["baseColorFactor"][2]
                );
            }
            
            // Metallic factor
            if (pbrMetallicRoughness.contains("metallicFactor")) {
                metallic = pbrMetallicRoughness["metallicFactor"];
            }
            
            // Roughness factor
            if (pbrMetallicRoughness.contains("roughnessFactor")) {
                roughness = pbrMetallicRoughness["roughnessFactor"];
            }
        }
        
        // Create a PBR material using the MaterialLibrary
        std::shared_ptr<Material> material = MaterialLibrary::createPBRMaterial(
            materialName.empty() ? "PBRMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(&materialJSON)) : materialName,
            baseColor,
            roughness,
            metallic,
            specular
        );
        
        // If there's pbrMetallicRoughness data, process textures
        if (!hasSpecularGlossiness && materialJSON.contains("pbrMetallicRoughness")) {
            json& pbrMetallicRoughness = materialJSON["pbrMetallicRoughness"];
            
            // Load textures
            // Base color texture
            if (pbrMetallicRoughness.contains("baseColorTexture")) {
                int texIndex = pbrMetallicRoughness["baseColorTexture"]["index"];
                if (GLCapabilities::hasBindlessTextures()) {
                    loadAndSetTexture(material, ParameterID::TEXTURE_ALBEDO_BINDLESS, texIndex);
                } else {
                    loadAndSetTexture(material, ParameterID::TEXTURE_ALBEDO, texIndex);
                }
            }
            
            // Metallic roughness texture
            if (pbrMetallicRoughness.contains("metallicRoughnessTexture")) {
                int texIndex = pbrMetallicRoughness["metallicRoughnessTexture"]["index"];
                
                // In glTF, metallicRoughness is combined: R=unused, G=roughness, B=metallic
                if (GLCapabilities::hasBindlessTextures()) {
                    loadAndSetTexture(material, ParameterID::TEXTURE_METALLIC_BINDLESS, texIndex);
                    loadAndSetTexture(material, ParameterID::TEXTURE_ROUGHNESS_BINDLESS, texIndex);
                } else {
                    loadAndSetTexture(material, ParameterID::TEXTURE_METALLIC, texIndex);
                    loadAndSetTexture(material, ParameterID::TEXTURE_ROUGHNESS, texIndex);
                }
            }
        }
        
        // Normal map - common to both workflows
        if (materialJSON.contains("normalTexture")) {
            int texIndex = materialJSON["normalTexture"]["index"];
            //loadAndSetTexture(material, ParameterID::TEXTURE_NORMAL, texIndex);
            if (GLCapabilities::hasBindlessTextures()) {
                loadAndSetTexture(material, ParameterID::TEXTURE_NORMAL_BINDLESS, texIndex);
            } else {
                loadAndSetTexture(material, ParameterID::TEXTURE_NORMAL, texIndex);
            }
        }
        
        // Occlusion map - common to both workflows
        if (materialJSON.contains("occlusionTexture")) {
            int texIndex = materialJSON["occlusionTexture"]["index"];
            //loadAndSetTexture(material, ParameterID::TEXTURE_AO, texIndex);
            if (GLCapabilities::hasBindlessTextures()) {
                loadAndSetTexture(material, ParameterID::TEXTURE_AO_BINDLESS, texIndex);
            } else {
                loadAndSetTexture(material, ParameterID::TEXTURE_AO, texIndex);
            }
        }
        
        // Emissive map - common to both workflows
        if (materialJSON.contains("emissiveTexture")) {
            int texIndex = materialJSON["emissiveTexture"]["index"];
            //loadAndSetTexture(material, ParameterID::TEXTURE_EMISSIVE, texIndex);
            if (GLCapabilities::hasBindlessTextures()) {
                loadAndSetTexture(material, ParameterID::TEXTURE_EMISSIVE_BINDLESS, texIndex);
            } else {
                loadAndSetTexture(material, ParameterID::TEXTURE_EMISSIVE, texIndex);
            }
        }
        
        // Emissive factor - common to both workflows
        if (materialJSON.contains("emissiveFactor")) {
            glm::vec3 emissiveFactor(
                materialJSON["emissiveFactor"][0],
                materialJSON["emissiveFactor"][1],
                materialJSON["emissiveFactor"][2]
            );
            material->setVec3(ParameterID::EMISSION, emissiveFactor);
        }
        
        return material;
    }

    void glTF2Loader::loadAccessor(json& accessorJSON, std::vector<unsigned char>& dataVec)
    {
        // Clear output vector
        dataVec.clear();
        
        // Validate accessor has necessary fields
        if (!accessorJSON.contains("count") || 
            !accessorJSON.contains("componentType") || 
            !accessorJSON.contains("type")) {
            GE_CORE_ERROR("glTF2Loader: Accessor is missing required fields");
            return;
        }
        
        unsigned int bufferviewInd = accessorJSON.value("bufferView", 0);
        if (bufferviewInd >= m_bufferViews.size()) {
            GE_CORE_ERROR("glTF2Loader: Buffer view index out of range: {}", bufferviewInd);
            return;
        }
        
        unsigned int count = accessorJSON["count"];
        unsigned int componentType = accessorJSON["componentType"];
        size_t accbyteOffset = accessorJSON.value("byteOffset", 0);
        std::string type = accessorJSON["type"];

        json& bufferView = m_bufferViews[bufferviewInd];
        size_t byteOffset = bufferView.value("byteOffset", 0) + accbyteOffset;
        unsigned int byteStride = bufferView.value("byteStride", 0);
        
        // Calculate element size
        unsigned int elementSize = 1; // default for SCALAR
        if (type == "VEC2") elementSize = 2;
        else if (type == "VEC3") elementSize = 3;
        else if (type == "VEC4") elementSize = 4;
        else if (type == "MAT4") elementSize = 16;
        
        unsigned int componentSize = 0;
        switch (componentType)
        {
        case 5120: componentSize = 1; break; // BYTE
        case 5121: componentSize = 1; break; // UNSIGNED_BYTE
        case 5122: componentSize = 2; break; // SHORT
        case 5123: componentSize = 2; break; // UNSIGNED_SHORT
        case 5125: componentSize = 4; break; // UNSIGNED_INT
        case 5126: componentSize = 4; break; // FLOAT
        default:
            GE_CORE_ERROR("glTF2Loader: Unknown component type: {}", componentType);
            return;
        }
        
        // Total bytes for this accessor
        unsigned int totalBytes = count * elementSize * componentSize;
        
        // Pre-allocate the vector to avoid reallocations
        dataVec.reserve(totalBytes);
        dataVec.resize(totalBytes);
        
        // Check if we need to handle interleaved data with stride
        if (byteStride > 0 && byteStride != (elementSize * componentSize)) {
            // Data is interleaved, need to copy with stride
            unsigned int elementBytes = elementSize * componentSize;
            unsigned char* dstPtr = dataVec.data();
            
            for (unsigned int i = 0; i < count; i++) {
                if (byteOffset + i * byteStride + elementBytes > m_binVec.size()) {
                    GE_CORE_ERROR("glTF2Loader: Buffer access out of bounds");
                    dataVec.clear();
                    return;
                }
                
                const unsigned char* srcPtr = m_binVec.data() + byteOffset + i * byteStride;
                std::memcpy(dstPtr, srcPtr, elementBytes);
                dstPtr += elementBytes;
            }
        } else {
            // Data is tightly packed, can copy in one go
            if (byteOffset + totalBytes > m_binVec.size()) {
                GE_CORE_ERROR("glTF2Loader: Buffer access out of bounds: offset={}, size={}, buffer size={}", 
                    byteOffset, totalBytes, m_binVec.size());
                return;
            }
            
            std::memcpy(dataVec.data(), m_binVec.data() + byteOffset, totalBytes);
        }
        
        // Additional validation for TEXCOORD_0
        if (type == "VEC2" && elementSize == 2 && (totalBytes > 0)) {
            // Check if any texture coordinates are outside the [0,1] range 
            // This could indicate potential issues with texture mapping
            bool hasOutOfRange = false;
            if (componentType == 5126) { // FLOAT
                float* coords = reinterpret_cast<float*>(dataVec.data());
                for (unsigned int i = 0; i < count * elementSize; i += elementSize) {
                    float u = coords[i];
                    float v = coords[i + 1];
                    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
                        hasOutOfRange = true;
                        break;
                    }
                }
                
                if (hasOutOfRange) {
                    GE_CORE_WARN("glTF2Loader: Texture coordinates found outside [0,1] range. This may cause texture wrapping issues.");
                }
            }
        }
    }

    void glTF2Loader::cleanUp()
    {
        m_glTFfile.clear();
        m_accessors.clear();
        m_meshes.clear();
        m_bufferViews.clear();
        m_buffers.clear();
        m_nodes.clear();
        m_materials.clear();
        m_animations.clear();
        m_skins.clear();
        m_textures.clear();
        m_images.clear();
        m_samplers.clear();
        
        // Only track deallocation if the vector has data
        if (!m_binVec.empty()) {
            RAPTURE_PROFILE_FREE(m_binVec.data()); // Track deallocation
        }
        m_binVec.clear();

        // scuffed way to force the cache cleanup to clean this loader, otherwise there is a case where the destructor was called before the model was loaded (or never started to load)
        // could be because some error or wathever. 
        //dont know what will happen when this entire loader gets multithreaded. could possibly cause some bugs?
        m_isLoaded = true;
        // Clean up the cache when a loader is cleaned up
        ModelLoadersCache::cleanup();

        m_isInitialized = false;
        m_isLoaded = false;
        
    }

    void glTF2Loader::reportProgress(float progress)
    {
        GE_CORE_INFO("glTF2Loader: Progress: {}", progress);
        //m_progressCallback(progress);
    }

    bool glTF2Loader::loadAndSetTexture(std::shared_ptr<Material> material, ParameterID paramID, int textureIndex)
    {
        if (textureIndex < 0 || textureIndex >= m_textures.size()) {
            GE_CORE_ERROR("glTF2Loader: Invalid texture index {}", textureIndex);
            return false;
        }

        json& texture = m_textures[textureIndex];
        
        // Get the image index
        if (!texture.contains("source")) {
            GE_CORE_ERROR("glTF2Loader: Texture missing source property");
            return false;
        }
        
        int imageIndex = texture["source"];
        if (imageIndex < 0 || imageIndex >= m_images.size()) {
            GE_CORE_ERROR("glTF2Loader: Invalid image index {}", imageIndex);
            return false;
        }
        
        json& image = m_images[imageIndex];
        
        // Get the image URI
        if (!image.contains("uri")) {
            GE_CORE_ERROR("glTF2Loader: Image missing URI");
            return false;
        }
        
        std::string imageURI = image["uri"];
        
        // Load the texture
        std::string texturePath = m_basePath + imageURI;
        std::filesystem::path texturePathFS = std::filesystem::path(texturePath);

        //std::shared_ptr<Texture2D> tex = TextureLibrary::loadAsync(texturePath);
        uint32_t clampedIndex = std::max(textureIndex, 0);

        auto [tex, handle] = AssetManager::importAsset<Texture2D>(texturePathFS, {clampedIndex});

        if (!tex || !handle) {
            GE_CORE_ERROR("glTF2Loader::loadAndSetTexture - Failed to import or get texture {}", texturePath);
            return false;
        }

        // Apply sampler parameters if present
        if (texture.contains("sampler")) {
            int samplerIndex = texture["sampler"];
            if (samplerIndex >= 0 && samplerIndex < m_samplers.size()) {
                json& sampler = m_samplers[samplerIndex];
                
                // Apply filter settings
                if (sampler.contains("magFilter")) {
                    int magFilter = sampler["magFilter"];
                    if (magFilter == 9728) { // GL_NEAREST
                        tex->setMagFilter(TextureFilter::Nearest);
                    } else if (magFilter == 9729) { // GL_LINEAR
                        tex->setMagFilter(TextureFilter::Linear);
                    }
                }
                
                if (sampler.contains("minFilter")) {
                    int minFilter = sampler["minFilter"];
                    if (minFilter == 9728) { // GL_NEAREST
                        tex->setMinFilter(TextureFilter::Nearest);
                    } else if (minFilter == 9729) { // GL_LINEAR
                        tex->setMinFilter(TextureFilter::Linear);
                    } else if (minFilter == 9984) { // GL_NEAREST_MIPMAP_NEAREST
                        tex->setMinFilter(TextureFilter::NearestMipmapNearest);
                    } else if (minFilter == 9985) { // GL_LINEAR_MIPMAP_NEAREST
                        tex->setMinFilter(TextureFilter::LinearMipmapNearest);
                    } else if (minFilter == 9986) { // GL_NEAREST_MIPMAP_LINEAR
                        tex->setMinFilter(TextureFilter::NearestMipmapLinear);
                    } else if (minFilter == 9987) { // GL_LINEAR_MIPMAP_LINEAR
                        tex->setMinFilter(TextureFilter::LinearMipmapLinear);
                    }
                }
                
                if (sampler.contains("wrapS")) {
                    int wrapS = sampler["wrapS"];
                    if (wrapS == 33071) { // GL_CLAMP_TO_EDGE
                        tex->setWrapS(TextureWrap::ClampToEdge);
                    } else if (wrapS == 33648) { // GL_MIRRORED_REPEAT
                        tex->setWrapS(TextureWrap::MirroredRepeat);
                    } else if (wrapS == 10497) { // GL_REPEAT
                        tex->setWrapS(TextureWrap::Repeat);
                    }
                }
                
                if (sampler.contains("wrapT")) {
                    int wrapT = sampler["wrapT"];
                    if (wrapT == 33071) { // GL_CLAMP_TO_EDGE
                        tex->setWrapT(TextureWrap::ClampToEdge);
                    } else if (wrapT == 33648) { // GL_MIRRORED_REPEAT
                        tex->setWrapT(TextureWrap::MirroredRepeat);
                    } else if (wrapT == 10497) { // GL_REPEAT
                        tex->setWrapT(TextureWrap::Repeat);
                    }
                }
            } else {
                // Set default texture parameters if no sampler is specified
                tex->setMinFilter(TextureFilter::LinearMipmapLinear);
                tex->setMagFilter(TextureFilter::Linear);
                tex->setWrapS(TextureWrap::Repeat);
                tex->setWrapT(TextureWrap::Repeat);
            }
            


            // Set the texture on the material using the enum ID
            if (GLCapabilities::hasBindlessTextures()) {
                tex->makeResident();
                material->setTextureBindless(paramID, tex, handle);
            } else {
                material->setTexture(paramID, tex, handle);
            }

            
            return true;
        }
        
        return false;
    }


    // Animation loading methods
    std::vector<std::shared_ptr<Animation>> glTF2Loader::loadAnimations(const std::string& filepath, bool isAbsolute) {
        // Reset state to ensure clean loading
        cleanUp();
        
        // Report initial progress
        reportProgress(0.0f);
        
        std::string fullPath = isAbsolute ? filepath : DIRNAME + filepath;

        // Load the gltf file
        std::ifstream gltf_file(fullPath);
        if (!gltf_file) {
            GE_CORE_ERROR("glTF2Loader: Couldn't load glTF file '{}'", fullPath);
            return {};
        }

        // Parse the JSON file
        try {
            gltf_file >> m_glTFfile;
        }
        catch (const std::exception& e) {
            GE_CORE_ERROR("glTF2Loader: Failed to parse glTF JSON: {}", e.what());
            return {};
        }
        gltf_file.close();

        // Load references to major sections
        m_accessors = m_glTFfile.value("accessors", json::array());
        m_bufferViews = m_glTFfile.value("bufferViews", json::array());
        m_buffers = m_glTFfile.value("buffers", json::array());
        m_nodes = m_glTFfile.value("nodes", json::array());
        m_animations = m_glTFfile.value("animations", json::array());

        // Check if there are any animations
        if (m_animations.empty()) {
            GE_CORE_WARN("glTF2Loader: No animations found in '{}'", fullPath);
            return {};
        }

        // Extract the directory path from the filepath
        m_basePath = "";
        size_t lastSlashPos = fullPath.find_last_of("/\\");
        if (lastSlashPos != std::string::npos) {
            m_basePath = fullPath.substr(0, lastSlashPos + 1);
        }

        // Load the bin file with all the buffer data
        std::string bufferURI = m_buffers[0].value("uri", "");
        if (bufferURI.empty()) {
            GE_CORE_ERROR("glTF2Loader: Buffer URI is missing");
            return {};
        }
        
        // Check if the buffer URI is a relative path
        if (bufferURI.find("://") == std::string::npos && !bufferURI.empty()) {
            // Combine the directory path with the buffer URI
            bufferURI = m_basePath + bufferURI;
        }
        std::ifstream binary_file(bufferURI, std::ios::binary);
        if (!binary_file) {
            GE_CORE_ERROR("glTF2Loader: Couldn't load binary file '{}'", bufferURI);
            return {};
        }
        
        // Get file size and reserve space
        binary_file.seekg(0, std::ios::end);
        size_t fileSize = binary_file.tellg();
        binary_file.seekg(0, std::ios::beg);
        
        m_binVec.resize(fileSize);
        
        // Read the entire file at once for efficiency
        if (!binary_file.read(reinterpret_cast<char*>(m_binVec.data()), fileSize)) {
            GE_CORE_ERROR("glTF2Loader: Failed to read binary data");
            return {};
        }
        
        binary_file.close();

        // Process animations
        std::vector<std::shared_ptr<Animation>> animations = processAnimations(m_animations);
        
        // Clean up
        cleanUp();
        
        return animations;
    }

    std::shared_ptr<Material> glTF2Loader::loadMaterialByIndex(size_t materialIndex)
    {
        if (materialIndex >= m_materials.size()) {
            GE_CORE_ERROR("glTF2Loader: Material index out of range");
            return nullptr;
        }

        json& materialJSON = m_materials[materialIndex];
        if (materialJSON.contains("pbrMetallicRoughness")) {
            // Process material properties
            std::shared_ptr<Material> material = processPBRMaterial(materialJSON);

            return material;
        } else if (materialJSON.contains("extensions")) {
            // Process material properties
            json& extensionsJSON = materialJSON["extensions"];
            if (extensionsJSON.contains("KHR_materials_pbrSpecularGlossiness")) {
                std::shared_ptr<Material> material = processPBRMaterial(extensionsJSON);

                return material;
            }
        } else {
            
            auto material = MaterialLibrary::createSolidMaterial(materialJSON.value("name", "DefaultMaterial"), glm::vec3(0.5f, 0.5f, 0.5f));
            if (material) {
                return material;
            }
        }

        return nullptr;
    }

    std::vector<std::shared_ptr<Animation>> glTF2Loader::processAnimations(json& animationsJSON) {
        std::vector<std::shared_ptr<Animation>> animations;
        
        for (auto& animationJSON : animationsJSON) {
            std::shared_ptr<Animation> animation = processAnimation(animationJSON);
            if (animation) {
                animations.push_back(animation);
            }
        }
        
        GE_CORE_INFO("glTF2Loader: Processed {} animations", animations.size());
        return animations;
    }

    std::shared_ptr<Animation> glTF2Loader::processAnimation(json& animationJSON) {
        // Get animation name and ensure it's unique
        std::string name = animationJSON.value("name", "Animation");
        if (name.empty()) {
            name = "Animation_" + std::to_string(reinterpret_cast<uintptr_t>(&animationJSON));
        }
        
        // Check required elements
        if (!animationJSON.contains("channels") || !animationJSON.contains("samplers")) {
            GE_CORE_ERROR("glTF2Loader: Animation missing required channels or samplers");
            return nullptr;
        }
        
        json& channelsJSON = animationJSON["channels"];
        json& samplersJSON = animationJSON["samplers"];
        
        if (channelsJSON.empty() || samplersJSON.empty()) {
            GE_CORE_ERROR("glTF2Loader: Animation has empty channels or samplers");
            return nullptr;
        }
        
        // Find animation duration by examining all keyframes
        float duration = 0.0f;
        
        for (auto& samplerJSON : samplersJSON) {
            if (!samplerJSON.contains("input")) {
                continue;
            }
            
            unsigned int inputAccessorIdx = samplerJSON["input"];
            if (inputAccessorIdx >= m_accessors.size()) {
                continue;
            }
            
            json& inputAccessor = m_accessors[inputAccessorIdx];
            if (!inputAccessor.contains("max")) {
                continue;
            }
            
            float maxTime = inputAccessor["max"][0];
            duration = std::max(duration, maxTime);
        }
        
        if (duration <= 0.0f) {
            GE_CORE_ERROR("glTF2Loader: Animation has invalid duration");
            return nullptr;
        }
        
        // Create animation
        std::shared_ptr<Animation> animation = std::make_shared<Animation>(name, duration);
        
        // Process channels and samplers
        processAnimationChannelsAndSamplers(animation, channelsJSON, samplersJSON);
        
        GE_CORE_INFO("glTF2Loader: Processed animation '{}' with duration {}s", name, duration);
        return animation;
    }

    void glTF2Loader::processAnimationChannelsAndSamplers(
        std::shared_ptr<Animation> animation, 
        json& channelsJSON, 
        json& samplersJSON) {
        
        // First, process all samplers
        std::vector<std::shared_ptr<AnimationSampler>> samplers;
        samplers.reserve(samplersJSON.size());
        
        for (auto& samplerJSON : samplersJSON) {
            // Get interpolation
            std::string interpolationStr = samplerJSON.value("interpolation", "LINEAR");
            InterpolationType interpolationType = getInterpolationType(interpolationStr);
            
            // Create sampler
            std::shared_ptr<AnimationSampler> sampler = std::make_shared<AnimationSampler>(interpolationType);
            
            // Get input accessor (keyframe times)
            if (!samplerJSON.contains("input")) {
                GE_CORE_ERROR("glTF2Loader: Sampler missing input accessor");
                samplers.push_back(nullptr);
                continue;
            }
            
            unsigned int inputAccessorIdx = samplerJSON["input"];
            if (inputAccessorIdx >= m_accessors.size()) {
                GE_CORE_ERROR("glTF2Loader: Sampler input accessor index out of range");
                samplers.push_back(nullptr);
                continue;
            }
            
            std::vector<unsigned char> timeData;
            loadAccessor(m_accessors[inputAccessorIdx], timeData);
            
            // Get output accessor (keyframe values)
            if (!samplerJSON.contains("output")) {
                GE_CORE_ERROR("glTF2Loader: Sampler missing output accessor");
                samplers.push_back(nullptr);
                continue;
            }
            
            unsigned int outputAccessorIdx = samplerJSON["output"];
            if (outputAccessorIdx >= m_accessors.size()) {
                GE_CORE_ERROR("glTF2Loader: Sampler output accessor index out of range");
                samplers.push_back(nullptr);
                continue;
            }
            
            json& outputAccessor = m_accessors[outputAccessorIdx];
            std::string outputType = outputAccessor.value("type", "");
            
            std::vector<unsigned char> valueData;
            loadAccessor(outputAccessor, valueData);
            
            // Number of keyframes
            unsigned int keyframeCount = timeData.size() / sizeof(float);
            
            // Parse keyframe data based on output type
            if (outputType == "VEC3") {
                // Position or scale keyframes
                for (unsigned int i = 0; i < keyframeCount; ++i) {
                    float time = reinterpret_cast<float*>(timeData.data())[i];
                    glm::vec3 value = *reinterpret_cast<glm::vec3*>(valueData.data() + i * sizeof(glm::vec3));
                    
                    // Determine if this is position or scale based on usage
                    // Will be determined when processing channels
                    sampler->addPositionKeyframe(time, value);
                    sampler->addScaleKeyframe(time, value);
                }
            } else if (outputType == "VEC4") {
                // Rotation keyframes (quaternions)
                for (unsigned int i = 0; i < keyframeCount; ++i) {
                    float time = reinterpret_cast<float*>(timeData.data())[i];

                    glm::vec4 rawQuat = *reinterpret_cast<glm::vec4*>(valueData.data() + i * sizeof(glm::vec4));
                    
                    // GLTF quaternions are stored as [x, y, z, w]
                    glm::quat rotation(rawQuat.w, rawQuat.x, rawQuat.y, rawQuat.z);
                    sampler->addRotationKeyframe(time, rotation);
                }
            }
            
            samplers.push_back(sampler);
        }
        
        // Process animation channels
        for (auto& channelJSON : channelsJSON) {
            // Check required fields
            if (!channelJSON.contains("sampler") || !channelJSON.contains("target")) {
                GE_CORE_ERROR("glTF2Loader: Channel missing required sampler or target");
                continue;
            }
            
            // Get sampler index
            unsigned int samplerIdx = channelJSON["sampler"];
            if (samplerIdx >= samplers.size() || !samplers[samplerIdx]) {
                GE_CORE_ERROR("glTF2Loader: Channel references invalid sampler");
                continue;
            }
            
            // Get target node and path
            json& targetJSON = channelJSON["target"];
            if (!targetJSON.contains("node") || !targetJSON.contains("path")) {
                GE_CORE_ERROR("glTF2Loader: Channel target missing node or path");
                continue;
            }
            
            unsigned int nodeIdx = targetJSON["node"];
            std::string path = targetJSON["path"];
            

            // Get node name
            //std::string nodeName = getNodeName(nodeIdx);
            std::string nodeName = std::to_string(nodeIdx);

            // Create animation channel
            std::shared_ptr<AnimationChannel> channel = std::make_shared<AnimationChannel>(nodeName);
            
            // Assign sampler based on path
            if (path == "translation") {
                channel->setPositionSampler(samplers[samplerIdx]);
            } else if (path == "rotation") {
                channel->setRotationSampler(samplers[samplerIdx]);
            } else if (path == "scale") {
                channel->setScaleSampler(samplers[samplerIdx]);
            } else {
                GE_CORE_WARN("glTF2Loader: Unsupported animation path: {}", path);
                continue;
            }
            
            // Add channel to animation
            animation->addChannel(channel);
        }
    }

    std::string glTF2Loader::getNodeName(unsigned int nodeIndex) {
        if (nodeIndex >= m_nodes.size()) {
            return std::to_string(nodeIndex);
        }
        
        json& nodeJSON = m_nodes[nodeIndex];
        std::string nodeName = nodeJSON.value("name", "");
        
        if (nodeName.empty()) {
            nodeName = std::to_string(nodeIndex);
        }
        
        return nodeName;
    }

    InterpolationType glTF2Loader::getInterpolationType(const std::string& interpolation) {
        if (interpolation == "STEP") {
            return InterpolationType::STEP;
        } else if (interpolation == "CUBICSPLINE") {
            return InterpolationType::CUBICSPLINE;
        } else {
            // Default to LINEAR
            return InterpolationType::LINEAR;
        }
    }

    glTFMetadata glTF2Loader::getFileMetadata(const std::string& filepath, bool isAbsolute) {
        glTFMetadata metadata;
        
        // Construct full path
        std::string fullPath = isAbsolute ? filepath : DIRNAME + filepath;
        
        // Open and read the glTF file
        std::ifstream gltf_file(fullPath);
        if (!gltf_file) {
            GE_CORE_ERROR("glTF2Loader: Couldn't load glTF file '{}'", fullPath);
            return metadata;
        }
        
        try {
            // Parse JSON but only read what we need
            json glTFfile;
            gltf_file >> glTFfile;
            gltf_file.close();
            
            // Get version and generator info
            metadata.version = glTFfile.value("asset", json::object()).value("version", "unknown");
            metadata.generator = glTFfile.value("asset", json::object()).value("generator", "unknown");
            
            // Count materials
            if (glTFfile.contains("materials")) {
                metadata.materialCount = glTFfile["materials"].size();
            }
            
            // Count animations
            if (glTFfile.contains("animations")) {
                metadata.animationCount = glTFfile["animations"].size();
            }
            
            // Count nodes
            if (glTFfile.contains("nodes")) {
                metadata.nodeCount = glTFfile["nodes"].size();
            }
            
            // Count textures
            if (glTFfile.contains("textures")) {
                metadata.textureCount = glTFfile["textures"].size();
            }
            
            // Count meshes and primitives
            if (glTFfile.contains("meshes")) {
                metadata.meshCount = glTFfile["meshes"].size();
                
                // Count total primitives across all meshes
                for (const auto& mesh : glTFfile["meshes"]) {
                    if (mesh.contains("primitives")) {
                        metadata.primitiveCount += mesh["primitives"].size();
                    }
                }
            }
            
            // Check for skeletons
            if (glTFfile.contains("skins")) {
                metadata.hasSkeletons = !glTFfile["skins"].empty();
            }
            
        } catch (const std::exception& e) {
            GE_CORE_ERROR("glTF2Loader: Failed to parse glTF JSON: {}", e.what());
            return metadata;
        }
        
        return metadata;
    }

}