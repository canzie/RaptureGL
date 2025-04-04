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
#include "Renderer/PrimitiveShapes.h"
#include "Renderer/Raycast.h"
#include "Scenes/Systems/AnimationSystem.h"

#include "File Loaders/glTF/glTF2Loader.h"
#include "Textures/Texture.h"
#include "Debug/Profiler.h"

// Vendor includes
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>

#include "Debug/TracyProfiler.h"

void TestLayer::setSelectedEntity(std::shared_ptr<Rapture::Entity> entity)
{
    // If we had a previous selection, hide its bounding box
    if (m_selectedEntity) {
        Rapture::Renderer::hideBoundingBox(*m_selectedEntity);
    }
    
    m_selectedEntity = entity;
    
    // Show bounding box for the new selection
    if (entity) {
        // Show the bounding box of the selected entity with a distinctive color
        auto* bb = entity->tryGetComponent<Rapture::BoundingBoxComponent>();
        if (bb) bb->isVisible = true;
        
    } else if (m_selectedEntity) {
        auto* bb = m_selectedEntity->tryGetComponent<Rapture::BoundingBoxComponent>();
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


	// Initialize keybindings from config file
	KeyBindings::init("keybindings.cfg");

	
	Rapture::glTF2Loader loader = Rapture::glTF2Loader(m_activeScene);
	//loader.loadModel("adamHead/adamHead.gltf");
	loader.loadModel("Sponza/glTF/Sponza.gltf");

	//loader.loadModel("metalroughspheres/MetalRoughSpheres.gltf");


    //loader.loadModel("Buggy/Buggy.gltf");
    //loader.loadModel("BoxAnimated/BoxAnimated.gltf");
    //loader.loadModel("RiggedSimple/RiggedSimple.gltf");

    loader.loadModel("BrainStem/BrainStem.gltf");
    //loader.loadModel("CesiumMan/CesiumMan.gltf");

	//loader.loadModel("sphere.gltf");
	//loader.loadModel("donut.gltf");
    //loader.loadModel("cube.gltf");

    std::vector<std::string> cubemapPathsSTR = {
        "D:/downloads/skybox/skybox/right.jpg",
        "D:/downloads/skybox/skybox/left.jpg",
        "D:/downloads/skybox/skybox/top.jpg",
        "D:/downloads/skybox/skybox/bottom.jpg",
        "D:/downloads/skybox/skybox/front.jpg", 
        "D:/downloads/skybox/skybox/back.jpg"
    };

    std::vector<std::filesystem::path> cubemapPaths = {
        "D:/downloads/skybox/skybox/right.jpg",
        "D:/downloads/skybox/skybox/left.jpg",
        "D:/downloads/skybox/skybox/top.jpg",
        "D:/downloads/skybox/skybox/bottom.jpg",
        "D:/downloads/skybox/skybox/front.jpg", 
        "D:/downloads/skybox/skybox/back.jpg"
    };


    m_activeScene->getSkyBox().setTexturePaths(cubemapPaths);
    m_showDebugRay = m_activeScene->getSettings().rayCastDebugEnabled;


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
    light1.addComponent<Rapture::SpriteComponent>();
    
    // Light 2: A blue-tinted light to the left side
    Rapture::Entity light2 = m_activeScene->createEntity("Light 2");
    light2.addComponent<Rapture::TransformComponent>(
        glm::vec3(-2.0f, 0.5f, -3.0f), // Position to the left of the sphere, same Z coordinate
        glm::vec3(0.0f),               // No rotation needed for point light
        glm::vec3(0.2f)                // Small scale to make the cube compact
    );
    // Blue-tinted light with medium intensity
    light2.addComponent<Rapture::LightComponent>();
    light2.addComponent<Rapture::SpriteComponent>();


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

void TestLayer::notifyCameraChange()
{
    // Only proceed if we have a callback and valid camera entity
    if (!m_cameraMatricesCallback || !m_cameraEntity || !m_cameraEntity->isValid())
        return;
        
    // Get camera component data
    auto* cameraComponent = m_cameraEntity->tryGetComponent<Rapture::CameraControllerComponent>();
    if (!cameraComponent)
        return;
        
    // Get matrices and call the callback
    glm::mat4 viewMatrix = cameraComponent->camera.getViewMatrix();
    glm::mat4 projMatrix = cameraComponent->camera.getProjectionMatrix();
    m_cameraMatricesCallback(viewMatrix, projMatrix);
}

void TestLayer::onUpdate(float ts)
{
    RAPTURE_PROFILE_FUNCTION();
    RAPTURE_PROFILE_GPU_SCOPE("TestLayer::onUpdate");
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
    
    // Notify about camera changes (for ImGuizmo)
    notifyCameraChange();
    
    // Update animations in the scene
    Rapture::AnimationSystem::updateAnimations(*m_activeScene, timeInSeconds);

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

                // Convert to viewport coordinates
                float viewportMouseX, viewportMouseY;
                bool isInViewport = m_viewportPanel->windowToViewportCoordinates(
                    viewportMouseX, viewportMouseY);
                
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
                                setSelectedEntity(std::make_shared<Rapture::Entity>(hit->entity));
                                
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
    Rapture::Renderer::drawSprites(m_activeScene);

    m_activeScene->onUpdate();

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

                
                // Only handle clicks inside the viewport
                if (m_viewportPanel->isMouseInViewport())
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

