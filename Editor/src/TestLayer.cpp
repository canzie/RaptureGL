#include "TestLayer.h"
#include "Scenes/Entity.h"
#include "Renderer/Renderer.h"
#include "Renderer/OpenGLRendererAPI.h"
#include "Logger/Log.h"
#include "Input/Input.h"
#include "Events/MouseEvents.h"
#include "Input/KeyBindings.h"
#include "Scenes/Components/Components.h"
#include "Mesh/Mesh.h"

#include "File Loaders/glTF/glTF2Loader.h"
#include "Textures/Texture.h"

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
	Rapture::TextureLibrary::init();
    
    Rapture::TextureLibrary::add("albedoMap", Rapture::TextureLibrary::load("./assets/models/adamHead/Assets/Models/PBR/Adam/Textures/Adam_Head_a.jpg"));

	// Initialize keybindings from config file
	KeyBindings::init("keybindings.cfg");

    // Create test cube using our new method (no file loading)
    //m_testCube = Rapture::Mesh::createCube(2.0f);

	// Create the first object (cube)
	//Rapture::Entity cube = m_activeScene->createEntity("Test Cube");
    // Use the constructor that takes a mesh shared_ptr
    //cube.addComponent<Rapture::MeshComponent>("adamHead.gltf");
	
	Rapture::glTF2Loader loader = Rapture::glTF2Loader(m_activeScene);
	loader.loadModel("adamHead/adamHead.gltf");

	//loader.loadModel("sphere.gltf");

	// Add a custom red material to the cube
	// Create a bright red metal material (1,0,0 for RGB red, 0.2 roughness, 0.8 metallic)
	//cube.addComponent<Rapture::MaterialComponent>(glm::vec3(1.0f, 0.0f, 1.0f), 0.2f, 0.8f, 0.5f);
	
	//cube.addComponent<Rapture::TransformComponent>();
	// Position the cube in front of the camera (negative Z moves it further away)

	//Rapture::Entity sph = m_activeScene->createEntity("sdaduiwqhiudahiudh ent");
	//sph.addComponent<Rapture::MeshComponent>();
	//sph.addComponent<Rapture::MaterialComponent>(glm::vec3(0.0f, 0.0f, 1.0f));
    //sph.getComponent<Rapture::MaterialComponent>().material->setTexture("albedoMap", Rapture::TextureLibrary::get("albedoMap"));
    //sph.addComponent<Rapture::TransformComponent>();
    // Create two light entities
    // Light 1: A bright white point light to the right of the model
    Rapture::Entity light1 = m_activeScene->createEntity("Light 1");
    light1.addComponent<Rapture::TransformComponent>(
        glm::vec3(2.0f, 1.0f, -3.0f),  // Position to the right of the sphere, same Z coordinate
        glm::vec3(0.0f),              // No rotation needed for point light
        glm::vec3(0.2f)               // Small scale to make the cube compact
    );
    // White light with high intensity
    light1.addComponent<Rapture::LightComponent>(
        glm::vec3(1.0f, 1.0f, 1.0f),  // Pure white color
        1.2f,                         // High intensity
        10.0f                         // Range
    );
    // Add a cube mesh to visualize the light
    light1.addComponent<Rapture::MeshComponent>();
    light1.getComponent<Rapture::MeshComponent>().mesh = Rapture::Mesh::createCube();
    // Create an emissive material for the light
    light1.addComponent<Rapture::MaterialComponent>(glm::vec3(1.0f, 1.0f, 1.0f));  // White emissive material
    
    // Light 2: A blue-tinted light to the left side
    Rapture::Entity light2 = m_activeScene->createEntity("Light 2");
    light2.addComponent<Rapture::TransformComponent>(
        glm::vec3(-2.0f, 0.5f, -3.0f), // Position to the left of the sphere, same Z coordinate
        glm::vec3(0.0f),               // No rotation needed for point light
        glm::vec3(0.2f)                // Small scale to make the cube compact
    );
    // Blue-tinted light with medium intensity
    light2.addComponent<Rapture::LightComponent>(
        glm::vec3(0.2f, 0.4f, 1.0f),  // Blue-tinted color
        1.0f,                         // Medium intensity
        8.0f                          // Range
    );
    // Add a cube mesh to visualize the light
    light2.addComponent<Rapture::MeshComponent>();
    light2.getComponent<Rapture::MeshComponent>().mesh = Rapture::Mesh::createCube();
    // Create an emissive material that matches the light color
    light2.addComponent<Rapture::MaterialComponent>(glm::vec3(0.2f, 0.4f, 1.0f));  // Blue emissive material

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


	// Update the camera controller
	CameraController::update(ts);

	// Bind the framebuffer to render the scene to a texture
	m_framebuffer->bind();
	

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
