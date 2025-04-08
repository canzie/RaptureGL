#pragma once

#include "Material.h"
#include "MaterialParameter.h"
#include <unordered_map>
#include <string>
#include <memory>
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "../AssetsManager/Asset.h"


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

    // Enum-based parameter setting
    void setFloat(ParameterID id, float value);
    void setInt(ParameterID id, int value);
    void setBool(ParameterID id, bool value);
    void setVec2(ParameterID id, const glm::vec2& value);
    void setVec3(ParameterID id, const glm::vec3& value);
    void setVec4(ParameterID id, const glm::vec4& value);
    void setMat3(ParameterID id, const glm::mat3& value);
    void setMat4(ParameterID id, const glm::mat4& value);
    void setTexture(ParameterID id, std::shared_ptr<Texture2D> texture, AssetHandle handle);
    void setTextureBindless(ParameterID id, std::shared_ptr<Texture2D> texture, AssetHandle handle);
    void setParameter(ParameterID id, const MaterialParameter& parameter);
    bool hasParameterOverride(ParameterID id) const;
    const MaterialParameter& getParameterOverride(ParameterID id) const;
    void clearParameterOverride(ParameterID id);

    // String-based parameter setting (for backward compatibility)
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    void setBool(const std::string& name, bool value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    void setTexture(const std::string& name, std::shared_ptr<Texture2D> texture, AssetHandle handle);
    void setParameter(const std::string& name, const MaterialParameter& parameter);
    bool hasParameterOverride(const std::string& name) const;
    const MaterialParameter& getParameterOverride(const std::string& name) const;
    void clearParameterOverride(const std::string& name);
    
    // Binding
    void bind();
    void unbind();

private:
    std::shared_ptr<Material> m_baseMaterial;
    std::string m_name;
    std::shared_ptr<UniformBuffer> m_uniformBuffer = nullptr;
    MaterialParameterMap m_parameterOverrides;
    bool m_uniformDataDirty = false;

    // Update the uniform buffer with our parameter overrides
    void updateUniformBufferFromOverrides();
};

} // namespace Rapture 