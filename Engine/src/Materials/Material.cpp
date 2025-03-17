#include "Material.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../logger/Log.h"
#include "../Shaders/OpenGLShaders/OpenGLShader.h"
#include "../Shaders/OpenGLUniforms/OpenGLUniformBuffer.h"

namespace Rapture
{

	Shader* MetalMaterial::s_shader = nullptr;
	Shader* PhongMaterial::s_shader = nullptr;
	UniformBuffer* PhongMaterial::s_UBO = nullptr;
	UniformBuffer* MetalMaterial::s_UBO = nullptr;

	Shader* SolidMaterial::s_shader = nullptr;
	UniformBuffer* SolidMaterial::s_UBO = nullptr;

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



	MetalMaterial::MetalMaterial(glm::vec3 base_color, float roughness, float metallic, float specular)
	{
		if (s_shader == nullptr)
			s_shader = new OpenGLShader("PBR_vs.glsl", "PBR_fs.glsl");
		if (s_UBO == nullptr)
			s_UBO = new OpenGLUniformBuffer(sizeof(PBRUniform), PBR_BINDING_POINT_IDX);

		// debug code, maybe make kak member variable. the renderer needs to set this struct as the buffers content
		//PBRUniform kak = { base_color , roughness, metallic, specular };
		//s_UBO->updateAllBufferData(&kak);
		
		m_uniformData = { base_color , roughness, metallic, specular };

	}


	void MetalMaterial::bindData()
	{
		s_UBO->updateAllBufferData(&m_uniformData);

	}





	PhongMaterial::PhongMaterial(float flux, glm::vec4 diffuseColor, glm::vec4 specularColor, glm::vec4 ambientLight, float shininess)
	{
		if (s_shader == nullptr)
			s_shader = new OpenGLShader("blinn_phong_vs.glsl", "blinn_phong_fs.glsl");
		if (s_UBO == nullptr)
			s_UBO = new OpenGLUniformBuffer(sizeof(PhongUniform), PHONG_BINDING_POINT_IDX);

		// debug code, maybe make kak member variable. the renderer needs to set this struct as the buffers content
		//PhongUniform kak = { ambientLight , diffuseColor, specularColor, flux, shininess };
		//s_UBO->updateAllBufferData(&kak);

		m_uniformData = { ambientLight , diffuseColor, specularColor, flux, shininess };

	}
	void PhongMaterial::bindData()
	{
		s_UBO->updateAllBufferData(&m_uniformData);
	}

	SolidMaterial::SolidMaterial(glm::vec3 base_color)
	{
		if (s_shader == nullptr)
			s_shader = new OpenGLShader("default_vs.glsl", "default_fs.glsl");
		if (s_UBO == nullptr)
			s_UBO = new OpenGLUniformBuffer(sizeof(SolidColorUniform), SOLID_BINDING_POINT_IDX);

		m_uniformData = { glm::vec4(base_color, 1.0f)};
	}

	void SolidMaterial::bindData()
	{
		s_UBO->updateAllBufferData(&m_uniformData);
	}
}