#pragma once

#include "Material.h"
#include "MaterialParameter.h"
#include <unordered_map>
#include <string>
#include <memory>
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"

namespace Rapture {

class MaterialInstance {
public:
    MaterialInstance(const std::shared_ptr<Material>& material, const std::string& name);
    ~MaterialInstance() = default;

    // Get the base material
    std::shared_ptr<Material> getBaseMaterial() const { return m_baseMaterial; }
    
    // Get the name of this instance
    const std::string& getName() const { return m_name; }
    
    // Get instance-specific uniform buffer
    std::shared_ptr<UniformBuffer> getUniformBuffer() const { return m_uniformBuffer; }

    // Parameter setting
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    void setBool(const std::string& name, bool value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    void setTexture(const std::string& name, std::shared_ptr<Texture2D> texture);

    // Generic parameter setting
    void setParameter(const std::string& name, const MaterialParameter& parameter);
    
    // Check if a parameter is overridden
    bool hasParameterOverride(const std::string& name) const;
    
    // Get a parameter override
    const MaterialParameter& getParameterOverride(const std::string& name) const;
    
    // Clear an override
    void clearParameterOverride(const std::string& name);
    
    // Binding
    void bind();
    void unbind();

private:
    // Update the uniform buffer with our parameter overrides
    void updateUniformBufferFromOverrides();

    std::string m_name;
    std::shared_ptr<Material> m_baseMaterial;
    MaterialParameterMap m_parameterOverrides;
    std::shared_ptr<UniformBuffer> m_uniformBuffer = nullptr;
};

} // namespace Rapture 