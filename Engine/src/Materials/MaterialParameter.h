#pragma once

#include <string>
#include <variant>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Rapture {

class Texture2D;

// Forward declare
class MaterialParameter;
using MaterialParameterMap = std::unordered_map<std::string, MaterialParameter>;

enum class MaterialParameterType {
    NONE,
    FLOAT, INT, BOOL,
    VEC2, VEC3, VEC4,
    MAT3, MAT4,
    TEXTURE2D, TEXTURECUBE
};

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
    
    static MaterialParameter createTexture(std::shared_ptr<Texture2D> texture) {
        MaterialParameter param;
        param.m_type = MaterialParameterType::TEXTURE2D;
        param.m_value = texture;
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
    std::shared_ptr<Texture2D> asTexture() const { return std::get<std::shared_ptr<Texture2D>>(m_value); }
    
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
        std::shared_ptr<Texture2D>
    > m_value;
};

} // namespace Rapture 