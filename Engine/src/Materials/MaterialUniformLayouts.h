#pragma once
#include <glm/glm.hpp>

namespace Rapture
{
	// This structure should match the PBR uniform block in PBR_fs.glsl
	// std140 layout requires careful alignment (vec3 often needs padding)
	struct PBRUniform
	{
		alignas(16) glm::vec4 baseColorFactor;
		alignas(16) float metallicFactor;
		alignas(16) float roughnessFactor;
		alignas(16) float specularFactor;
		alignas(16) float flags;
		alignas(16) char padding[44]; // pad to 96 bytes
	};

	// This structure should match the Phong uniform block in blinn_phong_fs.glsl
	struct PhongUniform
	{
		alignas(16) float flux;
		alignas(16) glm::vec4 diffuseColor;
		alignas(16) glm::vec4 specularColor;
		alignas(16) glm::vec4 ambientLight;
		alignas(16) float shininess;
		alignas(16) char padding[60]; // pad to 96 bytes
	};

	// This structure should match the SOLID uniform block in default_fs.glsl
	struct SolidColorUniform
	{
		alignas(16) glm::vec4 baseColorFactor;
		alignas(16) char padding[80]; // pad to 96 bytes
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

	// New struct for Specular-Glossiness materials
	struct SpecularGlossinessUniform
	{
		alignas(16) glm::vec4 diffuseFactor;
		alignas(16) glm::vec4 specularFactor; // RGB is specular color, A is glossiness factor
		alignas(16) float flags;
		alignas(16) char padding[60]; // pad to 96 bytes
	};

	// Maximum number of lights supported in the shader
	#define MAX_LIGHTS 8

	// Individual light data for the shader
	struct LightData
	{
		alignas(16) glm::vec4 position;     // xyz = position, w = type (0=point, 1=directional, 2=spot)
		alignas(16) glm::vec4 color;        // xyz = color, w = intensity
		alignas(16) glm::vec4 direction;    // xyz = direction (for spot/directional), w = range
		alignas(16) glm::vec4 coneAngles;   // x = innerConeAngle, y = outerConeAngle (for spot lights)
	};

	// Light uniform buffer layout
	struct LightsUniform
	{
		alignas(16) uint32_t lightCount;    // Number of active lights
		alignas(16) LightData lights[MAX_LIGHTS];
	};
}