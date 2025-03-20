//#include <glm/glm.hpp>

#include "../../Materials/Material.h"
#include "../../Materials/MaterialLibrary.h"
#include "../../Mesh/Mesh.h"
#include "../../Camera/PerspectiveCamera.h"
#include<glm/gtc/quaternion.hpp>

#include <vector>
//#include <string>

namespace Rapture {


	struct TransformComponent
	{
		glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
		//glm::quat rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };

		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
	};

	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
		
		MeshComponent(std::string fname)
		{
			mesh = std::make_shared<Mesh>(fname);
            GE_CORE_INFO("Loading mesh with glTF Loader: {0}, submesh count: {1}", fname, mesh->getSubMeshes().size());
		}
        
        MeshComponent(std::string fname, bool useGLTF2)
        {
            // TODO: Implement glTF2 loading
            mesh = std::make_shared<Mesh>(fname);
            GE_CORE_INFO("Loading mesh with glTF2 Loader: {0}, submesh count: {1}", fname, mesh->getSubMeshes().size());
        }
        
        // Constructor that takes a mesh shared_ptr
        MeshComponent(const std::shared_ptr<Mesh>& existingMesh) 
        {
            mesh = existingMesh;
            GE_CORE_INFO("Created MeshComponent with existing mesh, submesh count: {0}", 
                mesh ? mesh->getSubMeshes().size() : 0);
        }
        
        // Constructor that takes a raw mesh pointer (for backward compatibility)
        MeshComponent(Mesh* existingMesh) 
        {
            if (existingMesh) {
                mesh = std::shared_ptr<Mesh>(existingMesh, [](Mesh*){});  // Non-owning shared_ptr
                GE_CORE_WARN("Created MeshComponent with raw pointer, consider using shared_ptr instead");
            }
            GE_CORE_INFO("Created MeshComponent with existing mesh, submesh count: {0}", 
                mesh ? mesh->getSubMeshes().size() : 0);
        }

		MeshComponent() 
		{
			mesh = std::make_shared<Mesh>("Cube.gltf");
		}
	};

	struct MaterialComponent
	{
		std::shared_ptr<Material> material;
        std::string materialName;

		MaterialComponent()
		{
			// Create a default metal material with dark gray color
			material = MaterialLibrary::createPBRMaterial(
				"DefaultMaterial",
				glm::vec3(0.04768598f, 0.05147058f, 0.05068756f), // Base color
				0.0f,  // Roughness
				0.0f,  // Metallic
				0.2f   // Specular
			);
            materialName = "DefaultMaterial";
		}

        MaterialComponent(glm::vec3 base_color)
        {
            material = MaterialLibrary::createSolidMaterial(
                "SolidMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this)),
                base_color
            );
            materialName = "SolidMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this));
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
            materialName = "CustomMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(this));
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
		void setBaseColor(const glm::vec3& color)
		{
			if (material)
			{
				// For PBR materials, the parameter name is typically "baseColor"
				if (material->getType() == MaterialType::PBR)
				{
					material->setVec3("baseColor", color);
				}
				// For solid materials, it might be called "color"
				else if (material->getType() == MaterialType::SOLID)
				{
					material->setVec3("color", color);
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
				material->setVec3("baseColor", baseColor);
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
					return material->getParameter("baseColor").asVec3();
				}
				else if (material->getType() == MaterialType::SOLID && material->hasParameter("color"))
				{
					return material->getParameter("color").asVec3();
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


}