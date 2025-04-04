#pragma once

#include <string>
#include <variant>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

#include "../AssetsManager/AssetManager.h"


namespace Rapture {

class Texture2D;

// Forward declare
class MaterialParameter;



enum class MaterialParameterType {
    NONE,
    FLOAT, INT, BOOL,
    VEC2, VEC3, VEC4,
    MAT3, MAT4,
    TEXTURE2D, TEXTURECUBE
};


enum class ParameterID : uint16_t {
    NONE,
    BASE_COLOR,
    METALLIC,
    ROUGHNESS,
    SPECULAR,
    EMISSION,
    SHININESS,
    FLUX,
    DIFFUSE_COLOR,
    SPECULAR_COLOR,
    AMBIENT_LIGHT,
    TEXTURE_FLAGS,
    TEXTURE_ALBEDO,
    TEXTURE_METALLIC,
    TEXTURE_ROUGHNESS,
    TEXTURE_NORMAL,
    TEXTURE_HEIGHT,
    TEXTURE_AO,
    TEXTURE_EMISSIVE,
    TEXTURE_DISPLACEMENT,
    TEXTURE_SHININESS,
    TEXTURE_OPACITY,
    TEXTURE_EMISSION,
    TEXTURE_DIFFUSE,
    TEXTURE_SPECULAR,
    TEXTURE_CUBEMAP
    
};

// Utility functions for parameter name conversions
inline std::string ParameterIDToString(ParameterID id) {
    switch (id) {
        case ParameterID::NONE: return "none";
        case ParameterID::BASE_COLOR: return "baseColor";
        case ParameterID::METALLIC: return "metallic";
        case ParameterID::ROUGHNESS: return "roughness";
        case ParameterID::SPECULAR: return "specular";
        case ParameterID::EMISSION: return "emission";
        case ParameterID::SHININESS: return "shininess";
        case ParameterID::FLUX: return "flux";
        case ParameterID::DIFFUSE_COLOR: return "diffuseColor";
        case ParameterID::SPECULAR_COLOR: return "specularColor";
        case ParameterID::AMBIENT_LIGHT: return "ambientLight";
        case ParameterID::TEXTURE_FLAGS: return "textureFlags";
        case ParameterID::TEXTURE_ALBEDO: return "albedoMap";
        case ParameterID::TEXTURE_METALLIC: return "metallicMap";
        case ParameterID::TEXTURE_ROUGHNESS: return "roughnessMap";
        case ParameterID::TEXTURE_NORMAL: return "normalMap";
        case ParameterID::TEXTURE_HEIGHT: return "heightMap";
        case ParameterID::TEXTURE_AO: return "aoMap";
        case ParameterID::TEXTURE_EMISSIVE: return "emissiveMap";
        case ParameterID::TEXTURE_DISPLACEMENT: return "displacementMap";
        case ParameterID::TEXTURE_SHININESS: return "shininessMap";
        case ParameterID::TEXTURE_OPACITY: return "opacityMap";
        case ParameterID::TEXTURE_EMISSION: return "emissionMap";
        case ParameterID::TEXTURE_DIFFUSE: return "diffuseMap";
        case ParameterID::TEXTURE_SPECULAR: return "specularMap";
        case ParameterID::TEXTURE_CUBEMAP: return "cubeMap";
        default: return "unknown";
    }
}

inline ParameterID StringToParameterID(const std::string& name) {
    // Common material parameters
    if (name == "baseColor") return ParameterID::BASE_COLOR;
    if (name == "color") return ParameterID::BASE_COLOR; // Alias for baseColor
    if (name == "metallic") return ParameterID::METALLIC;
    if (name == "roughness") return ParameterID::ROUGHNESS;
    if (name == "specular") return ParameterID::SPECULAR;
    if (name == "emission") return ParameterID::EMISSION;
    if (name == "emissiveFactor") return ParameterID::EMISSION; // Alias for emission
    if (name == "shininess") return ParameterID::SHININESS;
    if (name == "flux") return ParameterID::FLUX;
    if (name == "diffuseColor") return ParameterID::DIFFUSE_COLOR;
    if (name == "specularColor") return ParameterID::SPECULAR_COLOR;
    if (name == "ambientLight") return ParameterID::AMBIENT_LIGHT;
    if (name == "textureFlags") return ParameterID::TEXTURE_FLAGS;
    
    // Texture maps
    if (name == "albedoMap") return ParameterID::TEXTURE_ALBEDO;
    if (name == "metallicMap") return ParameterID::TEXTURE_METALLIC;
    if (name == "roughnessMap") return ParameterID::TEXTURE_ROUGHNESS;
    if (name == "normalMap") return ParameterID::TEXTURE_NORMAL;
    if (name == "heightMap") return ParameterID::TEXTURE_HEIGHT;
    if (name == "aoMap") return ParameterID::TEXTURE_AO;
    if (name == "emissiveMap") return ParameterID::TEXTURE_EMISSIVE;
    if (name == "displacementMap") return ParameterID::TEXTURE_DISPLACEMENT;
    if (name == "shininessMap") return ParameterID::TEXTURE_SHININESS;
    if (name == "opacityMap") return ParameterID::TEXTURE_OPACITY;
    if (name == "emissionMap") return ParameterID::TEXTURE_EMISSION;
    if (name == "diffuseMap") return ParameterID::TEXTURE_DIFFUSE;
    if (name == "specularMap") return ParameterID::TEXTURE_SPECULAR;
    if (name == "specularGlossinessMap") return ParameterID::TEXTURE_SPECULAR; // Alias
    if (name == "cubeMap") return ParameterID::TEXTURE_CUBEMAP;
    
    return ParameterID::NONE;
}

struct Texture2DReference {
    mutable std::weak_ptr<Texture2D> texture;
    AssetHandle handle;

    Texture2DReference(std::shared_ptr<Texture2D> tex, AssetHandle handle) : texture(tex), handle(handle) {}

    std::weak_ptr<Texture2D> getTexture2D() const {
        
        if (auto tex = texture.lock()) {
            return tex;
        } else {
            tex = AssetManager::getAsset<Texture2D>(handle);
            if (tex) {  
                texture = tex;
                return tex;
            }
        }

        return std::weak_ptr<Texture2D>();
    }
    
};

// New enum-based parameter map (primary definition)
using MaterialParameterMap = std::unordered_map<ParameterID, MaterialParameter>;

// Legacy string-based parameter map for backward compatibility
using StringMaterialParameterMap = std::unordered_map<std::string, MaterialParameter>;

// Class to handle different material parameter types
class MaterialParameter {
public:
    MaterialParameter() : m_type(MaterialParameterType::NONE) {}
    
    // Static factory methods for better type safety
    static MaterialParameter createFloat(float value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::FLOAT;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createInt(int value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::INT;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createBool(bool value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::BOOL;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createVec2(const glm::vec2& value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::VEC2;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createVec3(const glm::vec3& value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::VEC3;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createVec4(const glm::vec4& value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::VEC4;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createMat3(const glm::mat3& value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::MAT3;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createMat4(const glm::mat4& value) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::MAT4;
        param.m_value = value;
        return param;
    }
    
    static MaterialParameter createTexture(std::shared_ptr<Texture2D> texture, AssetHandle handle) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::TEXTURE2D;
        param.m_value = Texture2DReference(texture, handle);
        return param;
    }

    // Getters
    MaterialParameterType getType() const { return m_type; }
    
    // Type-safe accessors
    float asFloat() const { return std::get<float>(m_value); }
    int asInt() const { return std::get<int>(m_value); }
    bool asBool() const { return std::get<bool>(m_value); }
    const glm::vec2& asVec2() const { return std::get<glm::vec2>(m_value); }
    const glm::vec3& asVec3() const { return std::get<glm::vec3>(m_value); }
    const glm::vec4& asVec4() const { return std::get<glm::vec4>(m_value); }
    const glm::mat3& asMat3() const { return std::get<glm::mat3>(m_value); }
    const glm::mat4& asMat4() const { return std::get<glm::mat4>(m_value); }
    std::weak_ptr<Texture2D> asTexture() const {
        if (std::holds_alternative<Texture2DReference>(m_value)) {
            return std::get<Texture2DReference>(m_value).getTexture2D();
        }
        
        return std::weak_ptr<Texture2D>();
    }
    
    // Utility to get raw data pointer for uniform setting
    const void* getData() const {
        switch (m_type) {
            case MaterialParameterType::FLOAT: return &std::get<float>(m_value);
            case MaterialParameterType::INT: return &std::get<int>(m_value);
            case MaterialParameterType::BOOL: return &std::get<bool>(m_value);
            case MaterialParameterType::VEC2: return &std::get<glm::vec2>(m_value);
            case MaterialParameterType::VEC3: return &std::get<glm::vec3>(m_value);
            case MaterialParameterType::VEC4: return &std::get<glm::vec4>(m_value);
            case MaterialParameterType::MAT3: return &std::get<glm::mat3>(m_value);
            case MaterialParameterType::MAT4: return &std::get<glm::mat4>(m_value);
            default: return nullptr;
        }
    }

private:
    MaterialParameterType m_type;
    std::variant<
        std::monostate,
        float, int, bool,
        glm::vec2, glm::vec3, glm::vec4,
        glm::mat3, glm::mat4,
        Texture2DReference
    > m_value;
};

} // namespace Rapture 