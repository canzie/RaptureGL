/*
#include "glTF2Loader.h"

#include <fstream>
#include <algorithm>
#include <filesystem>

#include "../../Logger/Log.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace Rapture {

// Define static members
json glTF2Loader::s_glTFDocument;
std::vector<unsigned char> glTF2Loader::s_binaryData;
std::vector<glTFMesh> glTF2Loader::s_meshes;
std::vector<glTFNode> glTF2Loader::s_nodes;
std::vector<glTFMaterial> glTF2Loader::s_materials;
std::vector<glTFTexture> glTF2Loader::s_textures;
std::vector<glTFSampler> glTF2Loader::s_samplers;
std::vector<glTFImage> glTF2Loader::s_images;
BufferLayout glTF2Loader::s_bufferLayout;
int glTF2Loader::s_defaultSceneIndex = 0;
Mesh* glTF2Loader::s_mesh = nullptr;

// GLTF Component Types
#define GLTF_FLOAT  5126
#define GLTF_UINT   5125
#define GLTF_USHORT 5123
#define GLTF_SHORT  5122
#define GLTF_UBYTE  5121
#define GLTF_BYTE   5120

void glTF2Loader::loadMesh(const std::string& filepath, Mesh* mesh) {
    if (!mesh) {
        GE_CORE_ERROR("Cannot load glTF into null mesh");
        return;
    }
    
    s_mesh = mesh;
    
    if (!parseGLTF(filepath)) {
        GE_CORE_ERROR("Failed to load glTF file: {0}", filepath);
        return;
    }

    // Parse all glTF components
    parseBufferViews();
    parseAccessors();
    parseImages();
    parseSamplers();
    parseTextures();
    parseMaterials();
    parseMeshes();
    parseNodes();
    
    // Process all nodes (which in turn processes meshes)
    processNodes();

    // Clean up resources
    cleanUp();
}

bool glTF2Loader::parseGLTF(const std::string& filepath) {
    std::filesystem::path path(filepath);
    std::filesystem::path directory = path.parent_path();
    
    // Load the glTF file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        GE_CORE_ERROR("Could not open glTF file: {0}", filepath);
        return false;
    }
    
    try {
        file >> s_glTFDocument;
        file.close();
    }
    catch (json::parse_error& e) {
        GE_CORE_ERROR("JSON parse error: {0}", e.what());
        return false;
    }
    
    // Check if there are any buffers defined
    if (s_glTFDocument.contains("buffers") && !s_glTFDocument["buffers"].empty()) {
        json buffer = s_glTFDocument["buffers"][0];
        
        // Check if the buffer is an embedded base64 buffer
        if (buffer.contains("uri")) {
            std::string uri = buffer["uri"];
            
            // Check if it's a data URI (base64)
            if (uri.substr(0, 5) == "data:") {
                // Handle embedded base64 data
                size_t commaPos = uri.find(',');
                if (commaPos != std::string::npos) {
                    std::string base64Data = uri.substr(commaPos + 1);
                    // TODO: Decode base64 data into s_binaryData
                }
            }
            else {
                // It's an external file
                std::filesystem::path bufferPath = directory / uri;
                std::ifstream binaryFile(bufferPath, std::ios::binary);
                
                if (!binaryFile.is_open()) {
                    GE_CORE_ERROR("Could not open binary file: {0}", bufferPath.string());
                    return false;
                }
                
                binaryFile.unsetf(std::ios::skipws);
                
                // Get file size
                binaryFile.seekg(0, std::ios::end);
                std::streampos fileSize = binaryFile.tellg();
                binaryFile.seekg(0, std::ios::beg);
                
                // Reserve space and read the file
                s_binaryData.resize(static_cast<size_t>(fileSize));
                binaryFile.read(reinterpret_cast<char*>(s_binaryData.data()), fileSize);
                
                binaryFile.close();
            }
        }
        // GLB format - binary chunk follows JSON chunk
        else if (filepath.substr(filepath.length() - 4) == ".glb") {
            // TODO: Implement GLB parsing
            GE_CORE_ERROR("GLB format not yet supported");
            return false;
        }
    }
    
    // Set the default scene
    s_defaultSceneIndex = s_glTFDocument.value("scene", 0);
    
    return true;
}

void glTF2Loader::parseBufferViews() {
    // Parse buffer views
    if (!s_glTFDocument.contains("bufferViews")) {
        return;
    }
    
    // Nothing to implement here yet - we'll use the buffer views directly
}

void glTF2Loader::parseAccessors() {
    // Parse accessors
    if (!s_glTFDocument.contains("accessors")) {
        return;
    }
    
    // Nothing to implement here yet - we'll use the accessors directly
}

void glTF2Loader::parseImages() {
    // Parse images section
    if (!s_glTFDocument.contains("images")) {
        return;
    }
    
    const json& images = s_glTFDocument["images"];
    s_images.resize(images.size());
    
    for (size_t i = 0; i < images.size(); i++) {
        const json& image = images[i];
        
        s_images[i].uri = image.value("uri", "");
        s_images[i].mimeType = image.value("mimeType", "");
        s_images[i].bufferView = image.value("bufferView", -1);
        
        // NOTE: We don't load the actual image data yet; we'll do that when needed
    }
}

void glTF2Loader::parseSamplers() {
    // Parse samplers section
    if (!s_glTFDocument.contains("samplers")) {
        return;
    }
    
    const json& samplers = s_glTFDocument["samplers"];
    s_samplers.resize(samplers.size());
    
    for (size_t i = 0; i < samplers.size(); i++) {
        const json& sampler = samplers[i];
        
        s_samplers[i].magFilter = sampler.value("magFilter", 9729); // GL_LINEAR
        s_samplers[i].minFilter = sampler.value("minFilter", 9987); // GL_LINEAR_MIPMAP_LINEAR
        s_samplers[i].wrapS = sampler.value("wrapS", 10497);        // GL_REPEAT
        s_samplers[i].wrapT = sampler.value("wrapT", 10497);        // GL_REPEAT
    }
}

void glTF2Loader::parseTextures() {
    // Parse textures section
    if (!s_glTFDocument.contains("textures")) {
        return;
    }
    
    const json& textures = s_glTFDocument["textures"];
    s_textures.resize(textures.size());
    
    for (size_t i = 0; i < textures.size(); i++) {
        const json& texture = textures[i];
        
        s_textures[i].source = texture.value("source", -1);
        s_textures[i].sampler = texture.value("sampler", -1);
    }
}

void glTF2Loader::parseMaterials() {
    // Parse materials section
    if (!s_glTFDocument.contains("materials")) {
        return;
    }
    
    const json& materials = s_glTFDocument["materials"];
    s_materials.resize(materials.size());
    
    for (size_t i = 0; i < materials.size(); i++) {
        const json& material = materials[i];
        
        s_materials[i].name = material.value("name", "Material_" + std::to_string(i));
        
        // Parse PBR metallic roughness properties
        if (material.contains("pbrMetallicRoughness")) {
            const json& pbr = material["pbrMetallicRoughness"];
            
            // Base color factor (default: white)
            if (pbr.contains("baseColorFactor")) {
                const json& bcf = pbr["baseColorFactor"];
                s_materials[i].baseColorFactor = glm::vec4(
                    bcf[0], bcf[1], bcf[2], bcf[3]
                );
            }
            
            // Base color texture
            if (pbr.contains("baseColorTexture")) {
                s_materials[i].baseColorTexture = pbr["baseColorTexture"].value("index", -1);
            }
            
            // Metallic factor (default: 1.0)
            s_materials[i].metallicFactor = pbr.value("metallicFactor", 1.0f);
            
            // Roughness factor (default: 1.0)
            s_materials[i].roughnessFactor = pbr.value("roughnessFactor", 1.0f);
            
            // Metallic-roughness texture
            if (pbr.contains("metallicRoughnessTexture")) {
                s_materials[i].metallicRoughnessTexture = pbr["metallicRoughnessTexture"].value("index", -1);
            }
        }
        
        // Normal texture
        if (material.contains("normalTexture")) {
            s_materials[i].normalTexture = material["normalTexture"].value("index", -1);
            s_materials[i].normalScale = material["normalTexture"].value("scale", 1.0f);
        }
        
        // Occlusion texture
        if (material.contains("occlusionTexture")) {
            s_materials[i].occlusionTexture = material["occlusionTexture"].value("index", -1);
            s_materials[i].occlusionStrength = material["occlusionTexture"].value("strength", 1.0f);
        }
        
        // Emissive texture and factor
        if (material.contains("emissiveTexture")) {
            s_materials[i].emissiveTexture = material["emissiveTexture"].value("index", -1);
        }
        
        if (material.contains("emissiveFactor")) {
            const json& ef = material["emissiveFactor"];
            s_materials[i].emissiveFactor = glm::vec3(ef[0], ef[1], ef[2]);
        }
        
        // Alpha mode
        if (material.contains("alphaMode")) {
            std::string alphaMode = material["alphaMode"];
            if (alphaMode == "MASK") {
                s_materials[i].alphaMode = GltfAlphaMode::MASK;
                s_materials[i].alphaCutoff = material.value("alphaCutoff", 0.5f);
            }
            else if (alphaMode == "BLEND") {
                s_materials[i].alphaMode = GltfAlphaMode::BLEND;
            }
            else {
                s_materials[i].alphaMode = GltfAlphaMode::OPAQUE;
            }
        }
        
        // Double sided
        s_materials[i].doubleSided = material.value("doubleSided", false);
    }
}

void glTF2Loader::parseMeshes() {
    // Parse meshes section
    if (!s_glTFDocument.contains("meshes")) {
        return;
    }
    
    const json& meshes = s_glTFDocument["meshes"];
    s_meshes.resize(meshes.size());
    
    for (size_t i = 0; i < meshes.size(); i++) {
        const json& mesh = meshes[i];
        
        s_meshes[i].name = mesh.value("name", "Mesh_" + std::to_string(i));
        
        // Parse primitives
        if (mesh.contains("primitives")) {
            const json& primitives = mesh["primitives"];
            s_meshes[i].primitives.resize(primitives.size());
            
            for (size_t j = 0; j < primitives.size(); j++) {
                const json& primitive = primitives[j];
                
                s_meshes[i].primitives[j].attributes = primitive["attributes"];
                s_meshes[i].primitives[j].indicesIndex = primitive.value("indices", -1);
                s_meshes[i].primitives[j].materialIndex = primitive.value("material", -1);
            }
        }
    }
}

void glTF2Loader::parseNodes() {
    // Parse nodes section
    if (!s_glTFDocument.contains("nodes")) {
        return;
    }
    
    const json& nodes = s_glTFDocument["nodes"];
    s_nodes.resize(nodes.size());
    
    for (size_t i = 0; i < nodes.size(); i++) {
        const json& node = nodes[i];
        
        s_nodes[i].name = node.value("name", "Node_" + std::to_string(i));
        s_nodes[i].meshIndex = node.value("mesh", -1);
        
        // Parse transformation
        glm::mat4 transform = glm::mat4(1.0f);
        
        // There are three ways to specify the node's transform:
        // 1. matrix (array of 16 values)
        // 2. translation, rotation, scale
        // 3. nothing (identity transform)
        
        if (node.contains("matrix")) {
            const json& matrix = node["matrix"];
            transform = glm::mat4(
                matrix[0], matrix[1], matrix[2], matrix[3],
                matrix[4], matrix[5], matrix[6], matrix[7],
                matrix[8], matrix[9], matrix[10], matrix[11],
                matrix[12], matrix[13], matrix[14], matrix[15]
            );
        }
        else {
            // Apply TRS properties if provided
            if (node.contains("translation")) {
                const json& t = node["translation"];
                transform = glm::translate(transform, glm::vec3(t[0], t[1], t[2]));
            }
            
            if (node.contains("rotation")) {
                const json& r = node["rotation"];
                // Note: glTF quaternion is [x, y, z, w], glm::quat constructor takes [w, x, y, z]
                glm::quat rotation(r[3], r[0], r[1], r[2]);
                transform = transform * glm::mat4_cast(rotation);
            }
            
            if (node.contains("scale")) {
                const json& s = node["scale"];
                transform = glm::scale(transform, glm::vec3(s[0], s[1], s[2]));
            }
        }
        
        s_nodes[i].localTransform = transform;
        
        // Parse children
        if (node.contains("children")) {
            const json& children = node["children"];
            s_nodes[i].children.resize(children.size());
            
            for (size_t j = 0; j < children.size(); j++) {
                s_nodes[i].children[j] = children[j];
            }
        }
    }
}

void glTF2Loader::processNodes() {
    // Only process if there are scenes defined
    if (!s_glTFDocument.contains("scenes")) {
        return;
    }
    
    // Get the default scene
    const json& scenes = s_glTFDocument["scenes"];
    
    if (s_defaultSceneIndex >= scenes.size()) {
        GE_CORE_ERROR("Invalid default scene index: {0}", s_defaultSceneIndex);
        return;
    }
    
    const json& scene = scenes[s_defaultSceneIndex];
    
    // Process each root node in the scene
    if (scene.contains("nodes")) {
        const json& nodes = scene["nodes"];
        
        for (const auto& nodeIndex : nodes) {
            processNode(nodeIndex, glm::mat4(1.0f), s_mesh);
        }
    }
}

void glTF2Loader::processNode(int nodeIndex, const glm::mat4& parentTransform, Mesh* mesh) {
    // Skip if node index is invalid
    if (nodeIndex < 0 || nodeIndex >= s_nodes.size()) {
        GE_CORE_ERROR("Invalid node index: {0}", nodeIndex);
        return;
    }
    
    const glTFNode& node = s_nodes[nodeIndex];
    
    // Calculate global transform
    glm::mat4 globalTransform = parentTransform * node.localTransform;
    
    // Process mesh if this node has one
    if (node.meshIndex >= 0 && node.meshIndex < s_meshes.size() && mesh) {
        loadGeometry(s_meshes[node.meshIndex], mesh);
    }
    
    // Process children
    for (int childIndex : node.children) {
        processNode(childIndex, globalTransform, mesh);
    }
}

void glTF2Loader::loadBufferData(int accessorIndex, std::vector<unsigned char>& dataVec) {
    // Skip if accessor index is invalid
    if (accessorIndex < 0 || !s_glTFDocument.contains("accessors") || 
        accessorIndex >= s_glTFDocument["accessors"].size()) {
        return;
    }
    
    const json& accessor = s_glTFDocument["accessors"][accessorIndex];
    
    // Get buffer view index
    int bufferViewIndex = accessor.value("bufferView", -1);
    if (bufferViewIndex < 0) {
        // No buffer view, possibly sparse accessor (not yet supported)
        return;
    }
    
    // Get buffer view
    const json& bufferView = s_glTFDocument["bufferViews"][bufferViewIndex];
    
    // Get buffer index
    int bufferIndex = bufferView.value("buffer", 0);
    
    // Get byte offset and stride
    size_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
    size_t byteLength = bufferView.value("byteLength", 0);
    
    // Copy data
    dataVec.resize(byteLength);
    std::memcpy(dataVec.data(), s_binaryData.data() + byteOffset, byteLength);
}

unsigned int glTF2Loader::getComponentSize(int componentType) {
    switch (componentType) {
        case GLTF_BYTE:
        case GLTF_UBYTE:
            return 1;
        case GLTF_SHORT:
        case GLTF_USHORT:
            return 2;
        case GLTF_UINT:
        case GLTF_FLOAT:
            return 4;
        default:
            return 0;
    }
}

unsigned int glTF2Loader::getTypeSize(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT2") return 4;
    if (type == "MAT3") return 9;
    if (type == "MAT4") return 16;
    return 0;
}

unsigned int glTF2Loader::getAccessorByteSize(int accessorIndex) {
    // Skip if accessor index is invalid
    if (accessorIndex < 0 || !s_glTFDocument.contains("accessors") || 
        accessorIndex >= s_glTFDocument["accessors"].size()) {
        return 0;
    }
    
    const json& accessor = s_glTFDocument["accessors"][accessorIndex];
    
    unsigned int count = accessor.value("count", 0);
    int componentType = accessor.value("componentType", 0);
    std::string type = accessor.value("type", "");
    
    return count * getComponentSize(componentType) * getTypeSize(type);
}

unsigned int glTF2Loader::getBufferViewStride(int bufferViewIndex) {
    // Skip if buffer view index is invalid
    if (bufferViewIndex < 0 || !s_glTFDocument.contains("bufferViews") || 
        bufferViewIndex >= s_glTFDocument["bufferViews"].size()) {
        return 0;
    }
    
    const json& bufferView = s_glTFDocument["bufferViews"][bufferViewIndex];
    
    return bufferView.value("byteStride", 0);
}

void glTF2Loader::loadGeometry(const glTFMesh& gltfMesh, Mesh* mesh) {
    if (!mesh) return;
    
    // Load each primitive as a submesh
    for (const auto& primitive : gltfMesh.primitives) {
        auto submesh = mesh->addSubMesh();
        submesh->setName(gltfMesh.name);
        
        // Load the primitive data
        loadPrimitive(primitive, submesh);
    }
}

void glTF2Loader::loadPrimitive(const glTFPrimitive& primitive, std::shared_ptr<SubMesh> submesh) {
    // TODO: implement primitive loading - we'll build vertex and index buffers from the accessor data
    
    // Load material if present
    if (primitive.materialIndex >= 0 && primitive.materialIndex < s_materials.size()) {
        std::shared_ptr<Material> material;
        loadMaterial(s_materials[primitive.materialIndex], material);
        submesh->setMaterial(material);
    }
}

void glTF2Loader::loadMaterial(const glTFMaterial& material, std::shared_ptr<Material>& outMaterial) {
    // Create the appropriate material based on the glTF material properties
    
    // PBR material is the default in glTF
    outMaterial = MaterialLibrary::createPBRMaterial(
        material.name,
        glm::vec3(material.baseColorFactor),
        material.roughnessFactor,
        material.metallicFactor,
        1.0f // Specular (not directly in glTF)
    );
    
    // Set additional properties if specified
    
    // Load textures if available
    if (material.baseColorTexture >= 0 && material.baseColorTexture < s_textures.size()) {
        std::shared_ptr<Texture2D> texture = loadTexture(s_textures[material.baseColorTexture]);
        if (texture) {
            outMaterial->setTexture("baseColorTexture", texture);
        }
    }
    
    if (material.metallicRoughnessTexture >= 0 && material.metallicRoughnessTexture < s_textures.size()) {
        std::shared_ptr<Texture2D> texture = loadTexture(s_textures[material.metallicRoughnessTexture]);
        if (texture) {
            outMaterial->setTexture("metallicRoughnessTexture", texture);
        }
    }
    
    if (material.normalTexture >= 0 && material.normalTexture < s_textures.size()) {
        std::shared_ptr<Texture2D> texture = loadTexture(s_textures[material.normalTexture]);
        if (texture) {
            outMaterial->setTexture("normalTexture", texture);
            outMaterial->setFloat("normalScale", material.normalScale);
        }
    }
    
    if (material.occlusionTexture >= 0 && material.occlusionTexture < s_textures.size()) {
        std::shared_ptr<Texture2D> texture = loadTexture(s_textures[material.occlusionTexture]);
        if (texture) {
            outMaterial->setTexture("occlusionTexture", texture);
            outMaterial->setFloat("occlusionStrength", material.occlusionStrength);
        }
    }
    
    if (material.emissiveTexture >= 0 && material.emissiveTexture < s_textures.size()) {
        std::shared_ptr<Texture2D> texture = loadTexture(s_textures[material.emissiveTexture]);
        if (texture) {
            outMaterial->setTexture("emissiveTexture", texture);
            outMaterial->setVec3("emissiveFactor", material.emissiveFactor);
        }
    }
    
    // Set alpha mode
    if (material.alphaMode == GltfAlphaMode::MASK) {
        outMaterial->setFloat("alphaCutoff", material.alphaCutoff);
        // Set flag for alpha mask mode
        outMaterial->setFlag(MaterialFlagBitLocations::TRANSPARENT, true);
    }
    else if (material.alphaMode == GltfAlphaMode::BLEND) {
        // Set flag for transparency
        outMaterial->setFlag(MaterialFlagBitLocations::TRANSPARENT, true);
    }
    
    // Set double-sided flag if needed
    // TODO: Add support for double-sided rendering
}

std::shared_ptr<Texture2D> glTF2Loader::loadTexture(const glTFTexture& texture) {
    // Skip if texture has no valid source
    if (texture.source < 0 || texture.source >= s_images.size()) {
        return nullptr;
    }
    
    // Get the image
    const glTFImage& image = s_images[texture.source];
    
    // Load image data and create texture
    return loadImageData(image);
}

std::shared_ptr<Texture2D> glTF2Loader::loadImageData(const glTFImage& image) {
    // Handle different image sources
    if (!image.uri.empty()) {
        // External image file
        std::filesystem::path imagePath = std::filesystem::path(image.uri);
        
        // Check if the image is already loaded in the texture library
        std::shared_ptr<Texture2D> existingTexture = TextureLibrary::get(imagePath.string());
        if (existingTexture) {
            return existingTexture;
        }
        
        // Load from file
        return TextureLibrary::load(imagePath.string());
    }
    else if (image.bufferView >= 0) {
        // Image data is stored in a buffer view
        // TODO: Implement loading from buffer view
        GE_CORE_ERROR("Loading textures from buffer views not yet implemented");
    }
    
    return nullptr;
}

void glTF2Loader::cleanUp() {
    // Clear all loaded data
    s_glTFDocument.clear();
    s_binaryData.clear();
    s_meshes.clear();
    s_nodes.clear();
    s_materials.clear();
    s_textures.clear();
    s_samplers.clear();
    s_images.clear();
    s_mesh = nullptr;
}

} // namespace Rapture 

*/