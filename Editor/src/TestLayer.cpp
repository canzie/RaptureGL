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
#include "ImGuiPanels/ViewportPanel.h"

#include "File Loaders/glTF/glTF2Loader.h"
#include "File Loaders/ModelLoader.h"
#include "Textures/Texture.h"
#include "Debug/Profiler.h"
#include "Renderer/Raycast.h"

void TestLayer::setSelectedEntity(Rapture::Entity entity)
{
    // If we had a previous selection, hide its bounding box
    if (m_selectedEntity) {
        Rapture::Renderer::hideBoundingBox(m_selectedEntity);
    }
    
    m_selectedEntity = entity;
    
    // Show bounding box for the new selection
    if (entity) {
        // Show the bounding box of the selected entity with a distinctive color
        auto* bb = entity.tryGetComponent<Rapture::BoundingBoxComponent>();
        if (bb) bb->isVisible = true;
        
    } else if (m_selectedEntity) {
        auto* bb = m_selectedEntity.tryGetComponent<Rapture::BoundingBoxComponent>();
        if (bb) bb->isVisible = false;
    }
    
    // Call the callback if one is set
    if (m_entitySelectedCallback) {
        m_entitySelectedCallback(entity);
    }
}

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
	//Rapture::TextureLibrary::init();
    

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
    
  
    //loader.loadModel("Sponza/glTF/Sponza.gltf");

	loader.loadModel("sphere.gltf");
	loader.loadModel("donut.gltf");

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
    //light1.addComponent<Rapture::MeshComponent>();
    //light1.getComponent<Rapture::MeshComponent>().mesh = Rapture::Mesh::createCube();
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
    //light2.addComponent<Rapture::MeshComponent>();
    //light2.getComponent<Rapture::MeshComponent>().mesh = Rapture::Mesh::createCube();
    // Create an emissive material that matches the light color
    light2.addComponent<Rapture::MaterialComponent>(glm::vec3(0.2f, 0.4f, 1.0f));  // Blue emissive material

	// Create camera controller
	Rapture::Entity camera_controller = m_activeScene->createEntity("Camera Controller");
	camera_controller.addComponent<Rapture::CameraControllerComponent>(60.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    m_cameraEntity = std::make_shared<Rapture::Entity>(camera_controller);
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

    if (Rapture::Input::isMouseBtnPressed(0))
    {
        m_wasMouseBtnPressedLastFrame = true;
    }

    if (Rapture::Input::isMouseBtnReleased(0) && m_wasMouseBtnPressedLastFrame)
    {
        m_wasMouseBtnPressedLastFrame = false;
            // Make sure we have valid framebuffer and camera
            if (m_framebuffer && m_cameraEntity && m_viewportPanel)
            {
                // Get global window mouse position
                float windowMouseX = Rapture::Input::getMousePos().first;
                float windowMouseY = Rapture::Input::getMousePos().second;
                
                // Convert to viewport coordinates
                float viewportMouseX, viewportMouseY;
                bool isInViewport = m_viewportPanel->windowToViewportCoordinates(
                    windowMouseX, windowMouseY, viewportMouseX, viewportMouseY);
                
                // Only process clicks inside the viewport
                if (isInViewport)
                {
                    float width = static_cast<float>(m_framebuffer->getSpecification().width);
                    float height = static_cast<float>(m_framebuffer->getSpecification().height);
                    
                    
                    // Get camera position (ray origin)
                    const auto& cameraComponent = m_cameraEntity->getComponent<Rapture::CameraControllerComponent>();
                    glm::mat4 viewMatrix = cameraComponent.camera.getViewMatrix();
                    glm::mat4 invView = glm::inverse(viewMatrix);
                    glm::vec3 cameraPosition = glm::vec3(invView[3]);
                    
                    // Generate ray direction
                    glm::vec3 rayDirection = Rapture::Raycast::screenToWorldRay(
                        viewportMouseX, viewportMouseY,
                        width, height,
                        cameraComponent.camera.getProjectionMatrix(),
                        viewMatrix);
                    
                    // Normalize ray direction (important!)
                    rayDirection = glm::normalize(rayDirection);
                    
                    if (m_showDebugRay) {
                        // Create a debug ray that extends far into the scene
                        const float RAY_LENGTH = 100.0f; // Arbitrary large distance
                        glm::vec3 rayEnd = cameraPosition + rayDirection * RAY_LENGTH;
                        
                        // Create and store the debug ray line
                        m_debugRayLine = std::make_shared<Rapture::Line>(
                            cameraPosition,    // Start at camera position
                            rayEnd,            // End at a point far along the ray direction
                            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)  // Red color
                        );
                        
                        // Set the timer to show the ray for 2 seconds
                        m_showDebugRay = true;
                    }
                    
                    // Queue the raycast with a callback
                    Rapture::Raycast::queueRaycast(
                        viewportMouseX, 
                        viewportMouseY,
                        width, height,
                        m_activeScene.get(),
                        cameraComponent.camera.getProjectionMatrix(),
                        viewMatrix,
                        [this, cameraPosition, rayDirection](const std::optional<Rapture::RaycastHit>& hit) {
                            // This callback will be called when the raycast is processed
                            if (hit.has_value()) {
                                Rapture::GE_INFO("Queued raycast hit entity with ID: {}", hit->entity.getID());

                                // Set this entity as the selected entity
                                setSelectedEntity(hit->entity);
                                
                                // Update the debug ray line to end at the hit point
                                if (m_debugRayLine && m_showDebugRay) {
                                    // Create a new debug ray that extends to the hit point
                                    m_debugRayLine = std::make_shared<Rapture::Line>(
                                        cameraPosition,    // Start at camera position
                                        hit->hitPoint,     // End at the hit point
                                        glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green color for hits
                                    );
                                    // Reset timer to ensure it stays visible
                                    m_rayDisplayTimer = 2.0f;
                                }
                            } else {
                                Rapture::GE_INFO("Queued raycast did not hit any entity");
                            }
                        }
                    );
                }
            }
            else
        {
            Rapture::GE_ERROR("Cannot perform raycast - framebuffer or camera is null");
        }
    }
    

	// Bind the framebuffer to render the scene to a texture
	m_framebuffer->bind();
	

	// Render the scene to the framebuffer
	Rapture::Renderer::sumbitScene(m_activeScene);
    
    // Draw the debug ray if active
    if (m_showDebugRay && m_debugRayLine) {
        Rapture::Renderer::drawLine(*m_debugRayLine);
    }

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
            // Only capture the mouse if it's inside the viewport
            if (m_viewportPanel)
            {
                float windowMouseX = Rapture::Input::getMousePos().first;
                float windowMouseY = Rapture::Input::getMousePos().second;
                
                // Only handle clicks inside the viewport
                if (m_viewportPanel->isMouseInViewport(windowMouseX, windowMouseY))
                {
                    CameraController::onWindowClicked();
                    m_wasMouseBtnPressedLastFrame = true;
                }
            }
            else
            {
                // Fallback if viewport panel is not available
                CameraController::onWindowClicked();
                m_wasMouseBtnPressedLastFrame = true;
            }
        }
    }
}


