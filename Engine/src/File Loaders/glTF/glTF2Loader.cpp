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

#define DIRNAME "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/build/bin/Debug/assets/models/"

namespace Rapture {

    glTF2Loader::glTF2Loader(std::shared_ptr<Scene> scene)
        : m_scene(scene)
    {
        if (!m_scene) {
            GE_CORE_ERROR("glTF2Loader: Scene pointer is null");
        }
    }

    glTF2Loader::~glTF2Loader()
    {
        cleanUp();
    }

    bool glTF2Loader::loadModel(const std::string& filepath)
    {
        // Reset state to ensure clean loading
        cleanUp();
        
        // Load the gltf file
        std::ifstream gltf_file(DIRNAME + filepath);
        if (!gltf_file)
        {
            GE_CORE_ERROR("glTF2Loader: Couldn't load glTF file '{}'", filepath);
            return false;
        }

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
        std::ifstream binary_file(DIRNAME + bufferURI, std::ios::binary);
        if (!binary_file)
        {
            GE_CORE_ERROR("glTF2Loader: Couldn't load binary file '{}'", DIRNAME + bufferURI);
            return false;
        }
        
        // Get file size and reserve space
        binary_file.seekg(0, std::ios::end);
        size_t fileSize = binary_file.tellg();
        binary_file.seekg(0, std::ios::beg);
        
        m_binVec.resize(fileSize);
        
        // Read the entire file at once for efficiency
        if (!binary_file.read(reinterpret_cast<char*>(m_binVec.data()), fileSize)) {
            GE_CORE_ERROR("glTF2Loader: Failed to read binary data");
            return false;
        }
        
        binary_file.close();

        // Create a root entity for the model
        Entity rootEntity = m_scene->createEntity("glTF_Model");
        
        // Process the default scene or the first scene if default not specified
        int defaultScene = m_glTFfile.value("scene", 0);
        if (m_glTFfile.contains("scenes") && !m_glTFfile["scenes"].empty()) {
            processScene(m_glTFfile["scenes"][defaultScene]);
        }
        else if (!m_nodes.empty()) {
            // If no scenes but has nodes, process the first node as root
            Entity nodeEntity = m_scene->createEntity("Root Node");
            processNode(nodeEntity, m_nodes[0]);
        }
        
        // Clean up
        cleanUp();
        
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
                    Entity nodeEntity = m_scene->createEntity("Root Node");
                    processNode(nodeEntity, m_nodes[nodeIndex]);
                }
            }
        }
    }

    Entity glTF2Loader::processNode(Entity nodeEntity, json& nodeJSON)
    {

        if (!nodeEntity.hasComponent<EntityNodeComponent>()) {
            nodeEntity.addComponent<EntityNodeComponent>(nodeEntity);
        }

        // Create a new entity for this node
        std::string nodeName = nodeJSON.value("name", "Node");
        //Entity nodeEntity = m_scene->createEntity(nodeName);

        // Update the tag
        nodeEntity.getComponent<TagComponent>().tag = nodeName;
        auto& nodeEntityComp = nodeEntity.getComponent<EntityNodeComponent>();
        

        nodeEntity.addComponent<TransformComponent>();
        auto& transformComp = nodeEntity.getComponent<TransformComponent>();

        // Extract transform components if present
        if (nodeJSON.contains("matrix")) {
            // Use matrix directly
            float matrixValues[16];
            for (int i = 0; i < 16; i++) {
                matrixValues[i] = nodeJSON["matrix"][i];
            }
            glm::mat4 nodeMatrix = glm::make_mat4(matrixValues);
            std::shared_ptr<EntityNode> parent = nodeEntityComp.entity_node->getParent();
            if (parent != nullptr) {
                nodeMatrix = parent->getEntity()->getComponent<TransformComponent>().transformMatrix() * nodeMatrix;
            }

            transformComp.transforms.setTransform(nodeMatrix);
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
            glm::mat4 transformMatrix = glm::mat4(1.0f);
            transformMatrix = glm::translate(transformMatrix, translation);
            transformMatrix = transformMatrix * glm::mat4_cast(rotation);
            transformMatrix = glm::scale(transformMatrix, scale);
            
            
            std::shared_ptr<EntityNode> parent = nodeEntityComp.entity_node->getParent();
            if (parent != nullptr) {
                transformMatrix = parent->getEntity()->getComponent<TransformComponent>().transformMatrix() * transformMatrix;
            }



            // Add the component with the full transform matrix
            transformComp.transforms.setTransform(transformMatrix);
            
        }
        
        // If this node has a mesh, process it
        if (nodeJSON.contains("mesh")) {
            unsigned int meshIndex = nodeJSON["mesh"];
            if (meshIndex < m_meshes.size()) {
                processMesh(nodeEntity, m_meshes[meshIndex]);
            }
        }
        
        // Process children
        if (nodeJSON.contains("children")) {
            for (auto& childIdx : nodeJSON["children"]) {
                unsigned int childIndex = childIdx.get<unsigned int>();
                if (childIndex < m_nodes.size()) {
                    Entity childEntity = m_scene->createEntity("Child Node");
                    childEntity.addComponent<EntityNodeComponent>(childEntity, nodeEntity.getComponent<EntityNodeComponent>().entity_node);

                    nodeEntity.getComponent<EntityNodeComponent>().entity_node->addChild(childEntity.getComponent<EntityNodeComponent>().entity_node);

                    processNode(childEntity, m_nodes[childIndex]);

                    // Establish parent-child relationship
                    if (!childEntity.hasComponent<EntityNodeComponent>()) {
                        GE_CORE_ERROR("Child entity '{}' missing EntityNodeComponent", childIndex);
                        continue;
                    }
                    
                    if (!nodeEntity.hasComponent<EntityNodeComponent>()) {
                        GE_CORE_ERROR("Node entity '{}' missing EntityNodeComponent", nodeName);
                        continue;
                    }
                    

                }
            }
        }
        
        return nodeEntity;
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
        // Create a new vertex array for this primitive
        auto vao = std::make_shared<VertexArray>();
        entity.addComponent<MaterialComponent>();
        
        // Check if entity has MeshComponent before accessing it
        if (!entity.hasComponent<MeshComponent>()) {
            GE_CORE_ERROR("Entity missing MeshComponent");
            return;
        }
        entity.getComponent<MeshComponent>().mesh->SetVAO(vao);

        BufferLayout bufferLayout;
        
        // First gather all attribute data to calculate total buffer size
        std::vector<std::pair<std::string, std::vector<unsigned char>>> attributeData;
        size_t totalVertexDataSize = 0;
        size_t currentOffset = 0;

        // Process vertex attributes
        if (primitive.contains("attributes")) {
            json& attribs = primitive["attributes"];
            
            // Set up buffer layout based on attributes and gather data
            for (auto& attrib : attribs.items()) {
                std::string name = attrib.key();
                if (name == "COLOR_0") continue; // Skip color data for now
                
                unsigned int accessorIdx = attrib.value();
                json& accessor = m_accessors[accessorIdx];
                
                unsigned int componentType = accessor["componentType"];
                std::string type = accessor["type"];
                bufferLayout.buffer_attribs.push_back({name, componentType, type, currentOffset});


                // Load attribute data
                std::vector<unsigned char> attrData;
                loadAccessor(m_accessors[accessorIdx], attrData);
                
                currentOffset += attrData.size();

                if (!attrData.empty()) {
                    attributeData.push_back({name, attrData});
                    totalVertexDataSize += attrData.size();
                }
            }
            //bufferLayout.print_buffer_layout();
        }

 
        // Create vertex buffer with the correct size
        vao->setVertexBuffer(std::make_shared<VertexBuffer>(totalVertexDataSize, BufferUsage::Static));
        auto vertexBuffer = vao->getVertexBuffer();
        
        // Set buffer layout after vertex buffer is created
        vao->setBufferLayout(bufferLayout);
        
        // Add attribute data to the buffer
        currentOffset = 0;
        for (auto& [name, data] : attributeData) {
            vertexBuffer->setData(data.data(), data.size(), currentOffset);
            currentOffset += data.size();
        }
        
        // Process indices if present
        if (primitive.contains("indices")) {
            unsigned int indicesIdx = primitive["indices"];
            std::vector<unsigned char> indexData;
            
            // Load index data
            loadAccessor(m_accessors[indicesIdx], indexData);
            
            if (!indexData.empty()) {
                // Get index component type
                unsigned int compType = m_accessors[indicesIdx]["componentType"];
                unsigned int indCount = m_accessors[indicesIdx]["count"];
                
                // Create and set index buffer
                auto indexBuffer = std::make_shared<IndexBuffer>(indexData.size(), compType, BufferUsage::Static);
                indexBuffer->setData(indexData.data(), indexData.size());
                vao->setIndexBuffer(indexBuffer);
                
                // Set up draw parameters
                entity.getComponent<MeshComponent>().mesh->setIndexCount(indCount);
            }
        }
        
        // Set material if present
        if (primitive.contains("material")) {
            unsigned int materialIdx = primitive["material"];
            if (materialIdx < m_materials.size()) {
                // Check if entity has MaterialComponent
                if (!entity.hasComponent<MaterialComponent>()) {
                    GE_CORE_WARN("Entity missing MaterialComponent for material index {}", materialIdx);
                }

                json& materialJSON = m_materials[materialIdx];
                
                // Check if this material uses the KHR_materials_pbrSpecularGlossiness extension
                bool hasSpecularGlossiness = false;
                
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
                        auto pbr = std::make_shared<PBRMaterial>();
                        entity.getComponent<MaterialComponent>().material = pbr;
                        entity.getComponent<MaterialComponent>().materialName = pbr->getName();
                        processPBRMaterial(pbr, materialJSON);
                    }
                } else {
                    // Create a standard PBR material
                    auto pbr = std::make_shared<PBRMaterial>();
                    entity.getComponent<MaterialComponent>().material = pbr;
                    processPBRMaterial(pbr, materialJSON);
                }
            }
        }
    }

    void glTF2Loader::processPBRMaterial(std::shared_ptr<PBRMaterial> material, json& materialJSON)
    {
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
                glm::vec3 baseColor(
                    pbrMetallicRoughness["baseColorFactor"][0],
                    pbrMetallicRoughness["baseColorFactor"][1],
                    pbrMetallicRoughness["baseColorFactor"][2]
                );
                material->setVec3("baseColor", baseColor);
            }
            
            // Metallic factor
            if (pbrMetallicRoughness.contains("metallicFactor")) {
                float metallic = pbrMetallicRoughness["metallicFactor"];
                material->setFloat("metallic", metallic);
            }
            
            // Roughness factor
            if (pbrMetallicRoughness.contains("roughnessFactor")) {
                float roughness = pbrMetallicRoughness["roughnessFactor"];
                material->setFloat("roughness", roughness);
            }
            
            // Load textures
            // Base color texture
            if (pbrMetallicRoughness.contains("baseColorTexture")) {
                int texIndex = pbrMetallicRoughness["baseColorTexture"]["index"];
                if (loadAndSetTexture(material, "albedoMap", texIndex)) {
                    material->setBool("u_HasAlbedoMap", true);
                }
            }
            
            // Metallic roughness texture
            if (pbrMetallicRoughness.contains("metallicRoughnessTexture")) {
                int texIndex = pbrMetallicRoughness["metallicRoughnessTexture"]["index"];
                
                // In glTF, metallicRoughness is combined: R=unused, G=roughness, B=metallic
                if (loadAndSetTexture(material, "metallicMap", texIndex)) {
                    material->setBool("u_HasMetallicMap", true);
                }
                
                if (loadAndSetTexture(material, "roughnessMap", texIndex)) {
                    material->setBool("u_HasRoughnessMap", true);
                }
            }
        }
        
        // Normal map - common to both workflows
        if (materialJSON.contains("normalTexture")) {
            int texIndex = materialJSON["normalTexture"]["index"];
            if (loadAndSetTexture(material, "normalMap", texIndex)) {
                material->setBool("u_HasNormalMap", true);
            }
        }
        
        // Occlusion map - common to both workflows
        if (materialJSON.contains("occlusionTexture")) {
            int texIndex = materialJSON["occlusionTexture"]["index"];
            if (loadAndSetTexture(material, "aoMap", texIndex)) {
                material->setBool("u_HasAOMap", true);
            }
        }
        
        // Emissive map - common to both workflows
        if (materialJSON.contains("emissiveTexture")) {
            int texIndex = materialJSON["emissiveTexture"]["index"];
            if (loadAndSetTexture(material, "emissiveMap", texIndex)) {
                material->setBool("u_HasEmissiveMap", true);
            }
        }
        
        // Emissive factor - common to both workflows
        if (materialJSON.contains("emissiveFactor")) {
            glm::vec3 emissiveFactor(
                materialJSON["emissiveFactor"][0],
                materialJSON["emissiveFactor"][1],
                materialJSON["emissiveFactor"][2]
            );
            material->setVec3("emissiveFactor", emissiveFactor);
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
            if (loadAndSetTexture(material, "diffuseMap", texIndex)) {
                material->setBool("u_HasDiffuseMap", true);
            }
        }
        
        // Specular-glossiness texture
        if (specularGlossiness.contains("specularGlossinessTexture")) {
            int texIndex = specularGlossiness["specularGlossinessTexture"]["index"];
            if (loadAndSetTexture(material, "specularGlossinessMap", texIndex)) {
                material->setBool("u_HasSpecularGlossinessMap", true);
            }
        }
        
        // Process additional textures common to both workflows
        // Normal map
        if (materialJSON.contains("normalTexture")) {
            int texIndex = materialJSON["normalTexture"]["index"];
            if (loadAndSetTexture(material, "normalMap", texIndex)) {
                material->setBool("u_HasNormalMap", true);
            }
        }
        
        // Occlusion map
        if (materialJSON.contains("occlusionTexture")) {
            int texIndex = materialJSON["occlusionTexture"]["index"];
            if (loadAndSetTexture(material, "aoMap", texIndex)) {
                material->setBool("u_HasAOMap", true);
            }
        }
        
        // Emissive map
        if (materialJSON.contains("emissiveTexture")) {
            int texIndex = materialJSON["emissiveTexture"]["index"];
            if (loadAndSetTexture(material, "emissiveMap", texIndex)) {
                material->setBool("u_HasEmissiveMap", true);
            }
        }
        
        // Emissive factor
        if (materialJSON.contains("emissiveFactor")) {
            glm::vec3 emissiveFactor(
                materialJSON["emissiveFactor"][0],
                materialJSON["emissiveFactor"][1],
                materialJSON["emissiveFactor"][2]
            );
            material->setVec3("emissiveFactor", emissiveFactor);
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
        
        
        // Check if we need to handle interleaved data with stride
        if (byteStride > 0 && byteStride != (elementSize * componentSize)) {
            // Data is interleaved, need to copy with stride
            dataVec.resize(totalBytes);
            
            unsigned int elementBytes = elementSize * componentSize;
            unsigned int dstOffset = 0;
            
            for (unsigned int i = 0; i < count; i++) {
                if (byteOffset + i * byteStride + elementBytes > m_binVec.size()) {
                    GE_CORE_ERROR("glTF2Loader: Buffer access out of bounds");
                    dataVec.clear();
                    return;
                }
                
                std::memcpy(
                    dataVec.data() + dstOffset,
                    m_binVec.data() + byteOffset + i * byteStride,
                    elementBytes
                );
                dstOffset += elementBytes;
            }
        } else {
            // Data is tightly packed, can copy in one go
            if (byteOffset + totalBytes > m_binVec.size()) {
                GE_CORE_ERROR("glTF2Loader: Buffer access out of bounds: offset={}, size={}, buffer size={}", 
                    byteOffset, totalBytes, m_binVec.size());
                return;
            }
            
            dataVec.resize(totalBytes);
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
        
        m_binVec.clear();
    }

    bool glTF2Loader::loadAndSetTexture(std::shared_ptr<Material> material, const std::string& textureName, int textureIndex)
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
        std::string texturePath = DIRNAME + m_basePath + imageURI;
        
        std::shared_ptr<Texture2D> tex = TextureLibrary::load(texturePath, textureName);
        
        if (!tex) {
            GE_CORE_ERROR("glTF2Loader: Failed to load texture {}", texturePath);
            return false;
        }
        
        // Apply sampler parameters if present
        if (texture.contains("sampler")) {
            int samplerIndex = texture["sampler"];
            if (samplerIndex >= 0 && samplerIndex < m_samplers.size()) {
                json& sampler = m_samplers[samplerIndex];
                
                // The glTF spec defines these constants:
                // GL_NEAREST = 9728
                // GL_LINEAR = 9729
                // GL_NEAREST_MIPMAP_NEAREST = 9984
                // GL_LINEAR_MIPMAP_NEAREST = 9985
                // GL_NEAREST_MIPMAP_LINEAR = 9986
                // GL_LINEAR_MIPMAP_LINEAR = 9987
                // GL_CLAMP_TO_EDGE = 33071
                // GL_MIRRORED_REPEAT = 33648
                // GL_REPEAT = 10497
                
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
                
                // Apply wrap settings
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
            }
        } else {
            // Set default texture parameters if no sampler is specified
            tex->setMinFilter(TextureFilter::LinearMipmapLinear);
            tex->setMagFilter(TextureFilter::Linear);
            tex->setWrapS(TextureWrap::Repeat);
            tex->setWrapT(TextureWrap::Repeat);
        }
        
        // Set the texture on the material
        material->setTexture(textureName, tex);
        
        return true;
    }
}

