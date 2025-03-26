#include "Material.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../logger/Log.h"
#include "../Shaders/OpenGLShaders/OpenGLShader.h"
#include "MaterialInstance.h"
#include "../Logger/Log.h"
#include "../Textures/Texture.h"
#include <glad/glad.h>

namespace Rapture
{

	// Initialize static shader and UBO pointers for derived classes
	Shader* PBRMaterial::s_shader = nullptr;

	Shader* PhongMaterial::s_shader = nullptr;

	Shader* SolidMaterial::s_shader = nullptr;
    
    // Initialize static shader for SpecularGlossiness material
    Shader* SpecularGlossinessMaterial::s_shader = nullptr;

	//OpenGLShader* Material::s_shader = nullptr;



	/*
	bool Material::setAttrib(std::string key, float value)
	{
		if (m_uniform_attr.contains(key))
		{
			m_uniform_attr[key] = value;
			return true;
		}

		return false;
	}
	*/



	Material::Material(MaterialType type, const std::string& name)
		: m_type(type), m_name(name), m_renderFlags(0)
	{
	}

	std::shared_ptr<MaterialInstance> Material::createInstance(const std::string& instanceName)
	{
		return std::make_shared<MaterialInstance>(shared_from_this(), instanceName);
	}

	void Material::setShader(Shader* shader)
	{
		m_shader = shader;
	}

	void Material::setUniformBuffer(std::shared_ptr<UniformBuffer> uniformBuffer)
	{
		m_uniformBuffer = uniformBuffer;
	}

	void Material::setFlag(MaterialFlagBitLocations flag, bool enabled)
	{
		if (enabled) {
			m_renderFlags |= (1 << static_cast<int>(flag));
		} else {
			m_renderFlags &= ~(1 << static_cast<int>(flag));
		}
	}

	bool Material::hasFlag(MaterialFlagBitLocations flag) const
	{
		return (m_renderFlags & (1 << static_cast<int>(flag))) != 0;
	}

	void Material::setFloat(const std::string& name, float value)
	{
		m_parameters[name] = MaterialParameter::createFloat(value);
	}

	void Material::setInt(const std::string& name, int value)
	{
		m_parameters[name] = MaterialParameter::createInt(value);
	}

	void Material::setBool(const std::string& name, bool value)
	{
		m_parameters[name] = MaterialParameter::createBool(value);
	}

	void Material::setVec2(const std::string& name, const glm::vec2& value)
	{
		m_parameters[name] = MaterialParameter::createVec2(value);
	}

	void Material::setVec3(const std::string& name, const glm::vec3& value)
	{
		m_parameters[name] = MaterialParameter::createVec3(value);
	}

	void Material::setVec4(const std::string& name, const glm::vec4& value)
	{
		m_parameters[name] = MaterialParameter::createVec4(value);
	}

	void Material::setMat3(const std::string& name, const glm::mat3& value)
	{
		m_parameters[name] = MaterialParameter::createMat3(value);
	}

	void Material::setMat4(const std::string& name, const glm::mat4& value)
	{
		m_parameters[name] = MaterialParameter::createMat4(value);
	}

	void Material::setTexture(const std::string& name, std::shared_ptr<Texture2D> texture)
	{
		m_parameters[name] = MaterialParameter::createTexture(texture);
	}

	void Material::setParameter(const std::string& name, const MaterialParameter& parameter)
	{
		m_parameters[name] = parameter;
	}

	bool Material::hasParameter(const std::string& name) const
	{
		return m_parameters.find(name) != m_parameters.end();
	}

	const MaterialParameter& Material::getParameter(const std::string& name) const
	{
		static MaterialParameter s_defaultParameter;
		auto it = m_parameters.find(name);
		if (it != m_parameters.end()) {
			return it->second;
		}
		GE_CORE_WARN("Material parameter '{0}' not found in material '{1}'", name, m_name);
		return s_defaultParameter;
	}

	void Material::bind()
	{
		if (m_shader)
		{
            m_shader->bind();
			bindData();
		}
		else
		{
			GE_CORE_ERROR("Attempted to bind material '{0}' with no shader!", m_name);
		}
	}

	void Material::unbind()
	{
		if (m_shader)
			m_shader->unBind();

		// Unbind all textures in the material parameters
		for (const auto& [name, param] : m_parameters) {
			if (param.getType() == MaterialParameterType::TEXTURE2D) {
				std::shared_ptr<Texture2D> texture = param.asTexture();
				if (texture) {
					texture->unbind();
				}
			}
		}
	}

	PBRMaterial::PBRMaterial()
		: PBRMaterial(glm::vec3(0.5f, 0.5f, 0.5f), 0.5f, 1.0f, 0.5f) { }

	PBRMaterial::PBRMaterial(glm::vec3 base_color, float roughness, float metallic, float specular)
		: Material(MaterialType::PBR, "PBR_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating PBR Material: {0} (Color: {1},{2},{3})", 
			m_name, base_color.x, base_color.y, base_color.z);
		
		if (!s_shader) {
			GE_CORE_ERROR("Metal shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		

		setShader(s_shader);


		m_uniformBuffer = std::make_shared<UniformBuffer>(
            sizeof(m_uniformData), 
            BufferUsage::Dynamic, 
            &m_uniformData,
            PBR_BINDING_POINT_IDX);
		
        GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getID(), 
			sizeof(m_uniformData), 
			PBR_BINDING_POINT_IDX);
		
		m_uniformData.baseColorFactor = glm::vec4(base_color, 1.0f);
		m_uniformData.metallicFactor = metallic;
		m_uniformData.roughnessFactor = roughness;
		m_uniformData.specularFactor = specular;
		
		// Store as parameters for serialization/deserialization
		setVec4("baseColor", glm::vec4(base_color, 1.0f));
		setFloat("roughness", roughness);
		setFloat("metallic", metallic);
		setFloat("specular", specular);

        GE_DEBUG_TRACE("PBR Material finished creating: {0}", m_name);
	}

	void PBRMaterial::bindData()
	{
		if (!m_uniformBuffer) {
			GE_CORE_ERROR("PBR material {0} has no uniform buffer!", m_name);
			return;
		}
	
		// Update uniform data from parameters
		if (hasParameter("baseColor"))
			m_uniformData.baseColorFactor = getParameter("baseColor").asVec4();
		if (hasParameter("roughness")) 
			m_uniformData.roughnessFactor = getParameter("roughness").asFloat();
		if (hasParameter("metallic"))
			m_uniformData.metallicFactor = getParameter("metallic").asFloat();
		if (hasParameter("specular"))
			m_uniformData.specularFactor = getParameter("specular").asFloat();
        

        // Bind all PBR textures to their respective slots
        if (hasParameter("albedoMap")) {
            
            std::shared_ptr<Texture2D> texture = getParameter("albedoMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
                m_shader->setInt("u_AlbedoMap", static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
                m_shader->setBool("u_HasAlbedoMap", true);
            } else {
                m_shader->setBool("u_HasAlbedoMap", false);
            }
        } else {
            m_shader->setBool("u_HasAlbedoMap", false);
        }
        
        if (hasParameter("normalMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("normalMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::NORMAL));
                m_shader->setInt("u_NormalMap", static_cast<uint32_t>(TextureActiveSlot::NORMAL));
                m_shader->setBool("u_HasNormalMap", true);
            } else {
                m_shader->setBool("u_HasNormalMap", false);
            }
        } else {
            m_shader->setBool("u_HasNormalMap", false);
        }
        
        if (hasParameter("metallicMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("metallicMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::METALLIC));
                m_shader->setInt("u_MetallicMap", static_cast<uint32_t>(TextureActiveSlot::METALLIC));
                m_shader->setBool("u_HasMetallicMap", true);
            } else {
                m_shader->setBool("u_HasMetallicMap", false);
            }
        } else {
            m_shader->setBool("u_HasMetallicMap", false);
        }
        
        if (hasParameter("roughnessMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("roughnessMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::ROUGHNESS));
                m_shader->setInt("u_RoughnessMap", static_cast<uint32_t>(TextureActiveSlot::ROUGHNESS));
                m_shader->setBool("u_HasRoughnessMap", true);
            } else {
                m_shader->setBool("u_HasRoughnessMap", false);
            }
        } else {
            m_shader->setBool("u_HasRoughnessMap", false);
        }
        
        if (hasParameter("aoMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("aoMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::AO));
                m_shader->setInt("u_AOMap", static_cast<uint32_t>(TextureActiveSlot::AO));
                m_shader->setBool("u_HasAOMap", true);
            } else {
                m_shader->setBool("u_HasAOMap", false);
            }
        } else {
            m_shader->setBool("u_HasAOMap", false);
        }
        
        if (hasParameter("emissiveMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("emissiveMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::EMISSION));
                m_shader->setInt("u_EmissiveMap", static_cast<uint32_t>(TextureActiveSlot::EMISSION));
                m_shader->setBool("u_HasEmissiveMap", true);
            } else {
                m_shader->setBool("u_HasEmissiveMap", false);
            }
        } else {
            m_shader->setBool("u_HasEmissiveMap", false);
        }

		
		// Explicitly bind UBO to binding point before updating
		m_uniformBuffer->bindBase(PBR_BINDING_POINT_IDX);
		
		// Now update the data
		m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
		
		// Force flush to ensure data is sent to GPU
		m_uniformBuffer->flush();
	}

    // default constructor
	PhongMaterial::PhongMaterial()
		: PhongMaterial(1.0f, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), 32.0f) 
        { }

	// constructor with parameters
	PhongMaterial::PhongMaterial(float flux, glm::vec4 diffuseColor, glm::vec4 specularColor, glm::vec4 ambientLight, float shininess)
		: Material(MaterialType::PHONG, "Phong_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating Phong Material: {0}", m_name);
		
		if (!s_shader) {
			GE_CORE_ERROR("Phong shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_shader);
		
		// Create our uniform buffer
		m_uniformBuffer = std::make_shared<UniformBuffer>(
            sizeof(m_uniformData), 
            BufferUsage::Dynamic, 
            &m_uniformData, 
            PHONG_BINDING_POINT_IDX);
		
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getID(), 
			sizeof(m_uniformData), 
			PHONG_BINDING_POINT_IDX);
		
		m_uniformData.flux = flux;
		m_uniformData.diffuseColor = diffuseColor;
		m_uniformData.specularColor = specularColor;
		m_uniformData.ambientLight = ambientLight;
		m_uniformData.shininess = shininess;
		
		// Store as parameters for serialization/deserialization
		setFloat("flux", flux);
		setVec4("diffuseColor", diffuseColor);
		setVec4("specularColor", specularColor);
		setVec4("ambientLight", ambientLight);
		setFloat("shininess", shininess);
	}

	void PhongMaterial::bindData()
	{
		if (!m_uniformBuffer) {
			GE_CORE_ERROR("Phong material {0} has no uniform buffer!", m_name);
			return;
		}
		
		// Update uniform data from parameters
		if (hasParameter("flux"))
			m_uniformData.flux = getParameter("flux").asFloat();
		if (hasParameter("diffuseColor"))
			m_uniformData.diffuseColor = getParameter("diffuseColor").asVec4();
		if (hasParameter("specularColor"))
			m_uniformData.specularColor = getParameter("specularColor").asVec4();
		if (hasParameter("ambientLight"))
			m_uniformData.ambientLight = getParameter("ambientLight").asVec4();
		if (hasParameter("shininess"))
			m_uniformData.shininess = getParameter("shininess").asFloat();
		
		// Explicitly bind UBO to binding point before updating
		m_uniformBuffer->bindBase(PHONG_BINDING_POINT_IDX);
		
		// Now update the data
		m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
		
		// Force flush to ensure data is sent to GPU
		m_uniformBuffer->flush();
	}

	// default constructor
	SolidMaterial::SolidMaterial()
		: SolidMaterial(glm::vec3(1.0f, 0.0f, 1.0f)) { }

	// constructor with parameters
	SolidMaterial::SolidMaterial(glm::vec3 base_color)
		: Material(MaterialType::SOLID, "Solid_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating Solid Material: {0} (Color: {1},{2},{3})", 
			m_name, base_color.x, base_color.y, base_color.z);
		
		if (!s_shader) {
			GE_CORE_ERROR("Solid shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_shader);
		
		// Create our uniform buffer
		m_uniformBuffer = std::make_shared<UniformBuffer>(
            sizeof(m_uniformData), 
            BufferUsage::Dynamic, 
            &m_uniformData, 
            SOLID_BINDING_POINT_IDX);
		
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getID(), 
			sizeof(m_uniformData), 
			SOLID_BINDING_POINT_IDX);
		
		m_uniformData.baseColorFactor = glm::vec4(base_color, 1.0f);
		
		// Store as parameters for serialization/deserialization
		setVec3("color", base_color);
	}

	void SolidMaterial::bindData()
	{
		if (!m_uniformBuffer) {
			GE_CORE_ERROR("Solid material {0} has no uniform buffer!", m_name);
			return;
		}
		
		// Update uniform data from parameters
		if (hasParameter("color")) {
			glm::vec3 color = getParameter("color").asVec3();
			m_uniformData.baseColorFactor = glm::vec4(color, 1.0f);
		}
                if (hasParameter("albedoMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("albedoMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
            }
        }
        if (hasParameter("normalMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("normalMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::NORMAL));
            }
        }
		
		// Explicitly bind UBO to binding point before updating
		m_uniformBuffer->bindBase(SOLID_BINDING_POINT_IDX);
		
		// Now update the data
		m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
		
		// Force flush to ensure data is sent to GPU
		m_uniformBuffer->flush();
	}

    // Default constructor for SpecularGlossinessMaterial
    SpecularGlossinessMaterial::SpecularGlossinessMaterial()
        : SpecularGlossinessMaterial(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f), 0.5f) { }

    // Constructor with parameters
    SpecularGlossinessMaterial::SpecularGlossinessMaterial(glm::vec3 diffuseColor, glm::vec3 specularColor, float glossiness)
        : Material(MaterialType::KHR_SPECULAR_GLOSSINESS, "SpecGloss_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
    {
        GE_CORE_INFO("Creating Specular-Glossiness Material: {0} (Diffuse: {1},{2},{3}, Specular: {4},{5},{6}, Glossiness: {7})", 
            m_name, diffuseColor.x, diffuseColor.y, diffuseColor.z, 
            specularColor.x, specularColor.y, specularColor.z, glossiness);
        
        if (!s_shader) {
            GE_CORE_ERROR("Specular-Glossiness shader not initialized! Use MaterialLibrary::init() first.");
            return;
        }
        
        setShader(s_shader);
        
        // Create our uniform buffer
        m_uniformBuffer = std::make_shared<UniformBuffer>(
            sizeof(m_uniformData), 
            BufferUsage::Dynamic, 
            &m_uniformData, 
            SPECULAR_GLOSSINESS_BINDING_POINT_IDX);
        
        GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
            m_uniformBuffer->getID(), 
            sizeof(m_uniformData), 
            SPECULAR_GLOSSINESS_BINDING_POINT_IDX);
        
        m_uniformData.diffuseFactor = glm::vec4(diffuseColor, 1.0f);
        m_uniformData.specularFactor = glm::vec4(specularColor, glossiness);
        m_uniformData.flags = 0.0f;
        
        // Store as parameters for serialization/deserialization
        setVec3("diffuseColor", diffuseColor);
        setVec3("specularColor", specularColor);
        setFloat("glossiness", glossiness);
    }

    void SpecularGlossinessMaterial::bindData()
    {
        if (!m_uniformBuffer) {
            GE_CORE_ERROR("Specular-Glossiness material {0} has no uniform buffer!", m_name);
            return;
        }
        
        // Update uniform data from parameters
        if (hasParameter("diffuseColor")) {
            glm::vec3 diffuse = getParameter("diffuseColor").asVec3();
            m_uniformData.diffuseFactor = glm::vec4(diffuse, m_uniformData.diffuseFactor.a);
        }
        
        if (hasParameter("specularColor")) {
            glm::vec3 specular = getParameter("specularColor").asVec3();
            m_uniformData.specularFactor = glm::vec4(specular, m_uniformData.specularFactor.a);
        }
        
        if (hasParameter("glossiness")) {
            float glossiness = getParameter("glossiness").asFloat();
            m_uniformData.specularFactor.a = glossiness;
        }
        
        // Bind textures
        if (hasParameter("diffuseMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("diffuseMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
                m_shader->setInt("u_DiffuseMap", static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
                m_shader->setBool("u_HasDiffuseMap", true);
            } else {
                m_shader->setBool("u_HasDiffuseMap", false);
            }
        } else {
            m_shader->setBool("u_HasDiffuseMap", false);
        }
        
        if (hasParameter("specularGlossinessMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("specularGlossinessMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::SPECULAR));
                m_shader->setInt("u_SpecularGlossinessMap", static_cast<uint32_t>(TextureActiveSlot::SPECULAR));
                m_shader->setBool("u_HasSpecularGlossinessMap", true);
            } else {
                m_shader->setBool("u_HasSpecularGlossinessMap", false);
            }
        } else {
            m_shader->setBool("u_HasSpecularGlossinessMap", false);
        }

        if (hasParameter("normalMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("normalMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::NORMAL));
                m_shader->setInt("u_NormalMap", static_cast<uint32_t>(TextureActiveSlot::NORMAL));
                m_shader->setBool("u_HasNormalMap", true);
            } else {
                m_shader->setBool("u_HasNormalMap", false);
            }
        } else {
            m_shader->setBool("u_HasNormalMap", false);
        }
        
        if (hasParameter("aoMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("aoMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::AO));
                m_shader->setInt("u_AOMap", static_cast<uint32_t>(TextureActiveSlot::AO));
                m_shader->setBool("u_HasAOMap", true);
            } else {
                m_shader->setBool("u_HasAOMap", false);
            }
        } else {
            m_shader->setBool("u_HasAOMap", false);
        }
        
        if (hasParameter("emissiveMap")) {
            std::shared_ptr<Texture2D> texture = getParameter("emissiveMap").asTexture();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::EMISSION));
                m_shader->setInt("u_EmissiveMap", static_cast<uint32_t>(TextureActiveSlot::EMISSION));
                m_shader->setBool("u_HasEmissiveMap", true);
            } else {
                m_shader->setBool("u_HasEmissiveMap", false);
            }
        } else {
            m_shader->setBool("u_HasEmissiveMap", false);
        }
        
        // Explicitly bind UBO to binding point before updating
        m_uniformBuffer->bindBase(SPECULAR_GLOSSINESS_BINDING_POINT_IDX);
        
        // Now update the data
        m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
        
        // Force flush to ensure data is sent to GPU
        m_uniformBuffer->flush();
    }
}