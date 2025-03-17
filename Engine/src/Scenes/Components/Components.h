//#include <glm/glm.hpp>

#include "../../Materials/Material.h"
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
		}


		MeshComponent() 
		{
			mesh = new Mesh("Cube.gltf");
		}
	};

	struct MaterialComponent
	{
		MetalMaterial* metalMat;

		MaterialComponent()
		{
			//metalMat = new PhongMaterial(3.0f, { 0.25f, 0.5f, 0.2f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.1f, 0.1f, 0.1f, 1.0f }, 40.0f);
			metalMat = new MetalMaterial( { 0.04768598f, 0.05147058f, 0.05068756f }, 0.0f, 0.0f, 0.2f);
		}

		
		MaterialComponent(glm::vec3 base_color, float roughness, float metallic, float specular)
		{
			metalMat = new MetalMaterial(base_color, roughness, metallic, specular);
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