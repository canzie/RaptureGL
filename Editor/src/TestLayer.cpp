#include "TestLayer.h"
#include "Scenes/Entity.h"
#include "Renderer/Renderer.h"
#include "Renderer/OpenGLRendererAPI.h"
#include "logger/Log.h"
#include "Input/Input.h"

void TestLayer::onAttach()
{
	

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


	auto cams = reg.view<Rapture::CameraControllerComponent>();
	Rapture::Entity camera_ent(cams.front(), m_activeScene.get());

	Rapture::CameraControllerComponent& controller_comp = camera_ent.getComponent<Rapture::CameraControllerComponent>();


	auto pos = Rapture::Input::getMousePos();

	float xoffset = pos.first - lastX;
	float yoffset = lastY - pos.second;
	lastX = pos.first;
	lastY = pos.second;

	float sensitivity = 5.0f * (ts/1000.0f);
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	controller_comp.yaw += xoffset;
	controller_comp.pitch += yoffset;

	if (controller_comp.pitch > 89.0f)
		controller_comp.pitch = 89.0f;
	if (controller_comp.pitch < -89.0f)
		controller_comp.pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(controller_comp.yaw)) * cos(glm::radians(controller_comp.pitch));
	front.y = sin(glm::radians(controller_comp.pitch));
	front.z = sin(glm::radians(controller_comp.yaw)) * cos(glm::radians(controller_comp.pitch));
	controller_comp.cameraFront = glm::normalize(front);

	glm::vec3 rot = glm::vec3(controller_comp.cameraFront.z, 0.0f, -controller_comp.cameraFront.x);

	if (Rapture::Input::isKeyPressed(65))// Dd
	{
		controller_comp.translation += 0.1f*(glm::vec3(1.0f) * rot);
	}
	else if (Rapture::Input::isKeyPressed(68)) // Aa
	{
		controller_comp.translation -= 0.1f*(glm::vec3(1.0f) * rot);
	}
	else if (Rapture::Input::isKeyPressed(87)) // Ww
	{
		controller_comp.translation += 0.1f*(glm::vec3(1.0f) * controller_comp.cameraFront);
	}
	else if (Rapture::Input::isKeyPressed(83)) // Ss
	{
		controller_comp.translation -= 0.1f*(glm::vec3(1.0f) * controller_comp.cameraFront);
	}
	else if (Rapture::Input::isKeyPressed(32)) // Space
	{
		controller_comp.translation.y += 0.1f;
	}
	else if (Rapture::Input::isKeyPressed(340)) // Shift
	{
		controller_comp.translation.y -= 0.1f;
	}
	else if (Rapture::Input::isKeyPressed(256)) // esc
	{
		Rapture::Input::disableMouseCursor();
	}
	else if (Rapture::Input::isKeyPressed(49)) // 1
	{
		Rapture::Input::enableMouseCursor();
	}


	controller_comp.camera.updateViewMatrix(controller_comp.translation, controller_comp.cameraFront);

	
	//m_framebuffer->bind();

	Rapture::OpenGLRendererAPI::setClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
	Rapture::OpenGLRendererAPI::clear();




	Rapture::Renderer::sumbitScene(m_activeScene);

	//m_framebuffer->unBind();


}

void TestLayer::onEvent(Rapture::Event& event)
{

}
