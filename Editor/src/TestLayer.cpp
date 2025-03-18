#include "TestLayer.h"
#include "Scenes/Entity.h"
#include "Renderer/Renderer.h"
#include "Renderer/OpenGLRendererAPI.h"
#include "Logger/Log.h"
#include "Input/Input.h"
#include "Events/MouseEvents.h"
#include "Input/KeyBindings.h"

void TestLayer::onAttach()
{
	// Initialize keybindings from config file
	KeyBindings::init("keybindings.cfg");

	Rapture::Entity cube = m_activeScene->createEntity("cube ent");
	cube.addComponent<Rapture::MeshComponent>("adamhead.gltf");
	glm::vec3 tst1 = { 0.8f, 0.9f, 0.4f };
	cube.addComponent<Rapture::MaterialComponent>(tst1, 1.0f-0.843f, 0.0f, 0.8308824f);
	cube.addComponent<Rapture::TransformComponent>();
	//cube.getComponent<Rapture::TransformComponent>().scale = { 2.0f, 2.00f, 2.00f };


	Rapture::Entity spher = m_activeScene->createEntity("sphere ent");
	//spher.addComponent<Rapture::MeshComponent>("sphere.gltf");
	glm::vec3 tst = { 1.0f, 0.0f, 0.0f };
	spher.addComponent<Rapture::MaterialComponent>(tst, 0.13f, 0.0f, 0.0f);
	spher.addComponent<Rapture::TransformComponent>();
	//spher.getComponent<Rapture::TransformComponent>().translation.x = 3.0f;
	//spher.getComponent<Rapture::TransformComponent>().rotation.x = 0.5f;
	//spher.getComponent<Rapture::TransformComponent>().scale = {0.5f, 0.5f, 0.5f};
	

	Rapture::Entity camera_controller = m_activeScene->createEntity("Camera Controller");
	camera_controller.addComponent<Rapture::CameraControllerComponent>(60.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
	
	// Initialize the camera controller
	CameraController::init(camera_controller);
	
	// Initialize with current mouse position
	auto pos = Rapture::Input::getMousePos();
	CameraController::setMousePosition(pos.first, pos.second);
}

void TestLayer::onDetach()
{

}

void TestLayer::onUpdate(float ts)
{
	
	auto& reg = m_activeScene->getRegistry();
	auto tra = reg.view<Rapture::TransformComponent>();
	for (auto ent : tra)
	{
		Rapture::Entity mesh(ent, m_activeScene.get());


		//mesh.getComponent<Rapture::TransformComponent>().rotation.y += 1.0f * ts / 1000.0f;
	}

	// Update the camera controller
	CameraController::update(ts);

	//m_framebuffer->bind();

	Rapture::OpenGLRendererAPI::setClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
	Rapture::OpenGLRendererAPI::clear();

	Rapture::Renderer::sumbitScene(m_activeScene);

	//m_framebuffer->unBind();
}

void TestLayer::onEvent(Rapture::Event& event)
{
    // Handle mouse button pressed events
    if (event.getEventType() == Rapture::EventType::MouseBtnPressed)
    {
        Rapture::MouseButtonPressedEvent& mouseEvent = static_cast<Rapture::MouseButtonPressedEvent&>(event);
        
        // Mouse button 0 is usually left mouse button
        if (mouseEvent.getMouseButton() == 0)
        {
            CameraController::onWindowClicked();
        }
    }
}
