#include <glm/glm.hpp>

namespace Rapture
{


	struct PBRUniform
	{
		glm::vec3 base_color;
		float roughness;
		float metallic;
		float specular;
	};

	struct PhongUniform
	{
		glm::vec4 ambientLight;
		glm::vec4 diffuseColor;
		glm::vec4 specularColor;
		float flux;
		float shininess;
	};


	struct SolidColorUniform
	{
		glm::vec4 color;
	};

	struct KHR_SpecularGlossiness_Uniform
	{
		glm::vec4 ambientLight;
		glm::vec4 diffuseColor;
		glm::vec4 specularColor;
		float flux;
		float shininess;
	};

}