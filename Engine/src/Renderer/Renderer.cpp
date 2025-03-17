#include "Renderer.h"
#include "../Scenes/Components/Components.h"
#include "OpenGLRendererAPI.h"
#include <glm/gtx/transform.hpp>
#include "../Logger/Log.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../Shaders/OpenGLUniforms/OpenGLUniformBuffer.h"

namespace Rapture
{

	UniformBuffer* Renderer::s_cameraUBO = nullptr;

	void Renderer::init()
	{
		s_cameraUBO = new OpenGLUniformBuffer(sizeof(CameraUniform), BASE_BINDING_POINT_IDX);
	}

	void Renderer::sumbitScene(const std::shared_ptr<Scene> s)
	{

		auto& reg = s->getRegistry();
		auto meshes = reg.view<TransformComponent, MeshComponent>();
		auto cams = reg.view<CameraControllerComponent>();

		Entity camera_ent(cams.front(), s.get());
		CameraControllerComponent& controller_comp = camera_ent.getComponent<CameraControllerComponent>();

		CameraUniform kak = { controller_comp.camera.getProjectionMatrix(), controller_comp.camera.getViewMatrix() };
		s_cameraUBO->updateAllBufferData(&kak);


		for (auto ent : meshes)
		{
			Entity mesh(ent, s.get());

			//std::shared_ptr<VertexArray> vao = mesh.getComponent<MeshComponent>().mesh->GetVAO();
			Mesh* meshe = mesh.getComponent<MeshComponent>().mesh;

			MaterialComponent& mat = mesh.getComponent<MaterialComponent>();
			TransformComponent t_comp = mesh.getComponent<TransformComponent>();
			Shader* shdr = mat.metalMat->getShader();

			meshe->getVAO()->bind();
			shdr->bind();
			mat.metalMat->bindData();

			// 
			
			glm::mat4 rot_mat = glm::rotate(t_comp.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f))*
								glm::rotate(t_comp.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f))*
								glm::rotate(t_comp.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
			
			//glm::mat4 rot_mat = glm::mat4_cast(t_comp.rotation);
			glm::mat4 scale_mat = glm::scale(t_comp.scale);
			glm::mat4 tran_mat = glm::translate(t_comp.translation);
			glm::mat4 model_mat = tran_mat * rot_mat * scale_mat;


			//shdr->setUniformMat4f("u_model", model_mat);
			auto popper = controller_comp.translation;
			popper.z = -popper.z;
			shdr->setUniformVec3f("u_camPos", popper);

			
			glm::mat4 goober;


			auto ibo = meshe->getVAO()->getIndexBuffer();

			for (auto& submesh : meshe->getSubMeshes())
			{

				goober = model_mat * submesh->getTransform();
				shdr->setUniformMat4f("u_model", goober);
				OpenGLRendererAPI::drawIndexed(submesh->getIndexCount(), ibo->getIndexType(), submesh->getOffset());
				
			}


			
		}

	}

}