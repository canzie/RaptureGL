#pragma once

#include "../Shaders/Shader.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "MaterialUniformLayouts.h"
#include "MaterialParameter.h"
#include "../Buffers/Buffers.h"
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"

namespace Rapture {

enum class MaterialType {
	PBR,
	PHONG,
	SOLID,
	KHR_SPECULAR_GLOSSINESS,
	CUSTOM
};



enum class MaterialFlagBitLocations {
	TRANSPARENT=0,
	OCCLUSION=1
	// add more on the go
};

// Forward declarations
class MaterialInstance;

class Material : public std::enable_shared_from_this<Material> {
    public:
        Material(MaterialType type, const std::string& name);
        virtual ~Material() = default;

        // Create an instance of this material
        std::shared_ptr<MaterialInstance> createInstance(const std::string& instanceName);

        // Get material information
        MaterialType getType() const { return m_type; }
        const std::string& getName() const { return m_name; }
        void setName(const std::string& name) { m_name = name; }
        
        // Shader management
        void setShader(Shader* shader);
        Shader* getShader() const { return m_shader; }
        
        // Uniform buffer management
        void setUniformBuffer(std::shared_ptr<UniformBuffer> uniformBuffer);
        std::shared_ptr<UniformBuffer> getUniformBuffer() const { return m_uniformBuffer; }
        
        // Feature flags
        void setFlag(MaterialFlagBitLocations flag, bool enabled);
        bool hasFlag(MaterialFlagBitLocations flag) const;
        
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
        
        // Check if material has a parameter
        bool hasParameter(const std::string& name) const;
        
        // Get a parameter 
        const MaterialParameter& getParameter(const std::string& name) const;
        
        // Binding and drawing
        virtual void bind();
        virtual void unbind();
        
        // Derived classes need to implement this to upload specific uniform data
        virtual void bindData() = 0;

        // Public method to force a material data update on next bind
        void markAsDirty() { m_isDirty = true; }
        bool isDirty() const { return m_isDirty; }

    protected:
        std::string m_name;
        MaterialType m_type;
        Shader* m_shader = nullptr;
        std::shared_ptr<UniformBuffer> m_uniformBuffer = nullptr;
        char m_renderFlags = 0;
        MaterialParameterMap m_parameters;
        bool m_isDirty = true;
        
        void markDirty() { m_isDirty = true; }

};

// Existing material classes

class PBRMaterial : public Material {
    public:
        PBRMaterial();
        PBRMaterial(glm::vec3 base_color, float roughness, float metallic, float specular);

        virtual void bindData() override;

        // Make static members public so they can be initialized by MaterialLibrary
        static Shader* s_shader;

    protected:
        PBRUniform m_uniformData;
};

class PhongMaterial : public Material {
    public:
        PhongMaterial();
        PhongMaterial(float flux, glm::vec4 diffuseColor, glm::vec4 specularColor, glm::vec4 ambientLight, float shininess);

        virtual void bindData() override;

        // Make static members public so they can be initialized by MaterialLibrary
        static Shader* s_shader;

    protected:
        PhongUniform m_uniformData;
};

class SolidMaterial : public Material {
    public:
        SolidMaterial();
        SolidMaterial(glm::vec3 base_color);

        virtual void bindData() override;

        // Make static members public so they can be initialized by MaterialLibrary
        static Shader* s_shader;

    protected:
        SolidColorUniform m_uniformData;
};

// New SpecularGlossiness material class
class SpecularGlossinessMaterial : public Material {
    public:
        SpecularGlossinessMaterial();
        SpecularGlossinessMaterial(glm::vec3 diffuseColor, glm::vec3 specularColor, float glossiness);

        virtual void bindData() override;

        // Make static members public so they can be initialized by MaterialLibrary
        static Shader* s_shader;

    protected:
        SpecularGlossinessUniform m_uniformData;
};

} // namespace Rapture
