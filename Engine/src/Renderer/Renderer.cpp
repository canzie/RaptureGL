#include "Renderer.h"
#include "../Scenes/Components/Components.h"
#include "OpenGLRendererAPI.h"
#include <glm/gtx/transform.hpp>
#include "../Logger/Log.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../Materials/MaterialUniformLayouts.h"
#include "glad/glad.h"

namespace Rapture
{

	std::shared_ptr<UniformBuffer> Renderer::s_cameraUBO = nullptr;
	std::shared_ptr<UniformBuffer> Renderer::s_lightsUBO = nullptr;

	void Renderer::init()
	{
		s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Dynamic, nullptr, BASE_BINDING_POINT_IDX);
		s_cameraUBO->bindBase();
		
		// Create lights uniform buffer with the lights binding point
		s_lightsUBO = std::make_shared<UniformBuffer>(sizeof(LightsUniform), BufferUsage::Dynamic, nullptr, LIGHTS_BINDING_POINT_IDX);
		s_lightsUBO->bindBase();
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
		auto lights = reg.view<TransformComponent, LightComponent>();

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
		
		// Create lights uniform data
		LightsUniform lightsData;
        memset(&lightsData, 0, sizeof(LightsUniform)); // Clear the data first
		
		// Count active lights (up to MAX_LIGHTS)
		uint32_t lightCount = 0;
		for (auto entityID : lights)
		{
			if (lightCount >= MAX_LIGHTS) break;
			
			Entity lightEntity(entityID, s.get());
			TransformComponent& transform = lightEntity.getComponent<TransformComponent>();
			LightComponent& light = lightEntity.getComponent<LightComponent>();
			
			if (!light.isActive) continue;
			
			// Fill light data
			LightData& lightData = lightsData.lights[lightCount];
			
			// Position and type
			lightData.position = glm::vec4(transform.translation(), static_cast<float>(light.type));
			
			// Color and intensity
			lightData.color = glm::vec4(light.color, light.intensity);
			
			// Direction (for directional/spot lights) and range
			if (light.type == LightType::Directional || light.type == LightType::Spot)
			{
				// Convert Euler angles to direction vector
				glm::vec3 euler = transform.rotation();
				glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), euler.z, glm::vec3(0, 0, 1)) *
					               glm::rotate(glm::mat4(1.0f), euler.y, glm::vec3(0, 1, 0)) *
					               glm::rotate(glm::mat4(1.0f), euler.x, glm::vec3(1, 0, 0));
				
				glm::vec3 direction = glm::vec3(rotMat * glm::vec4(0, 0, -1, 0)); // Forward vector
				lightData.direction = glm::vec4(direction, light.range);
			}
			else
			{
				lightData.direction = glm::vec4(0.0f, 0.0f, 0.0f, light.range);
			}
			
			// Cone angles for spot lights
			if (light.type == LightType::Spot)
			{
				lightData.coneAngles = glm::vec4(light.innerConeAngle, light.outerConeAngle, 0.0f, 0.0f);
			}
			else
			{
				lightData.coneAngles = glm::vec4(0.0f);
			}
			
			lightCount++;
		}
		
		lightsData.lightCount = lightCount;
		
		// Ensure lights buffer is bound before setting data
		s_lightsUBO->bind();
		s_lightsUBO->bindBase();
		s_lightsUBO->setData(&lightsData, sizeof(LightsUniform));
		s_lightsUBO->unbind();
		
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
            

			MaterialComponent& mat = mesh.getComponent<MaterialComponent>();
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
			glm::mat4 goober = glm::mat4(1.0f);
			auto ibo = meshe->getVAO()->getIndexBuffer();
			
			if (!ibo) {
				GE_CORE_ERROR("Null index buffer found on mesh VAO");
				continue;
			}
			

				// Calculate combined transform with parent hierarchy
				goober = mesh.getComponent<TransformComponent>().transformMatrix();
				//goober = calculateCombinedTransform(mesh);

				shdr->setMat4("u_model", goober);
				
				error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_CORE_ERROR("OpenGL error after setting u_model: {0} (0x{1:x})", error, error);
					continue;
				}

				OpenGLRendererAPI::drawIndexed(meshe->getIndexCount(), ibo->getIndexType(), meshe->getOffsetBytes());
				
				error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_CORE_ERROR("OpenGL error after drawIndexed: {0} (0x{1:x})", error, error);
				}
			
			
			// Unbind material after rendering this entity
			material->unbind();
            
            // Unbind VAO too for clean state management
            meshe->getVAO()->unbind();
		}
	}
}