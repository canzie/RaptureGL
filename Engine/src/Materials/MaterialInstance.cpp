#include "MaterialInstance.h"
#include "../Logger/Log.h"
#include <glad/glad.h>

namespace Rapture {

MaterialInstance::MaterialInstance(const std::shared_ptr<Material>& material, const std::string& name)
    : m_baseMaterial(material), m_name(name)
{
    if (!material) {
        GE_CORE_ERROR("MaterialInstance: Cannot create instance with null material!");
        return;
    }
    
    GE_CORE_INFO("Creating MaterialInstance '{0}' from base material '{1}'", name, material->getName());
    
    // Create a new uniform buffer for this instance if the base material has one
    if (material->getUniformBuffer()) {


        m_uniformBuffer = std::make_shared<UniformBuffer>(*material->getUniformBuffer());

        
        GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
            m_uniformBuffer->getID(), m_uniformBuffer->getSize(), m_uniformBuffer->getBindingPoint());
    }
}

void MaterialInstance::setFloat(ParameterID id, float value)
{
    m_parameterOverrides[id] = MaterialParameter::createFloat(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setInt(ParameterID id, int value)
{
    m_parameterOverrides[id] = MaterialParameter::createInt(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setBool(ParameterID id, bool value)
{
    m_parameterOverrides[id] = MaterialParameter::createBool(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setVec2(ParameterID id, const glm::vec2& value)
{
    m_parameterOverrides[id] = MaterialParameter::createVec2(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setVec3(ParameterID id, const glm::vec3& value)
{
    m_parameterOverrides[id] = MaterialParameter::createVec3(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setVec4(ParameterID id, const glm::vec4& value)
{
    m_parameterOverrides[id] = MaterialParameter::createVec4(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setMat3(ParameterID id, const glm::mat3& value)
{
    m_parameterOverrides[id] = MaterialParameter::createMat3(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setMat4(ParameterID id, const glm::mat4& value)
{
    m_parameterOverrides[id] = MaterialParameter::createMat4(value);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setTexture(ParameterID id, std::shared_ptr<Texture2D> texture, AssetHandle handle)
{
    m_parameterOverrides[id] = MaterialParameter::createTexture(texture, handle);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setTextureBindless(ParameterID id, std::shared_ptr<Texture2D> texture, AssetHandle handle)
{
    m_parameterOverrides[id] = MaterialParameter::createTextureBindless(texture, handle);
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

void MaterialInstance::setParameter(ParameterID id, const MaterialParameter& parameter)
{
    m_parameterOverrides[id] = parameter;
    if (m_baseMaterial) m_baseMaterial->markAsDirty();
}

bool MaterialInstance::hasParameterOverride(ParameterID id) const
{
    return m_parameterOverrides.find(id) != m_parameterOverrides.end();
}

const MaterialParameter& MaterialInstance::getParameterOverride(ParameterID id) const
{
    static MaterialParameter s_defaultParameter;
    auto it = m_parameterOverrides.find(id);
    if (it != m_parameterOverrides.end()) {
        return it->second;
    }
    return s_defaultParameter;
}

void MaterialInstance::clearParameterOverride(ParameterID id)
{
    auto it = m_parameterOverrides.find(id);
    if (it != m_parameterOverrides.end()) {
        m_parameterOverrides.erase(it);
        if (m_baseMaterial) m_baseMaterial->markAsDirty();
    }
}

void MaterialInstance::setFloat(const std::string& name, float value)
{
    setFloat(StringToParameterID(name), value);
}

void MaterialInstance::setInt(const std::string& name, int value)
{
    setInt(StringToParameterID(name), value);
}

void MaterialInstance::setBool(const std::string& name, bool value)
{
    setBool(StringToParameterID(name), value);
}

void MaterialInstance::setVec2(const std::string& name, const glm::vec2& value)
{
    setVec2(StringToParameterID(name), value);
}

void MaterialInstance::setVec3(const std::string& name, const glm::vec3& value)
{
    setVec3(StringToParameterID(name), value);
}

void MaterialInstance::setVec4(const std::string& name, const glm::vec4& value)
{
    setVec4(StringToParameterID(name), value);
}

void MaterialInstance::setMat3(const std::string& name, const glm::mat3& value)
{
    setMat3(StringToParameterID(name), value);
}

void MaterialInstance::setMat4(const std::string& name, const glm::mat4& value)
{
    setMat4(StringToParameterID(name), value);
}

void MaterialInstance::setTexture(const std::string& name, std::shared_ptr<Texture2D> texture, AssetHandle handle)
{
    setTexture(StringToParameterID(name), texture, handle);
}


void MaterialInstance::setParameter(const std::string& name, const MaterialParameter& parameter)
{
    setParameter(StringToParameterID(name), parameter);
}

bool MaterialInstance::hasParameterOverride(const std::string& name) const
{
    return hasParameterOverride(StringToParameterID(name));
}

const MaterialParameter& MaterialInstance::getParameterOverride(const std::string& name) const
{
    return getParameterOverride(StringToParameterID(name));
}

void MaterialInstance::clearParameterOverride(const std::string& name)
{
    clearParameterOverride(StringToParameterID(name));
}

void MaterialInstance::bind()
{
    if (!m_baseMaterial) {
        GE_CORE_ERROR("MaterialInstance: Cannot bind null material!");
        return;
    }
    
    GE_CORE_INFO("Binding MaterialInstance '{0}'", m_name);
    
    // First bind the shader from base material
    std::shared_ptr<Shader> shader = m_baseMaterial->getShader();
    if (shader) {
        shader->bind();
        GE_CORE_INFO("  Bound shader from base material");
    }
    
    // Then bind our private uniform buffer if we have one
    if (m_uniformBuffer) {
        uint32_t bindingPoint = m_uniformBuffer->getBindingPoint();
        GE_CORE_INFO("  Binding UBO {0} to binding point {1}", 
            m_uniformBuffer->getID(), bindingPoint);
        
        // Explicitly bind to the right binding point
        m_uniformBuffer->bindBase(bindingPoint);

        // Only update if parameters have changed
        if (m_baseMaterial->isDirty() || !m_parameterOverrides.empty()) {
            // Update the buffer with our overrides
            updateUniformBufferFromOverrides();
            
            // Ensure changes are flushed to GPU
            m_uniformBuffer->flush();
        }
    }
    else {
        // If we don't have our own UBO, bind the base material's data
        m_baseMaterial->bindData();
        
        // Then apply overrides directly to shader uniforms
        for (const auto& [name, parameter] : m_parameterOverrides) {
            switch (parameter.getType()) {
                case MaterialParameterType::FLOAT:
                    shader->setFloat(ParameterIDToString(name), parameter.asFloat());
                    break;
                case MaterialParameterType::INT:
                    shader->setInt(ParameterIDToString(name), parameter.asInt());
                    break;
                case MaterialParameterType::BOOL:
                    shader->setBool(ParameterIDToString(name), parameter.asBool());
                    break;
                case MaterialParameterType::VEC2:
                    shader->setVec2(ParameterIDToString(name), parameter.asVec2());
                    break;
                case MaterialParameterType::VEC3:
                    shader->setVec3(ParameterIDToString(name), parameter.asVec3());
                    break;
                case MaterialParameterType::VEC4:
                    shader->setVec4(ParameterIDToString(name), parameter.asVec4());
                    break;
                case MaterialParameterType::MAT3:
                    shader->setMat3(ParameterIDToString(name), parameter.asMat3());
                    break;
                case MaterialParameterType::MAT4:
                    shader->setMat4(ParameterIDToString(name), parameter.asMat4());
                    break;
                case MaterialParameterType::TEXTURE2D:
                    shader->setTexture(ParameterIDToString(name), parameter.asTexture().lock());
                    break;
                case MaterialParameterType::TEXTURE2D_BINDLESS:
                    shader->setUint64(ParameterIDToString(name), parameter.asTextureBindless());
                    break;
                default:
                    break;
            }
        }
    }
}

void MaterialInstance::unbind()
{
    if (m_baseMaterial) {
        if (m_uniformBuffer) {
            m_uniformBuffer->unbind();
        }
        m_baseMaterial->getShader()->unBind();
    }
}

// New method to update the uniform buffer with parameter overrides
void MaterialInstance::updateUniformBufferFromOverrides()
{
    if (!m_uniformBuffer || !m_baseMaterial) {
        return;
    }
    
    // If the material isn't marked as dirty, we can skip the update
    if (!m_baseMaterial->isDirty()) {
        return;
    }
    
    // The approach depends on the material type
    MaterialType type = m_baseMaterial->getType();
    
    // For now we'll handle just the common types for simplicity
    switch (type) {
        case MaterialType::PBR: {
            // Update PBR-specific parameters
            if (hasParameterOverride("baseColor")) {
                // PBR base color override
                const auto& param = getParameterOverride("baseColor");
                if (param.getType() == MaterialParameterType::VEC3) {
                    const glm::vec3& color = param.asVec3();
                    // We need to update just the base_color field in the PBR uniform
                    m_uniformBuffer->setData(&color, sizeof(glm::vec3));
                    GE_CORE_INFO("  Updated PBR base_color: ({0},{1},{2})", 
                        color.x, color.y, color.z);
                }
            }
            
            if (hasParameterOverride("roughness")) {
                // Roughness override
                const auto& param = getParameterOverride("roughness");
                if (param.getType() == MaterialParameterType::FLOAT) {
                    float value = param.asFloat();
                    // Roughness is at offset sizeof(glm::vec3)
                    m_uniformBuffer->setData(&value, sizeof(float));
                    GE_CORE_INFO("  Updated PBR roughness: {0}", value);
                }
            }
            
            // Similarly for metallic and specular...
            break;
        }
        
        case MaterialType::SOLID: {
            // Update solid color material parameters
            if (hasParameterOverride("color")) {
                const auto& param = getParameterOverride("color");
                if (param.getType() == MaterialParameterType::VEC3 || 
                    param.getType() == MaterialParameterType::VEC4) {
                    
                    glm::vec4 color;
                    if (param.getType() == MaterialParameterType::VEC3) {
                        // Convert vec3 to vec4 with alpha=1
                        glm::vec3 vec3Value = param.asVec3();
                        color = glm::vec4(vec3Value, 1.0f);
                    } else {
                        color = param.asVec4();
                    }
                    
                    // Update the color in the uniform buffer
                    m_uniformBuffer->setData(&color, sizeof(glm::vec4));
                    GE_CORE_INFO("  Updated SOLID color: ({0},{1},{2},{3})", 
                        color.x, color.y, color.z, color.w);
                }
            }
            break;
        }
        
        // Other material types...
        
        default: {
            
        }
    }
    
    // Reset the dirty flag after the update
    if (m_baseMaterial) {
        const_cast<Material*>(m_baseMaterial.get())->markAsDirty(); // This will reset in bindData()
    }
}
} // namespace Rapture 