
#include "../Shaders/Shader.h"
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "../Shaders/UniformBuffer.h"
#include "MaterialUniformLayouts.h"

namespace Rapture {

	enum class MaterialFlagBitLocations {
		TRANSPARENT=0,
		OCCLUSION=1
		// add more on the go
	};

	//static std::shared_ptr<OpenGLShader> metalShader = std::make_shared<OpenGLShader>(OpenGLShader("test.vs", "test.fs"));

	class Material {

	public:
		//Material(std::shared_ptr<Shader> shader, char flags);
		//Material() = default;

		virtual void bindData()=0;


		//bool setAttrib(std::string key, float value);

		virtual Shader* getShader() = 0;
		//virtual std::map<std::string, float>& getUniformAtrribs() = 0;


	protected:
		std::map<std::string, float> m_uniform_attr;
		// 8 flags
		char m_render_flags=0;

	};




	class MetalMaterial : public Material {
	
	public:
		MetalMaterial()=default;
		MetalMaterial(glm::vec3 base_color, float roughness, float metallic, float specular);

		virtual Shader* getShader() override final { return s_shader; }
		virtual std::map<std::string, float>& getUniformAtrribs() { return m_uniform_attr; }

		virtual void bindData() override;

	protected:
		PBRUniform m_uniformData;
		static Shader* s_shader;
		static UniformBuffer* s_UBO;

	};



	class PhongMaterial : public Material {

	public:
		PhongMaterial() = default;
		PhongMaterial(float flux, glm::vec4 diffuseColor, glm::vec4 specularColor, glm::vec4 ambientLight, float shininess);

		virtual Shader* getShader() override final { return s_shader; }

		virtual void bindData() override;


	protected:
		PhongUniform m_uniformData;
		static Shader* s_shader;
		static UniformBuffer* s_UBO;


	};

	class SolidMaterial : public Material {

	public:
		SolidMaterial() = default;
		SolidMaterial(glm::vec3 base_color);

		virtual Shader* getShader() override final { return s_shader; }

		virtual void bindData() override;


	protected:
		SolidColorUniform m_uniformData;
		static Shader* s_shader;
		static UniformBuffer* s_UBO;


	};


	class CopperMaterial : public MetalMaterial {
		CopperMaterial();
	};
}
