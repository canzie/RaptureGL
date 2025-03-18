#include "Material.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../logger/Log.h"
#include "../Shaders/OpenGLShaders/OpenGLShader.h"
#include "../Shaders/OpenGLUniforms/OpenGLUniformBuffer.h"
#include "MaterialInstance.h"
#include "../Logger/Log.h"
#include "../Textures/Texture.h"
#include <glad/glad.h>

namespace Rapture
{

	// Initialize static shader and UBO pointers for derived classes
	Shader* MetalMaterial::s_shader = nullptr;

	Shader* PhongMaterial::s_shader = nullptr;

	Shader* SolidMaterial::s_shader = nullptr;

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

	void Material::setUniformBuffer(UniformBuffer* uniformBuffer)
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
	}

	MetalMaterial::MetalMaterial()
		: Material(MaterialType::PBR, "Metal_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating Metal Material: {0}", m_name);
		
		if (!s_shader) {
			GE_CORE_ERROR("Metal shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_shader);
		
		// Create our uniform buffer
		m_uniformBuffer = new OpenGLUniformBuffer(sizeof(m_uniformData), PBR_BINDING_POINT_IDX);
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getRendererID(), 
			sizeof(m_uniformData), 
			PBR_BINDING_POINT_IDX);
		
		// Default PBR values
		m_uniformData.base_color = glm::vec3(0.5f, 0.5f, 0.5f);
		m_uniformData.roughness = 0.5f;
		m_uniformData.metallic = 1.0f;
		m_uniformData.specular = 0.5f;
		
		// Also store as parameters for serialization/deserialization
		setVec3("baseColor", m_uniformData.base_color);
		setFloat("roughness", m_uniformData.roughness);
		setFloat("metallic", m_uniformData.metallic);
		setFloat("specular", m_uniformData.specular);
	}

	MetalMaterial::MetalMaterial(glm::vec3 base_color, float roughness, float metallic, float specular)
		: Material(MaterialType::PBR, "Metal_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating Metal Material: {0} (Color: {1},{2},{3})", 
			m_name, base_color.x, base_color.y, base_color.z);
		
		if (!s_shader) {
			GE_CORE_ERROR("Metal shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_shader);
		
		// Create our uniform buffer
		m_uniformBuffer = new OpenGLUniformBuffer(sizeof(m_uniformData), PBR_BINDING_POINT_IDX);
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getRendererID(), 
			sizeof(m_uniformData), 
			PBR_BINDING_POINT_IDX);
		
		m_uniformData.base_color = base_color;
		m_uniformData.roughness = roughness;
		m_uniformData.metallic = metallic;
		m_uniformData.specular = specular;
		
		// Store as parameters for serialization/deserialization
		setVec3("baseColor", base_color);
		setFloat("roughness", roughness);
		setFloat("metallic", metallic);
		setFloat("specular", specular);
	}

	void MetalMaterial::bindData()
	{
		if (!m_uniformBuffer) {
			GE_CORE_ERROR("Metal material {0} has no uniform buffer!", m_name);
			return;
		}
	
		// Update uniform data from parameters
		if (hasParameter("baseColor"))
			m_uniformData.base_color = getParameter("baseColor").asVec3();
		if (hasParameter("roughness")) 
			m_uniformData.roughness = getParameter("roughness").asFloat();
		if (hasParameter("metallic"))
			m_uniformData.metallic = getParameter("metallic").asFloat();
		if (hasParameter("specular"))
			m_uniformData.specular = getParameter("specular").asFloat();
		
		// Explicitly bind UBO to binding point before updating
		glBindBufferBase(GL_UNIFORM_BUFFER, PBR_BINDING_POINT_IDX, m_uniformBuffer->getRendererID());
		
		// Now update the data
		m_uniformBuffer->updateAllBufferData(&m_uniformData);
		
		// Force flush to ensure data is sent to GPU
		glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBuffer->getRendererID());
		glFlush();
	}

	PhongMaterial::PhongMaterial()
		: Material(MaterialType::PHONG, "Phong_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating Phong Material: {0}", m_name);
		
		if (!s_shader) {
			GE_CORE_ERROR("Phong shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_shader);
		
		// Create our uniform buffer
		m_uniformBuffer = new OpenGLUniformBuffer(sizeof(m_uniformData), PHONG_BINDING_POINT_IDX);
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getRendererID(), 
			sizeof(m_uniformData), 
			PHONG_BINDING_POINT_IDX);
		
		// Default values
		m_uniformData.flux = 1.0f;
		m_uniformData.diffuseColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
		m_uniformData.specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		m_uniformData.ambientLight = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
		m_uniformData.shininess = 32.0f;
		
		// Also store as parameters for serialization/deserialization
		setFloat("flux", m_uniformData.flux);
		setVec4("diffuseColor", m_uniformData.diffuseColor);
		setVec4("specularColor", m_uniformData.specularColor);
		setVec4("ambientLight", m_uniformData.ambientLight);
		setFloat("shininess", m_uniformData.shininess);
	}

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
		m_uniformBuffer = new OpenGLUniformBuffer(sizeof(m_uniformData), PHONG_BINDING_POINT_IDX);
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getRendererID(), 
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
		glBindBufferBase(GL_UNIFORM_BUFFER, PHONG_BINDING_POINT_IDX, m_uniformBuffer->getRendererID());
		
		// Now update the data
		m_uniformBuffer->updateAllBufferData(&m_uniformData);
		
		// Force flush to ensure data is sent to GPU
		glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBuffer->getRendererID());
		glFlush();
	}

	SolidMaterial::SolidMaterial()
		: Material(MaterialType::SOLID, "Solid_" + std::to_string(reinterpret_cast<uintptr_t>(this)))
	{
		GE_CORE_INFO("Creating Solid Material: {0}", m_name);
		
		if (!s_shader) {
			GE_CORE_ERROR("Solid shader not initialized! Use MaterialLibrary::init() first.");
			return;
		}
		
		setShader(s_shader);
		
		// Create our uniform buffer
		m_uniformBuffer = new OpenGLUniformBuffer(sizeof(m_uniformData), SOLID_BINDING_POINT_IDX);
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getRendererID(), 
			sizeof(m_uniformData), 
			SOLID_BINDING_POINT_IDX);
		
		// Default to white
		m_uniformData.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		
		// Also store as parameters
		setVec4("color", m_uniformData.color);
	}

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
		m_uniformBuffer = new OpenGLUniformBuffer(sizeof(m_uniformData), SOLID_BINDING_POINT_IDX);
		GE_CORE_INFO("  Created UBO: ID={0}, Size={1}, BindingPoint={2}", 
			m_uniformBuffer->getRendererID(), 
			sizeof(m_uniformData), 
			SOLID_BINDING_POINT_IDX);
		
		m_uniformData.color = glm::vec4(base_color, 1.0f);
		
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
			m_uniformData.color = glm::vec4(color, 1.0f);
		}
		
		// Explicitly bind UBO to binding point before updating
		glBindBufferBase(GL_UNIFORM_BUFFER, SOLID_BINDING_POINT_IDX, m_uniformBuffer->getRendererID());
		
		// Now update the data
		m_uniformBuffer->updateAllBufferData(&m_uniformData);
		
		// Force flush to ensure data is sent to GPU
		glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBuffer->getRendererID());
		glFlush();
	}
}