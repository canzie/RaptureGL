#pragma once

#include <memory>
#include <functional>
#include "Layers/Layer.h"
#include "Scenes/Scene.h"
#include "CameraController.h"
#include "Renderer/Framebuffer.h"
#include "Mesh/Mesh.h"
#include "Renderer/PrimitiveShapes.h"

// Forward declaration
class ViewportPanel;

class TestLayer : public Rapture::Layer
{
public:
	// Define a callback for entity selection changes
	using EntitySelectedCallback = std::function<void(Rapture::Entity)>;

	TestLayer()
		: Layer("Test Layer")
	{
		m_activeScene = std::make_shared<Rapture::Scene>();
	}

	void onAttach() override;
	void onDetach() override;

	void onUpdate(float ts) override;
	void onEvent(Rapture::Event& event) override;
    
    // Getter for the framebuffer to use in ImGui viewport
    std::shared_ptr<Rapture::Framebuffer> getFramebuffer() const { return m_framebuffer; }
    std::shared_ptr<Rapture::Scene> getActiveScene() const { return m_activeScene; }
    
    // Set the viewport panel reference
    void setViewportPanel(ViewportPanel* viewportPanel) { m_viewportPanel = viewportPanel; }
    
    // Set the callback for entity selection events
    void setEntitySelectedCallback(EntitySelectedCallback callback) { m_entitySelectedCallback = callback; }
    
    // Get the currently selected entity
    Rapture::Entity getSelectedEntity() const { return m_selectedEntity; }
    
    // Set the currently selected entity
    void setSelectedEntity(Rapture::Entity entity);

private:
	std::shared_ptr<Rapture::Scene> m_activeScene;
    std::shared_ptr<Rapture::Framebuffer> m_framebuffer;
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
    
    // Reference to the viewport panel for coordinate conversion
    ViewportPanel* m_viewportPanel = nullptr;
    
    // Entity selection
    Rapture::Entity m_selectedEntity;
    EntitySelectedCallback m_entitySelectedCallback;
};
