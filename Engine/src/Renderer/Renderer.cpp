#include "Renderer.h"
#include "../Scenes/Components/Components.h"
#include "OpenGLRendererAPI.h"
#include <glm/gtx/transform.hpp>
#include "../Logger/Log.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "glad/glad.h"

namespace Rapture
{

	std::shared_ptr<UniformBuffer> Renderer::s_cameraUBO = nullptr;

	void Renderer::init()
	{
		s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Dynamic, nullptr, BASE_BINDING_POINT_IDX);
		s_cameraUBO->bindBase();
	}

	void Renderer::sumbitScene(const std::shared_ptr<Scene> s)
	{

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            GE_CORE_ERROR("OpenGL error in renderer at sumbitScene 0: {0} (0x{1:x})", error, error);
        }

		auto& reg = s->getRegistry();
		auto meshes = reg.view<TransformComponent, MeshComponent>();
		auto cams = reg.view<CameraControllerComponent>();

		Entity camera_ent(cams.front(), s.get());
		CameraControllerComponent& controller_comp = camera_ent.getComponent<CameraControllerComponent>();

		// Create properly aligned camera uniform data
		CameraUniform cameraData;
		cameraData.projection_mat = controller_comp.camera.getProjectionMatrix();
		cameraData.view_mat = controller_comp.camera.getViewMatrix();
		
		// Ensure buffer is bound before setting data
		s_cameraUBO->bind();
        
        error = glGetError();
        if (error != GL_NO_ERROR) {
            GE_CORE_ERROR("OpenGL error after binding UBO: {0} (0x{1:x})", error, error);
        }
        
		s_cameraUBO->bindBase();
        
        error = glGetError();
        if (error != GL_NO_ERROR) {
            GE_CORE_ERROR("OpenGL error after bindBase UBO: {0} (0x{1:x})", error, error);
        }
        
		s_cameraUBO->setData(&cameraData, sizeof(CameraUniform));

        error = glGetError();
        if (error != GL_NO_ERROR) {
            GE_CORE_ERROR("OpenGL error after setData UBO: {0} (0x{1:x})", error, error);
            // Print info about the buffer
            GE_CORE_ERROR("  UBO ID: {0}, Size: {1}, BindingPoint: {2}", 
                s_cameraUBO->getID(), sizeof(CameraUniform), s_cameraUBO->getBindingPoint());
        }

        // Unbind to ensure clean state
        s_cameraUBO->unbind();
        
        error = glGetError();
        if (error != GL_NO_ERROR) {
            GE_CORE_ERROR("OpenGL error after unbinding UBO: {0} (0x{1:x})", error, error);
        }

		for (auto ent : meshes)
		{
            
            // In EnTT, entity 0 is not null, but entt::null is FFFFFFFF
            if ((uint32_t)ent == 0xFFFFFFFF) {
                GE_CORE_ERROR("Entity is null (0xFFFFFFFF), skipping");
                continue;
            }
            
			Entity mesh(ent, s.get());
            
            if (!mesh.hasComponent<MeshComponent>()) {
                GE_CORE_ERROR("Entity doesn't have MeshComponent, skipping");
                continue;
            }
            
            MeshComponent& meshComp = mesh.getComponent<MeshComponent>();
			auto meshe = meshComp.mesh;
            
            // Add proper null checking for mesh and vao
            if (!meshe) {
                GE_CORE_ERROR("Null mesh encountered during rendering - entity ID: {0:x}", (uint32_t)ent);
                continue;
            }
            
            // Check if the mesh has a valid VAO
            auto vao = meshe->getVAO();
            if (!vao) {
                GE_CORE_ERROR("Mesh has null VAO - entity ID: {0:x}", (uint32_t)ent);
                continue;
            }
            
            // Verify submeshes
            auto& submeshes = meshe->getSubMeshes();
            if (submeshes.empty()) {
                GE_CORE_ERROR("Mesh has no submeshes - entity ID: {0:x}", (uint32_t)ent);
                continue;
            }
            

			MaterialComponent& mat = mesh.getComponent<MaterialComponent>();
			TransformComponent t_comp = mesh.getComponent<TransformComponent>();
			
			auto& material = mat.material;
			if (!material) {
				GE_CORE_WARN("Renderer: Entity has no valid material assigned");
				continue;
			}
			
			
			// Bind the VAO
			vao->bind();

            error = glGetError();
            if (error != GL_NO_ERROR) {
                GE_CORE_ERROR("OpenGL error after VAO bind: {0} (0x{1:x})", error, error);
                continue;  // Skip this mesh if VAO binding failed
            }

			// Bind the material (which also binds the shader)
			material->bind();
            
            error = glGetError();
            if (error != GL_NO_ERROR) {
                GE_CORE_ERROR("OpenGL error after material bind: {0} (0x{1:x})", error, error);
                
				// Add more detailed error checking to identify the problem
				Shader* shdr = material->getShader();
				if (!shdr) {
					GE_CORE_ERROR("Material has null shader!");
				} 
				
				auto ubo = material->getUniformBuffer();
				if (!ubo) {
					GE_CORE_ERROR("Material has null uniform buffer!");
				} 
				
                continue;
            }

			glm::mat4 rot_mat = glm::rotate(t_comp.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f))*
								glm::rotate(t_comp.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f))*
								glm::rotate(t_comp.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
			
			//glm::mat4 rot_mat = glm::mat4_cast(t_comp.rotation);
			glm::mat4 scale_mat = glm::scale(t_comp.scale);
			glm::mat4 tran_mat = glm::translate(t_comp.translation);
			glm::mat4 model_mat = tran_mat * rot_mat * scale_mat;

			Shader* shdr = material->getShader();
			if (!shdr) {
				GE_CORE_ERROR("Material shader pointer is null!");
				continue;
			}
            
            // Set the camera position uniform
			auto popper = controller_comp.translation;
			popper.z = -popper.z;

			shdr->setVec3("u_camPos", popper);
			
			error = glGetError();
			if (error != GL_NO_ERROR) {
				GE_CORE_ERROR("OpenGL error after setting u_camPos: {0} (0x{1:x})", error, error);
				continue;
			}

			// Verify the index buffer is valid
			glm::mat4 goober;
			auto ibo = meshe->getVAO()->getIndexBuffer();
			
			if (!ibo) {
				GE_CORE_ERROR("Null index buffer found on mesh VAO");
				continue;
			}
			

			for (auto& submesh : meshe->getSubMeshes())
			{

				goober = model_mat * submesh->getTransform();
				shdr->setMat4("u_model", goober);
				
				error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_CORE_ERROR("OpenGL error after setting u_model: {0} (0x{1:x})", error, error);
					continue;
				}

				OpenGLRendererAPI::drawIndexed(submesh->getIndexCount(), ibo->getIndexType(), submesh->getOffset());
				
				error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_CORE_ERROR("OpenGL error after drawIndexed: {0} (0x{1:x})", error, error);
				}
			}
			
			// Unbind material after rendering this entity
			material->unbind();
            
            // Unbind VAO too for clean state management
            meshe->getVAO()->unbind();
		}
	}
}