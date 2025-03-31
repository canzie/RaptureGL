#pragma once
#include <glm/glm.hpp>

namespace Rapture
{
	// Define bit flags for PBR texture presence
	namespace PBRTextureFlags {
		constexpr uint32_t ALBEDO_MAP    = (1 << 0);  // 0x01
		constexpr uint32_t NORMAL_MAP    = (1 << 1);  // 0x02
		constexpr uint32_t METALLIC_MAP  = (1 << 2);  // 0x04
		constexpr uint32_t ROUGHNESS_MAP = (1 << 3);  // 0x08
		constexpr uint32_t AO_MAP        = (1 << 4);  // 0x10
		constexpr uint32_t EMISSIVE_MAP  = (1 << 5);  // 0x20
		constexpr uint32_t HEIGHT_MAP    = (1 << 6);  // 0x40
	}

	// This structure should match the PBR uniform block in PBR_fs.glsl
	// std140 layout requires careful alignment (vec3 often needs padding)
	struct PBRUniform
	{
		alignas(16) glm::vec4 baseColorFactor;
		alignas(4) float metallicFactor;
		alignas(4) float roughnessFactor;
		alignas(4) float specularFactor;
		alignas(4) uint32_t flags;
	};



	// This structure should match the Phong uniform block in blinn_phong_fs.glsl
	struct PhongUniform
	{
		alignas(4) float flux;
		alignas(16) glm::vec4 diffuseColor;
		alignas(16) glm::vec4 specularColor;
		alignas(16) glm::vec4 ambientLight;
		alignas(4) float shininess;
	};

	// This structure should match the SOLID uniform block in default_fs.glsl
	struct SolidColorUniform
	{
		alignas(16) glm::vec4 baseColorFactor;
	};

	// This structure should match the KHR_SpecularGlossiness uniform block
	struct KHR_SpecularGlossiness_Uniform
	{
		glm::vec4 ambientLight;   // vec4 for alignment
		glm::vec4 diffuseColor;   // vec4 for alignment
		glm::vec4 specularColor;  // vec4 for alignment
		float flux;
		float shininess;
	};

	// New struct for Specular-Glossiness materials
	struct SpecularGlossinessUniform
	{
		alignas(16) glm::vec4 diffuseFactor;
		alignas(16) glm::vec4 specularFactor; // RGB is specular color, A is glossiness factor
		alignas(4) float flags;
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
    
	struct BoneMatricesUniform
	{
		alignas(16) glm::mat4 u_BoneTransforms[100];
	};
    
}