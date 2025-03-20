#pragma once

#include <memory>
#include "Layers/Layer.h"
#include "Scenes/Scene.h"
#include "CameraController.h"
#include "Renderer/Framebuffer.h"
#include "Mesh/Mesh.h"

class TestLayer : public Rapture::Layer
{
public:
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

private:
	std::shared_ptr<Rapture::Scene> m_activeScene;
    std::shared_ptr<Rapture::Framebuffer> m_framebuffer;
    std::shared_ptr<Rapture::Mesh> m_testCube;  // Store the test cube to keep it alive
    
    // FPS counter variables
    int m_fpsCounter = 0;
    float m_fpsTimer = 0.0f;
};
