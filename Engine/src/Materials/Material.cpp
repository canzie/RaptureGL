#include "Material.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../logger/Log.h"
#include "../Shaders/OpenGLShaders/OpenGLShader.h"
#include "MaterialInstance.h"
#include "../Logger/Log.h"
#include "../Textures/Texture.h"
#include "../Debug/TracyProfiler.h"
#include <glad/glad.h>
#include "../Utils/GLCapabilities.h"

namespace Rapture
{

	// Initialize static shader and UBO pointers for derived classes
	AssetHandle PBRMaterial::s_defaultShaderHandle = AssetHandle();

	AssetHandle PhongMaterial::s_defaultShaderHandle = AssetHandle();

	AssetHandle SolidMaterial::s_defaultShaderHandle = AssetHandle();
    
    // Initialize static shader for SpecularGlossiness material
    AssetHandle SpecularGlossinessMaterial::s_defaultShaderHandle = AssetHandle();

	AssetHandle CubeMapMaterial::s_defaultShaderHandle = AssetHandle();

    std::shared_ptr<Shader> Material::s_geometryPassShader = nullptr;
    std::shared_ptr<Shader> Material::s_lightingPassShader = nullptr;


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

	void Material::setShader(AssetHandle handle)
	{
        // check if shader is valid, and the handle points to a shaer, it will return nullptr neither are true
        if (auto shader = AssetManager::getAsset<Shader>(handle)) {
            m_weakShader = shader;
            m_shaderHandle = handle;
        } else {
            GE_CORE_ERROR("Material::setShader - Invalid shader handle: {0}", handle);
        }
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

	void Material::setFloat(ParameterID id, float value)
	{
		m_parameters[id] = MaterialParameter::createFloat(value);
		markDirty();
	}

	void Material::setInt(ParameterID id, int value)
	{
		m_parameters[id] = MaterialParameter::createInt(value);
		markDirty();
	}

	void Material::setBool(ParameterID id, bool value)
	{
		m_parameters[id] = MaterialParameter::createBool(value);
		markDirty();
	}

	void Material::setVec2(ParameterID id, const glm::vec2& value)
	{
		m_parameters[id] = MaterialParameter::createVec2(value);
		markDirty();
	}

	void Material::setVec3(ParameterID id, const glm::vec3& value)
	{
		m_parameters[id] = MaterialParameter::createVec3(value);
		markDirty();
	}

	void Material::setVec4(ParameterID id, const glm::vec4& value)
	{
		m_parameters[id] = MaterialParameter::createVec4(value);
		markDirty();
	}

	void Material::setMat3(ParameterID id, const glm::mat3& value)
	{
		m_parameters[id] = MaterialParameter::createMat3(value);
		markDirty();
	}

	void Material::setMat4(ParameterID id, const glm::mat4& value)
	{
		m_parameters[id] = MaterialParameter::createMat4(value);
		markDirty();
	}

	void Material::setTexture(ParameterID id, std::shared_ptr<Texture2D> texture, AssetHandle handle)
	{
		m_parameters[id] = MaterialParameter::createTexture(texture, handle);
		markDirty();
	}

	void Material::setTextureBindless(ParameterID id, std::shared_ptr<Texture2D> texture, AssetHandle handle)
	{
		m_parameters[id] = MaterialParameter::createTextureBindless(texture, handle);
		markDirty();
	}

	void Material::setParameter(ParameterID id, const MaterialParameter& parameter)
	{
		m_parameters[id] = parameter;
		markDirty();
	}

	bool Material::hasParameter(ParameterID id) const
	{
		RAPTURE_PROFILE_SCOPE("Material hasParameter");
		return m_parameters.find(id) != m_parameters.end();
	}

	const MaterialParameter& Material::getParameter(ParameterID id) const
	{
		RAPTURE_PROFILE_SCOPE("Material getParameter");
		static MaterialParameter s_defaultParameter;
		auto it = m_parameters.find(id);
		if (it != m_parameters.end()) {
			return it->second;
		}
		GE_CORE_WARN("Material parameter '{0}' not found in material '{1}'", static_cast<uint16_t>(id), m_name);
		return s_defaultParameter;
	}

	// String-based parameter methods now delegate to the ID-based methods
	void Material::setFloat(const std::string& name, float value)
	{
		setFloat(StringToParameterID(name), value);
	}

	void Material::setInt(const std::string& name, int value)
	{
		setInt(StringToParameterID(name), value);
	}

	void Material::setBool(const std::string& name, bool value)
	{
		setBool(StringToParameterID(name), value);
	}

	void Material::setVec2(const std::string& name, const glm::vec2& value)
	{
		setVec2(StringToParameterID(name), value);
	}

	void Material::setVec3(const std::string& name, const glm::vec3& value)
	{
		setVec3(StringToParameterID(name), value);
	}

	void Material::setVec4(const std::string& name, const glm::vec4& value)
	{
		setVec4(StringToParameterID(name), value);
	}

	void Material::setMat3(const std::string& name, const glm::mat3& value)
	{
		setMat3(StringToParameterID(name), value);
	}

	void Material::setMat4(const std::string& name, const glm::mat4& value)
	{
		setMat4(StringToParameterID(name), value);
	}

	void Material::setTexture(const std::string& name, std::shared_ptr<Texture2D> texture, AssetHandle handle)
	{
		setTexture(StringToParameterID(name), texture, handle);
	}

	void Material::setParameter(const std::string& name, const MaterialParameter& parameter)
	{
		setParameter(StringToParameterID(name), parameter);
	}

	bool Material::hasParameter(const std::string& name) const
	{
		RAPTURE_PROFILE_SCOPE("Material hasParameter");
		return hasParameter(StringToParameterID(name));
	}

	const MaterialParameter& Material::getParameter(const std::string& name) const
	{
		RAPTURE_PROFILE_SCOPE("Material getParameter");
		return getParameter(StringToParameterID(name));
	}



    void Material::bind(ShaderRenderPassType type)
    {
		RAPTURE_PROFILE_SCOPE("Material Bind");
		switch (type)
		{
			case ShaderRenderPassType::GEOMETRY: { 
                if (!s_geometryPassShader) throw std::runtime_error("Geometry pass shader not set for material '" + m_name + "'");
				s_geometryPassShader->bind();
                bindData();
				break;
            }
			case ShaderRenderPassType::LIGHTING: {
                if (!s_lightingPassShader) throw std::runtime_error("Lighting pass shader not set for material '" + m_name + "'");
				s_lightingPassShader->bind();
                bindData();
				break;
            }
            case ShaderRenderPassType::DEFAULT: {
                auto lockedShader = m_weakShader.lock(); // Lock it once
                if (!lockedShader) {
                    GE_CORE_CRITICAL("Material '{}': Attempting to bind with an expired/invalid shader! Handle: {}", m_name, m_shaderHandle);
                    throw std::runtime_error("Default shader is invalid for material '" + m_name + "'");
                }
                // Add log before the actual bind call
				lockedShader->bind(); // Use the locked pointer
                bindData();
				break;
            }
            default: {
				GE_CORE_ERROR("Invalid shader type for material '{0}'", m_name);
				return;
            }
        }
        m_lastShaderType = type;

	}

	void Material::unbind()
	{
		RAPTURE_PROFILE_SCOPE("Material Unbind");
        switch (m_lastShaderType)
        {
            case ShaderRenderPassType::GEOMETRY: {
                if (!s_geometryPassShader) throw std::runtime_error("Geometry pass shader not set for material '" + m_name + "'");
                s_geometryPassShader->unBind();
                break;
            }
            case ShaderRenderPassType::LIGHTING: {
                if (!s_lightingPassShader) throw std::runtime_error("Lighting pass shader not set for material '" + m_name + "'");
                s_lightingPassShader->unBind();
                break;
            }
            case ShaderRenderPassType::DEFAULT: {
                if (!m_weakShader.lock()) throw std::runtime_error("Default shader not set for material '" + m_name + "'");
                m_weakShader.lock()->unBind();
                break;
            }
            default: {
                GE_CORE_ERROR("Invalid shader renderpass type for material '{0}'", m_name);
                return;
            }
        }

		// Unbind all textures in the material parameters
		for (const auto& [name, param] : m_parameters) {
			if (param.getType() == MaterialParameterType::TEXTURE2D) {
				std::weak_ptr<Texture2D> texture = param.asTexture();
				if (auto tex = texture.lock()) {
					tex->unbind();
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
		
		if (!s_defaultShaderHandle) {
			GE_CORE_ERROR("Metal shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		

		setShader(s_defaultShaderHandle);


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
		//m_uniformData.flags = 0;
		
		// Store as parameters for serialization/deserialization
		setVec4("baseColor", glm::vec4(base_color, 1.0f));
		setFloat("roughness", roughness);
		setFloat("metallic", metallic);
		setFloat("specular", specular);

        GE_DEBUG_TRACE("PBR Material finished creating: {0}", m_name);
	}

	void PBRMaterial::bindData()
	{
		RAPTURE_PROFILE_GPU_SCOPE("PBR Material Bind Data");
		RAPTURE_PROFILE_SCOPE("PBR Material Bind Data");

		if (!m_uniformBuffer) {
			GE_CORE_ERROR("PBR material {0} has no uniform buffer!", m_name);
			return;
		}
		{
			RAPTURE_PROFILE_SCOPE("PBR Material Uniform Data Update");
			RAPTURE_PROFILE_GPU_SCOPE("PBR Material Uniform Data Update");
			
			// Update uniform data from parameters using enum keys
			if (m_isDirty && hasParameter(ParameterID::BASE_COLOR)) {
				m_uniformData.baseColorFactor = getParameter(ParameterID::BASE_COLOR).asVec4();
			}
			if (m_isDirty && hasParameter(ParameterID::ROUGHNESS)) {
				m_uniformData.roughnessFactor = getParameter(ParameterID::ROUGHNESS).asFloat();
			}
			if (m_isDirty && hasParameter(ParameterID::METALLIC)) {
				m_uniformData.metallicFactor = getParameter(ParameterID::METALLIC).asFloat();
			}
			if (m_isDirty && hasParameter(ParameterID::SPECULAR)) {
				m_uniformData.specularFactor = getParameter(ParameterID::SPECULAR).asFloat();
			}
		}
		
        // Reset texture flags
	    uint32_t textureFlags = 0;

		if (!GLCapabilities::hasBindlessTextures()) { // Non-bindless Textures

			RAPTURE_PROFILE_SCOPE("PBR Material - Uniform Texture bindings");
			RAPTURE_PROFILE_GPU_SCOPE("PBR Material - Uniform Texture bindings");


			// Bind all PBR textures to their respective slots and update flags
			if (hasParameter(ParameterID::TEXTURE_ALBEDO)) {
                
				std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_ALBEDO).asTexture().lock();
				if (texture) {
					texture->bind(static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
					textureFlags |= PBRTextureFlags::ALBEDO_MAP;
				}
			}
			
			if (hasParameter(ParameterID::TEXTURE_NORMAL)) {
				std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_NORMAL).asTexture().lock();
				if (texture) {
					texture->bind(static_cast<uint32_t>(TextureActiveSlot::NORMAL));
					textureFlags |= PBRTextureFlags::NORMAL_MAP;
				}
			}
			
			if (hasParameter(ParameterID::TEXTURE_METALLIC)) {
				std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_METALLIC).asTexture().lock();
				if (texture) {
					texture->bind(static_cast<uint32_t>(TextureActiveSlot::METALLIC));
					textureFlags |= PBRTextureFlags::METALLIC_MAP;
				}
			}
			
			if (hasParameter(ParameterID::TEXTURE_ROUGHNESS)) {
				std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_ROUGHNESS).asTexture().lock();
				if (texture) {
					texture->bind(static_cast<uint32_t>(TextureActiveSlot::ROUGHNESS));
					textureFlags |= PBRTextureFlags::ROUGHNESS_MAP;
				}
			}
			
			if (hasParameter(ParameterID::TEXTURE_AO)) {
				std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_AO).asTexture().lock();
				if (texture) {
					texture->bind(static_cast<uint32_t>(TextureActiveSlot::AO));
					textureFlags |= PBRTextureFlags::AO_MAP;
				}
			}
			
			if (hasParameter(ParameterID::TEXTURE_EMISSIVE)) {
				std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_EMISSIVE).asTexture().lock();
				if (texture) {
					texture->bind(static_cast<uint32_t>(TextureActiveSlot::EMISSION));
					textureFlags |= PBRTextureFlags::EMISSIVE_MAP;
				}
			}

        } else { // Bindless Textures

			RAPTURE_PROFILE_SCOPE("PBR Material - Bindless Textures");
			RAPTURE_PROFILE_GPU_SCOPE("PBR Material - Bindless Textures");

            if (m_isDirty && hasParameter(ParameterID::TEXTURE_ALBEDO_BINDLESS)) {
                uint64_t handle = getParameter(ParameterID::TEXTURE_ALBEDO_BINDLESS).asTextureBindless();
                if (handle) {
                    m_uniformData.albedoMap = handle;
                }
            }

            if (m_isDirty && hasParameter(ParameterID::TEXTURE_METALLIC_BINDLESS)) {
                uint64_t handle = getParameter(ParameterID::TEXTURE_METALLIC_BINDLESS).asTextureBindless();
                if (handle) {
                    m_uniformData.metallicMap = handle;
                }
            }

            if (m_isDirty && hasParameter(ParameterID::TEXTURE_ROUGHNESS_BINDLESS)) {
                uint64_t handle = getParameter(ParameterID::TEXTURE_ROUGHNESS_BINDLESS).asTextureBindless();
                if (handle) {
                    m_uniformData.roughnessMap = handle;
                }
            }

            if (m_isDirty && hasParameter(ParameterID::TEXTURE_NORMAL_BINDLESS)) {
                uint64_t handle = getParameter(ParameterID::TEXTURE_NORMAL_BINDLESS).asTextureBindless();
                if (handle) {
                    m_uniformData.normalMap = handle;
                }
            }

            if (m_isDirty && hasParameter(ParameterID::TEXTURE_AO_BINDLESS)) {
                uint64_t handle = getParameter(ParameterID::TEXTURE_AO_BINDLESS).asTextureBindless();
                if (handle) {
                    m_uniformData.aoMap = handle;
                }
            }

            if (m_isDirty && hasParameter(ParameterID::TEXTURE_EMISSIVE_BINDLESS)) {
                uint64_t handle = getParameter(ParameterID::TEXTURE_EMISSIVE_BINDLESS).asTextureBindless();
                if (handle) {
                    m_uniformData.emissiveMap = handle;
                }
            }

		}
		
        
			// Update texture flags in uniform data
		if (m_uniformData.flags != textureFlags) {
			m_uniformData.flags = textureFlags;
			markDirty();
		}

		// Explicitly bind UBO to binding point before updating
		m_uniformBuffer->bindBase(PBR_BINDING_POINT_IDX);
		
		// Only update the data if the material is dirty
		if (m_isDirty) {


			// Now update the data
			m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
			// Force flush to ensure data is sent to GPU
			m_uniformBuffer->flush();
			// Reset the dirty flag
			m_isDirty = false;
		}
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
		
		if (!s_defaultShaderHandle) {
			GE_CORE_ERROR("Phong shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_defaultShaderHandle);
		
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
		RAPTURE_PROFILE_GPU_SCOPE("Phong Material Bind Data");
		if (!m_uniformBuffer) {
			GE_CORE_ERROR("Phong material {0} has no uniform buffer!", m_name);
			return;
		}
		
		// Update uniform data from parameters using enum keys
		if (hasParameter(ParameterID::FLUX))
			m_uniformData.flux = getParameter(ParameterID::FLUX).asFloat();
		if (hasParameter(ParameterID::DIFFUSE_COLOR))
			m_uniformData.diffuseColor = getParameter(ParameterID::DIFFUSE_COLOR).asVec4();
		if (hasParameter(ParameterID::SPECULAR_COLOR))
			m_uniformData.specularColor = getParameter(ParameterID::SPECULAR_COLOR).asVec4();
		if (hasParameter(ParameterID::AMBIENT_LIGHT))
			m_uniformData.ambientLight = getParameter(ParameterID::AMBIENT_LIGHT).asVec4();
		if (hasParameter(ParameterID::SHININESS))
			m_uniformData.shininess = getParameter(ParameterID::SHININESS).asFloat();
		
		// Explicitly bind UBO to binding point before updating
		m_uniformBuffer->bindBase(PHONG_BINDING_POINT_IDX);
		
		// Only update the data if the material is dirty
		if (m_isDirty) {
			// Now update the data
			m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
			// Force flush to ensure data is sent to GPU
			m_uniformBuffer->flush();
			// Reset the dirty flag
			m_isDirty = false;
		}
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
		
		if (!s_defaultShaderHandle) {
			GE_CORE_ERROR("Solid shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_defaultShaderHandle);
		
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
		setVec3(ParameterID::BASE_COLOR, base_color);
	}

	void SolidMaterial::bindData()
	{
		RAPTURE_PROFILE_GPU_SCOPE("Solid Material Bind Data");
		if (!m_uniformBuffer) {
			GE_CORE_ERROR("Solid material {0} has no uniform buffer!", m_name);
			return;
		}
		
		// Update uniform data from parameters using enum keys
		if (hasParameter(ParameterID::BASE_COLOR)) {
			glm::vec3 color = getParameter(ParameterID::BASE_COLOR).asVec3();
			m_uniformData.baseColorFactor = glm::vec4(color, 1.0f);
		}
		// Bind all PBR textures to their respective slots
		if (hasParameter(ParameterID::TEXTURE_ALBEDO)) {
			
			std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_ALBEDO).asTexture().lock();
			if (texture && m_weakShader.lock()) {
				texture->bind(static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
				m_weakShader.lock()->setBool("u_HasAlbedoMap", true);
			} else {
				m_weakShader.lock()->setBool("u_HasAlbedoMap", false);
			}
		} else {
			m_weakShader.lock()->setBool("u_HasAlbedoMap", false);
		}


		if (hasParameter(ParameterID::TEXTURE_NORMAL)) {
			std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_NORMAL).asTexture().lock();
			if (texture) {
				texture->bind(static_cast<uint32_t>(TextureActiveSlot::NORMAL));
			}
		}
		
		// Explicitly bind UBO to binding point before updating
		m_uniformBuffer->bindBase(SOLID_BINDING_POINT_IDX);
		
		// Only update the data if the material is dirty
		if (m_isDirty) {
			// Now update the data
			m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
			// Force flush to ensure data is sent to GPU
			m_uniformBuffer->flush();
			// Reset the dirty flag
			m_isDirty = false;
		}
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
        
        if (!s_defaultShaderHandle) {
            GE_CORE_ERROR("Specular-Glossiness shader not initialized! Use MaterialLibrary::init() first.");
            return;
        }
        
        setShader(s_defaultShaderHandle);
        
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
        m_uniformData.glossinessFactor = glossiness;
        m_uniformData.specularFactor = glm::vec4(specularColor, 1.0f);
        m_uniformData.flags = 0;

        // Store as parameters for serialization/deserialization
        setVec3("diffuseColor", diffuseColor);
        setVec3("specularColor", specularColor);
        setFloat("glossiness", glossiness);
    }

    void SpecularGlossinessMaterial::bindData()
    {
		RAPTURE_PROFILE_GPU_SCOPE("Specular-Glossiness Material Bind Data");
        if (!m_uniformBuffer) {
            GE_CORE_ERROR("Specular-Glossiness material {0} has no uniform buffer!", m_name);
            return;
        }
        
        // Update uniform data from parameters using enum keys
        if (m_isDirty && hasParameter(ParameterID::DIFFUSE_COLOR)) {
            glm::vec3 diffuse = getParameter(ParameterID::DIFFUSE_COLOR).asVec3();
            m_uniformData.diffuseFactor = glm::vec4(diffuse, m_uniformData.diffuseFactor.a);
        }
        
        if (m_isDirty && hasParameter(ParameterID::SPECULAR_COLOR)) {
            glm::vec3 specular = getParameter(ParameterID::SPECULAR_COLOR).asVec3();
            m_uniformData.specularFactor = glm::vec4(specular, m_uniformData.specularFactor.a);
        }
        
        if (m_isDirty && hasParameter(ParameterID::SHININESS)) {
            float glossiness = getParameter(ParameterID::SHININESS).asFloat();
            m_uniformData.specularFactor.a = glossiness;
        }
        
        uint32_t flags = 0;
        // Bind textures
        if (hasParameter(ParameterID::TEXTURE_DIFFUSE)) {
            std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_DIFFUSE).asTexture().lock();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
                flags |= SpecularGlossinessTextureFlags::DIFFUSE_MAP;
            }
        } 
        
        if (hasParameter(ParameterID::TEXTURE_SPECULAR)) {
            std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_SPECULAR).asTexture().lock();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::SPECULAR));
                flags |= SpecularGlossinessTextureFlags::SPEC_GLOSS_MAP;
            } 
        } 

        if (hasParameter(ParameterID::TEXTURE_NORMAL)) {
            std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_NORMAL).asTexture().lock();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::NORMAL));
                flags |= SpecularGlossinessTextureFlags::NORMAL_MAP;
            } 
        } 
        
        if (hasParameter(ParameterID::TEXTURE_AO)) {
            std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_AO).asTexture().lock();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::AO));
                flags |= SpecularGlossinessTextureFlags::AO_MAP;
            } 
        }
        
        if (hasParameter(ParameterID::TEXTURE_EMISSIVE)) {
            std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_EMISSIVE).asTexture().lock();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::EMISSION));
                flags |= SpecularGlossinessTextureFlags::EMISSIVE_MAP;
            } 
        }

        if (m_uniformData.flags != flags) {
            m_uniformData.flags = flags;
            markDirty();
        }
        
        // Explicitly bind UBO to binding point before updating
        m_uniformBuffer->bindBase(SPECULAR_GLOSSINESS_BINDING_POINT_IDX);
        
        // Only update the data if the material is dirty
        if (m_isDirty) {
            // Now update the data
            m_uniformBuffer->setData(&m_uniformData, sizeof(m_uniformData));
            // Force flush to ensure data is sent to GPU
            m_uniformBuffer->flush();
            // Reset the dirty flag
            m_isDirty = false;
        }
    }
    CubeMapMaterial::CubeMapMaterial()
    : Material(MaterialType::CUBE_MAP, "CubeMap_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
    {
        setShader(s_defaultShaderHandle);
    }

    CubeMapMaterial::CubeMapMaterial(std::shared_ptr<Texture2D> skybox, AssetHandle handle)
    : Material(MaterialType::CUBE_MAP, "CubeMap_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
    {
        setShader(s_defaultShaderHandle);

        setTexture(ParameterID::TEXTURE_CUBEMAP, skybox, handle);

    }

    void CubeMapMaterial::bindData()
    {
        // Bind the skybox texture to texture slot 0
        if (hasParameter(ParameterID::TEXTURE_CUBEMAP)) {
            std::shared_ptr<Texture2D> texture = getParameter(ParameterID::TEXTURE_CUBEMAP).asTexture().lock();
            if (texture) {
                texture->bind(static_cast<uint32_t>(TextureActiveSlot::CUBEMAP));
            }
        }
    }
}