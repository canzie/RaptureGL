#include "TestLayer.h"
#include "Scenes/Entity.h"
#include "Renderer/Renderer.h"
#include "Renderer/OpenGLRendererAPI.h"
#include "Logger/Log.h"
#include "Input/Input.h"
#include "Events/MouseEvents.h"
#include "Input/KeyBindings.h"
#include "Scenes/Components/Components.h"

void TestLayer::onAttach()
{
    // Initialize the framebuffer with proper specs
    Rapture::FramebufferSpecification fbSpec;
    fbSpec.width = 1920;
    fbSpec.height = 1080;
    fbSpec.attachments = { 
        Rapture::FramebufferTextureFormat::RGBA8,        // Color attachment
        Rapture::FramebufferTextureFormat::DEPTH24STENCIL8  // Depth attachment
    };
    m_framebuffer = Rapture::Framebuffer::create(fbSpec);

    // Initialize the material library
	Rapture::MaterialLibrary::init();
	
    
	// Initialize keybindings from config file
	KeyBindings::init("keybindings.cfg");


	// Create the first object (cube)
	Rapture::Entity cube = m_activeScene->createEntity("cube ent");
	cube.addComponent<Rapture::MeshComponent>("adamhead.gltf");
	
	// Add a custom red material to the cube
	// Create a bright red metal material (1,0,0 for RGB red, 0.2 roughness, 0.8 metallic)
	cube.addComponent<Rapture::MaterialComponent>(glm::vec3(1.0f, 0.0f, 0.0f), 0.2f, 0.8f, 0.5f);
	
	cube.addComponent<Rapture::TransformComponent>();
	//cube.getComponent<Rapture::TransformComponent>().scale = { 2.0f, 2.00f, 2.00f };



	Rapture::Entity sph = m_activeScene->createEntity("sdaduiwqhiudahiudh ent");
	sph.addComponent<Rapture::MeshComponent>("sphere.gltf");
	sph.addComponent<Rapture::MaterialComponent>(glm::vec3(0.0f, 0.0f, 1.0f), 0.2f, 0.8f, 0.5f);
	sph.addComponent<Rapture::TransformComponent>();

    

	//spher.getComponent<Rapture::TransformComponent>().translation.x = 3.0f;
	//spher.getComponent<Rapture::TransformComponent>().rotation.x = 0.5f;
	//spher.getComponent<Rapture::TransformComponent>().scale = {0.5f, 0.5f, 0.5f};
	

	// Create camera controller
	Rapture::Entity camera_controller = m_activeScene->createEntity("Camera Controller");
	camera_controller.addComponent<Rapture::CameraControllerComponent>(60.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
	
	// Initialize the camera controller
	CameraController::init(camera_controller);
	
	// Initialize with current mouse position
	auto pos = Rapture::Input::getMousePos();
	CameraController::setMousePosition(pos.first, pos.second);
    
    // Initialize FPS counter variables
    m_fpsCounter = 0;
    m_fpsTimer = 0.0f;
}

void TestLayer::onDetach()
{

}

void TestLayer::onUpdate(float ts)
{
    // Ensure time is in seconds
    float timeInSeconds = ts;
    
    // Convert milliseconds to seconds if needed
    if (timeInSeconds > 0.1f) { // Likely in milliseconds
        timeInSeconds *= 0.001f;
    }
    
    // Update FPS counter
    m_fpsCounter++;
    m_fpsTimer += timeInSeconds;
    
    // Log FPS approximately once per second
    if (m_fpsTimer >= 1.0f) {
        float fps = static_cast<float>(m_fpsCounter) / m_fpsTimer;
        Rapture::GE_CORE_INFO("FPS: {0:.1f}", fps);
        
        // Reset counters
        m_fpsCounter = 0;
        m_fpsTimer = 0.0f;
    }
	
	auto& reg = m_activeScene->getRegistry();
	auto tra = reg.view<Rapture::TransformComponent>();
	for (auto ent : tra)
	{
		Rapture::Entity mesh(ent, m_activeScene.get());
		// Uncomment and use this to see the animation working
		// mesh.getComponent<Rapture::TransformComponent>().rotation.y += 1.0f * ts / 1000.0f;
	}

	// Update the camera controller
	CameraController::update(ts);

	// Bind the framebuffer to render the scene to a texture
	m_framebuffer->bind();
	
	// The framebuffer bind() now handles clearing, so we don't need these calls
	// Rapture::OpenGLRendererAPI::setClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
	// Rapture::OpenGLRendererAPI::clear();

	// Render the scene to the framebuffer
	Rapture::Renderer::sumbitScene(m_activeScene);

	// Unbind the framebuffer to return to the default framebuffer
	m_framebuffer->unBind();
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
