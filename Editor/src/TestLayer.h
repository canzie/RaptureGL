#pragma once

#include "Layers/Layer.h"
#include "Scenes/Components/Components.h"
#include "Renderer/Framebuffer.h"
#include "CameraController.h"

class TestLayer : public Rapture::Layer
{
public:
	virtual void onAttach() override;
	virtual void onDetach() override;

	virtual void onUpdate(float ts) override;
	virtual void onEvent(Rapture::Event& event) override;

private:
	std::shared_ptr<Rapture::Framebuffer> m_framebuffer = std::make_shared<Rapture::Framebuffer>();
	std::shared_ptr<Rapture::Scene> m_activeScene = std::make_shared<Rapture::Scene>();
};
