#pragma once

#include <memory>
#include <functional>
#include "Layers/Layer.h"
#include "Scenes/Scene.h"
#include "CameraController.h"
#include "Renderer/Framebuffer.h"
#include "Mesh/Mesh.h"
#include "Renderer/PrimitiveShapes.h"
#include "Renderer/Deferred Shading/DeferredRenderer.h"
#include "Textures/Texture.h"
#include "Shaders/Shader.h"
#include <vector>
#include "Renderer/DDGI/DynamicDiffuseGI.h"

// Forward declarations
class ViewportPanel;

// Callback types
using EntitySelectedCallback = std::function<void(std::shared_ptr<Rapture::Entity>)>;
using CameraMatricesCallback = std::function<void(const glm::mat4&, const glm::mat4&)>; // view, projection

class TestLayer : public Rapture::Layer
{
public:
	// Define a callback for entity selection changes
	using EntitySelectedCallback = std::function<void(std::shared_ptr<Rapture::Entity>)>;

	TestLayer()
		: Layer("Test Layer")
	{
	    // Scene is now managed by SceneManager, no need to create it here
	}

	void onAttach() override;
	void onDetach() override;

	void onUpdate(float ts) override;
	void onEvent(Rapture::Event& event) override;

    void onNewActiveScene(std::shared_ptr<Rapture::Scene> scene);
    
    // Getter for the framebuffer to use in ImGui viewport
    std::shared_ptr<Rapture::Framebuffer> getFramebuffer() const { 
        return Rapture::DeferredRenderer::getLightingBuffer();
    }
    std::shared_ptr<Rapture::Framebuffer> getMaterialFramebuffer() const { return m_materialViewerFramebuffer; }
    
    // Get material viewer sphere
    std::shared_ptr<Rapture::Sphere> getMaterialViewerSphere() const { return m_materialViewerSphere; }
    
    // Use SceneManager to get the active scene
    std::shared_ptr<Rapture::Scene> getActiveScene() const;
    
    // Set the viewport panel reference
    void setViewportPanel(ViewportPanel* viewportPanel) { m_viewportPanel = viewportPanel; }
    
    // Set the callback for entity selection events
    void setEntitySelectedCallback(EntitySelectedCallback callback) { m_entitySelectedCallback = callback; }
    
    // Get the currently selected entity
    std::shared_ptr<Rapture::Entity> getSelectedEntity() const { return m_selectedEntity; }
    
    // Set the currently selected entity
    void setSelectedEntity(std::shared_ptr<Rapture::Entity> entity);
    
    // Set callback for camera matrices updates (for ImGuizmo)
    void setCameraMatricesCallback(CameraMatricesCallback callback) { m_cameraMatricesCallback = callback; }
    
    // Call to notify about camera changes
    void notifyCameraChange();

private:
    std::shared_ptr<Rapture::Framebuffer> m_framebuffer;
    std::shared_ptr<Rapture::Framebuffer> m_materialViewerFramebuffer;

    std::shared_ptr<Rapture::Mesh> m_testCube;  // Store the test cube to keep it alive
    
    // Camera references
    std::shared_ptr<Rapture::Entity> m_cameraEntity;

    bool m_wasMouseBtnPressedLastFrame = false;
    
    // FPS counter variables
    int m_fpsCounter = 0;
    float m_fpsTimer = 0.0f;
    
    // Raycast visualization
    std::shared_ptr<Rapture::Line> m_debugRayLine;
    float m_rayDisplayTimer = 0.0f;
    bool m_showDebugRay = false;
    
    // Reference to the UI panels for interaction
    ViewportPanel* m_viewportPanel = nullptr;
    
    // Entity selection
    std::shared_ptr<Rapture::Entity> m_selectedEntity;
    EntitySelectedCallback m_entitySelectedCallback;
    
    // Camera matrices callback
    CameraMatricesCallback m_cameraMatricesCallback;

    // Sphere for material viewer
    std::shared_ptr<Rapture::Sphere> m_materialViewerSphere;
    std::shared_ptr<Rapture::Sphere> m_debugProbeSphere;

    std::shared_ptr<Rapture::DynamicDiffuseGI> m_ddgi;

    // Event listener IDs for cleanup
    size_t m_sceneActivatedListenerId = 0;



};
