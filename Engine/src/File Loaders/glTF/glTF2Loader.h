/*
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include "json.hpp"

#include "../../Textures/Texture.h"
#include "../../Materials/Material.h"
#include "../../Materials/MaterialLibrary.h"
#include "../../Mesh/Mesh.h"
#include "../../Mesh/SubMesh.h"

namespace Rapture {

using json = nlohmann::json;

// Alpha modes in glTF
enum class GltfAlphaMode {
    OPAQUE,
    MASK,
    BLEND
};

// Structs to store glTF data
struct glTFPrimitive {
    json attributes;
    int indicesIndex = -1;
    int materialIndex = -1;
};

struct glTFMesh {
    std::string name;
    std::vector<glTFPrimitive> primitives;
};

struct glTFNode {
    std::string name;
    int meshIndex = -1;
    glm::mat4 localTransform = glm::mat4(1.0f);
    std::vector<int> children;
};

struct glTFMaterial {
    std::string name;
    
    // PBR metallic roughness
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    int baseColorTexture = -1;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    int metallicRoughnessTexture = -1;
    
    // Normal texture
    int normalTexture = -1;
    float normalScale = 1.0f;
    
    // Occlusion texture
    int occlusionTexture = -1;
    float occlusionStrength = 1.0f;
    
    // Emissive properties
    int emissiveTexture = -1;
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    
    // Alpha properties
    GltfAlphaMode alphaMode = GltfAlphaMode::OPAQUE;
    float alphaCutoff = 0.5f;
    
    // Miscellaneous properties
    bool doubleSided = false;
};

struct glTFSampler {
    int magFilter = 9729; // GL_LINEAR
    int minFilter = 9987; // GL_LINEAR_MIPMAP_LINEAR
    int wrapS = 10497;    // GL_REPEAT
    int wrapT = 10497;    // GL_REPEAT
};

struct glTFTexture {
    int source = -1;
    int sampler = -1;
};

struct glTFImage {
    std::string uri;
    std::string mimeType;
    int bufferView = -1;
};

class glTF2Loader {
public:
    // Main entry point: Load a glTF mesh from a file
    static void loadMesh(const std::string& filepath, Mesh* mesh);

private:
    // glTF parsing functions
    static bool parseGLTF(const std::string& filepath);
    static void parseBufferViews();
    static void parseAccessors();
    static void parseImages();
    static void parseSamplers();
    static void parseTextures();
    static void parseMaterials();
    static void parseMeshes();
    static void parseNodes();
    
    // Node processing
    static void processNodes();
    static void processNode(int nodeIndex, const glm::mat4& parentTransform, Mesh* mesh);
    
    // Geometry loading
    static void loadGeometry(const glTFMesh& mesh, Mesh* outMesh);
    static void loadPrimitive(const glTFPrimitive& primitive, std::shared_ptr<SubMesh> submesh);
    
    // Material loading
    static void loadMaterial(const glTFMaterial& material, std::shared_ptr<Material>& outMaterial);
    
    // Texture loading
    static std::shared_ptr<Texture2D> loadTexture(const glTFTexture& texture);
    static std::shared_ptr<Texture2D> loadImageData(const glTFImage& image);
    
    // Buffer handling
    static void loadBufferData(int accessorIndex, std::vector<unsigned char>& dataVec);
    static unsigned int getComponentSize(int componentType);
    static unsigned int getTypeSize(const std::string& type);
    static unsigned int getAccessorByteSize(int accessorIndex);
    static unsigned int getBufferViewStride(int bufferViewIndex);
    
    // Clean up resources
    static void cleanUp();
    
    // Static storage for glTF data
    static json s_glTFDocument;
    static std::vector<unsigned char> s_binaryData;
    static std::vector<glTFMesh> s_meshes;
    static std::vector<glTFNode> s_nodes;
    static std::vector<glTFMaterial> s_materials;
    static std::vector<glTFTexture> s_textures;
    static std::vector<glTFSampler> s_samplers;
    static std::vector<glTFImage> s_images;
    static BufferLayout s_bufferLayout;
    static int s_defaultSceneIndex;
    static Mesh* s_mesh;
};

} // namespace Rapture 

*/