//#include <glm/glm.hpp>

#pragma once

#include "../../Materials/Material.h"
#include "../../Materials/MaterialLibrary.h"
#include "../../Mesh/Mesh.h"
#include "../../Camera/PerspectiveCamera.h"
#include "../../Scenes/EntityNode.h"
#include "Transforms.h"
#include "BoundingBox.h"
#include "../../Debug/Profiler.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
//#include <string>

namespace Rapture {


	struct TransformComponent
	{
		Transforms transforms;
        
        glm::vec3 translation() const { return transforms.getTranslation(); }
        glm::vec3 rotation() const { return transforms.getRotation(); }
        glm::vec3 scale() const { return transforms.getScale(); }
        glm::mat4 transformMatrix() const { return transforms.getTransform(); }

        TransformComponent()
        {
            transforms = Transforms();
        }

        TransformComponent(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale) {
            transforms = Transforms(translation, rotation, scale);

        }

        // Add constructor for quaternion rotation
        TransformComponent(glm::vec3 translation, glm::quat rotation, glm::vec3 scale) {
            transforms = Transforms(translation, rotation, scale);
        }

        TransformComponent(glm::mat4 transformMatrix) {
            transforms.setTransform(transformMatrix);
        }
	};

	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
        bool isLoading = false;
		
		MeshComponent(std::string fname)
		{
			mesh = std::make_shared<Mesh>(fname);
            isLoading = true;
            GE_CORE_INFO("Loading mesh with glTF Loader: {0}", fname);
		}
        
        MeshComponent(std::string fname, bool useGLTF2)
        {
            // TODO: Implement glTF2 loading
            mesh = std::make_shared<Mesh>(fname);
            isLoading = true;
            GE_CORE_INFO("Loading mesh with glTF2 Loader: {0}", fname);
        }
        
        // Constructor that takes a mesh shared_ptr
        MeshComponent(std::shared_ptr<Mesh> meshPtr) : mesh(meshPtr), isLoading(false) {}
        
        MeshComponent(bool isEmpty)
		{
			if (isEmpty)
			{
				mesh = std::make_shared<Mesh>();
                isLoading = true;
			}
            else
            {
                MeshComponent();
            }
		}

		MeshComponent()
		{
			mesh = Mesh::createCube();
		}

	};
    
    // Note: BoundingBoxComponent is now defined in BoundingBox.h
    
	struct MaterialComponent
	{
		std::shared_ptr<Material> material;
        std::string materialName;

		MaterialComponent()
		{
			// Create a default metal material with dark gray color
			material = MaterialLibrary::createPBRMaterial(
				"DefaultMaterial",
				glm::vec3(1.0f, 0.0f, 1.0f), // Base color
				0.0f,  // Roughness
				0.0f,  // Metallic
				0.2f   // Specular
			);
            materialName = material->getName();
		}

        MaterialComponent(glm::vec3 base_color)
        {
            material = MaterialLibrary::createSolidMaterial(
                "SolidMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this)),
                base_color
            );
            materialName = material->getName();
        }

		
		MaterialComponent(glm::vec3 base_color, float roughness, float metallic, float specular)
		{
			// Create a custom PBR material with the provided parameters
			material = MaterialLibrary::createPBRMaterial(
				"CustomMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this)), 
				base_color, 
				roughness, 
				metallic, 
				specular
			);
            materialName = material->getName();
		}
		
		MaterialComponent(const std::string& materialName)
		{
			// Use an existing material from the library
			material = MaterialLibrary::getMaterial(materialName);
            this->materialName = materialName;
		}
		
		~MaterialComponent() 
		{
			// No need to manually delete the material as it's now managed by shared_ptr
		}
		
		// Methods to modify material properties after creation
		
		// Set base color (works for both PBR and Solid materials)
		void setBaseColor(const glm::vec4& color)
		{
			if (material)
			{
				// For PBR materials, the parameter name is typically "baseColor"
				if (material->getType() == MaterialType::PBR)
				{
					material->setVec4("baseColor", color);
				}
				// For solid materials, it might be called "color"
				else if (material->getType() == MaterialType::SOLID)
				{
					material->setVec4("color", color);
				}
			}
		}
		
		// Set roughness (PBR materials only)
		void setRoughness(float roughness)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setFloat("roughness", roughness);
			}
		}
		
		// Set metallic value (PBR materials only)
		void setMetallic(float metallic)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setFloat("metallic", metallic);
			}
		}
		
		// Set specular value (PBR materials only)
		void setSpecular(float specular)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setFloat("specular", specular);
			}
		}
		
		// Additional helper to change all PBR properties at once
		void setPBRProperties(const glm::vec3& baseColor, float roughness, float metallic, float specular)
		{
			if (material && material->getType() == MaterialType::PBR)
			{
				material->setVec4("baseColor", glm::vec4(baseColor, 1.0f));
				material->setFloat("roughness", roughness);
				material->setFloat("metallic", metallic);
				material->setFloat("specular", specular);
			}
		}
		
		// Get current material properties
		glm::vec3 getBaseColor() const
		{
			if (material)
			{
				if (material->getType() == MaterialType::PBR && material->hasParameter("baseColor"))
				{
					return glm::vec3(material->getParameter("baseColor").asVec4());
				}
				else if (material->getType() == MaterialType::SOLID && material->hasParameter("color"))
				{
					return glm::vec3(material->getParameter("color").asVec4());
				}
			}
			return glm::vec3(0.0f);
		}
		
		float getRoughness() const
		{
			if (material && material->getType() == MaterialType::PBR && material->hasParameter("roughness"))
			{
				return material->getParameter("roughness").asFloat();
			}
			return 0.0f;
		}
		
		float getMetallic() const
		{
			if (material && material->getType() == MaterialType::PBR && material->hasParameter("metallic"))
			{
				return material->getParameter("metallic").asFloat();
			}
			return 0.0f;
		}
		
		float getSpecular() const
		{
			if (material && material->getType() == MaterialType::PBR && material->hasParameter("specular"))
			{
				return material->getParameter("specular").asFloat();
			}
			return 0.0f;
		}
	};


	struct CameraControllerComponent
	{
		PerspectiveCamera camera;

		float fov;
		float aspect_ratio;
		float near_plane;
		float far_plane;

		glm::vec3 translation;
		glm::vec3 cameraFront;

		float yaw;
		float pitch;

		glm::vec3 rotation_axis;
		float rotation_angle;

		CameraControllerComponent(float fovy, float AR, float nplane, float fplane)
		{
			fov = fovy;
			aspect_ratio = AR;
			near_plane = nplane;
			far_plane = fplane;
			camera = PerspectiveCamera(fovy, AR, nplane, fplane);


			rotation_angle = 0.0f;
			rotation_axis = { 1.0f, 0.0f, 0.0f };
			translation = { 0.0f, 0.0f, -3.0f };
			cameraFront = { 0.0f, 0.0f, 1.0f };

			yaw = -90.0f;
			pitch = 0.0f;

			camera.updateViewMatrix(translation);


		}

	};

    struct EntityNodeComponent
    {
        std::shared_ptr<EntityNode> entity_node;

        EntityNodeComponent() = default;

        EntityNodeComponent(std::shared_ptr<Entity> entity)
        {
            entity_node = std::make_shared<EntityNode>(entity);
        }

        EntityNodeComponent(std::shared_ptr<Entity> entity, std::shared_ptr<EntityNode> parent)
        {
            entity_node = std::make_shared<EntityNode>(entity, parent);
        }
        
        // Add constructors that accept Entity directly
        EntityNodeComponent(Entity entity)
        {
            entity_node = std::make_shared<EntityNode>(std::make_shared<Entity>(entity));
        }

        EntityNodeComponent(Entity entity, std::shared_ptr<EntityNode> parent)
        {
            entity_node = std::make_shared<EntityNode>(std::make_shared<Entity>(entity), parent);
        }

        ~EntityNodeComponent()
        {
        }
        
        
    };

    struct TagComponent
    {
        std::string tag;

        TagComponent(const std::string& tag) : tag(tag) {}
        
    };

    // Light types for the LightComponent
    enum class LightType
    {
        Point = 0,
        Directional = 1,
        Spot = 2
    };

    struct LightComponent
    {
        LightType type = LightType::Point;
        glm::vec3 color = glm::vec3(1.0f);    // Light color (default: white)
        float intensity = 1.0f;               // Light intensity multiplier
        
        // For point and spot lights
        float range = 10.0f;                  // Attenuation range
        
        // For spot lights only
        float innerConeAngle = glm::radians(30.0f); // Inner cone angle in radians
        float outerConeAngle = glm::radians(45.0f); // Outer cone angle in radians
        
        // Flag indicating if the light is active
        bool isActive = true;
        
        // Constructors
        LightComponent() = default;
        
        // Constructor for point light
        LightComponent(const glm::vec3& color, float intensity, float range)
            : type(LightType::Point), color(color), intensity(intensity), range(range) {}
        
        // Constructor for directional light
        LightComponent(const glm::vec3& color, float intensity)
            : type(LightType::Directional), color(color), intensity(intensity) {}
        
        // Constructor for spot light
        LightComponent(const glm::vec3& color, float intensity, float range, 
                      float innerAngleDegrees, float outerAngleDegrees)
            : type(LightType::Spot), color(color), intensity(intensity), range(range),
              innerConeAngle(glm::radians(innerAngleDegrees)), 
              outerConeAngle(glm::radians(outerAngleDegrees)) {}
    };

}