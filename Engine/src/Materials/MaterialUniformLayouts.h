#pragma once
#include <glm/glm.hpp>

namespace Rapture
{
	// This structure should match the PBR uniform block in PBR_fs.glsl
	// std140 layout requires careful alignment (vec3 often needs padding)
	struct PBRUniform
	{
		glm::vec3 base_color;
		float roughness;
		float metallic;
		float specular;
		float padding[2]; // Ensure 16-byte alignment
	};

	// This structure should match the Phong uniform block in blinn_phong_fs.glsl
	struct PhongUniform
	{
		glm::vec4 ambientLight;   // vec4 for alignment
		glm::vec4 diffuseColor;   // vec4 for alignment
		glm::vec4 specularColor;  // vec4 for alignment
		float flux;
		float shininess;
		float padding[2]; // Ensure 16-byte alignment
	};

	// This structure should match the SOLID uniform block in default_fs.glsl
	struct SolidColorUniform
	{
		glm::vec4 color; // vec4 for alignment
	};

	// This structure should match the KHR_SpecularGlossiness uniform block
	struct KHR_SpecularGlossiness_Uniform
	{
		glm::vec4 ambientLight;   // vec4 for alignment
		glm::vec4 diffuseColor;   // vec4 for alignment
		glm::vec4 specularColor;  // vec4 for alignment
		float flux;
		float shininess;
		float padding[2]; // Ensure 16-byte alignment
	};
}