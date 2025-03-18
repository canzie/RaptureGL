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
		Mesh* mesh;
		
		MeshComponent(std::string fname)
		{
            
			mesh = new Mesh(fname);
            GE_CORE_INFO("Loading mesh: {0}, submesh count: {1}", fname, mesh->getSubMeshes().size());
		}


		MeshComponent() 
		{
			mesh = new Mesh("Cube.gltf");
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