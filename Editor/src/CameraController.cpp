#include "CameraController.h"
#include "Scenes/Components/Components.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Initialize static members
Rapture::Entity CameraController::s_cameraEntity;
float CameraController::s_lastMouseX = 0.0f;
float CameraController::s_lastMouseY = 0.0f;
float CameraController::s_mouseSensitivity = 0.2f;
float CameraController::s_moveSpeed = 5.0f;  // Units per second instead of per frame
bool CameraController::s_isMouseLocked = true;

void CameraController::init(Rapture::Entity cameraEntity)
{
    s_cameraEntity = cameraEntity;
}

void CameraController::setMousePosition(float x, float y)
{
    s_lastMouseX = x;
    s_lastMouseY = y;
}

void CameraController::lockMouse()
{
    if (!s_isMouseLocked)
    {
        s_isMouseLocked = true;
        Rapture::Input::disableMouseCursor();
        
        // Save current mouse position to prevent camera jump when locking
        auto pos = Rapture::Input::getMousePos();
        s_lastMouseX = pos.first;
        s_lastMouseY = pos.second;
    }
}

void CameraController::unlockMouse()
{
    if (s_isMouseLocked)
    {
        s_isMouseLocked = false;
        Rapture::Input::enableMouseCursor();
    }
}

void CameraController::onWindowClicked()
{
    lockMouse();
}

void CameraController::update(float ts)
{
    // Ensure time is in seconds
    float deltaTime = ts;
    if (deltaTime > 0.1f) { // Likely in milliseconds
        deltaTime *= 0.001f; // Convert to seconds
    }
    
    // Clamp delta time to avoid huge jumps when framerate drops very low
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }
    
    handleMouseInput(deltaTime);
    handleKeyboardInput(deltaTime);
    
    // Update camera view matrix
    auto& controller_comp = s_cameraEntity.getComponent<Rapture::CameraControllerComponent>();
    controller_comp.camera.updateViewMatrix(controller_comp.translation, controller_comp.cameraFront);
}

void CameraController::handleMouseInput(float deltaTime)
{
    // Only process mouse movement when mouse is locked
    if (!s_isMouseLocked)
        return;
        
    auto& controller_comp = s_cameraEntity.getComponent<Rapture::CameraControllerComponent>();
    auto pos = Rapture::Input::getMousePos();

    float xoffset = pos.first - s_lastMouseX;
    float yoffset = s_lastMouseY - pos.second;
    s_lastMouseX = pos.first;
    s_lastMouseY = pos.second;

    // Mouse sensitivity is now in degrees per second
    float sensitivity = s_mouseSensitivity;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    controller_comp.yaw += xoffset;
    controller_comp.pitch += yoffset;

    // Clamp pitch to avoid flipping
    if (controller_comp.pitch > 89.0f)
        controller_comp.pitch = 89.0f;
    if (controller_comp.pitch < -89.0f)
        controller_comp.pitch = -89.0f;

    // Update camera front direction
    glm::vec3 front;
    front.x = cos(glm::radians(controller_comp.yaw)) * cos(glm::radians(controller_comp.pitch));
    front.y = sin(glm::radians(controller_comp.pitch));
    front.z = sin(glm::radians(controller_comp.yaw)) * cos(glm::radians(controller_comp.pitch));
    controller_comp.cameraFront = glm::normalize(front);
}

void CameraController::handleKeyboardInput(float deltaTime)
{
    auto& controller_comp = s_cameraEntity.getComponent<Rapture::CameraControllerComponent>();
    
    // Calculate right vector for strafing
    glm::vec3 right = glm::normalize(glm::vec3(controller_comp.cameraFront.z, 0.0f, -controller_comp.cameraFront.x));

    // Only allow movement when mouse is locked
    if (s_isMouseLocked)
    {
        // Calculate actual movement distance based on speed and delta time
        float moveDistance = s_moveSpeed * deltaTime;
        
        // Handle movement keys - using separate if statements allows for diagonal movement
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveLeft)))
        {
            controller_comp.translation += moveDistance * right;
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveRight)))
        {
            controller_comp.translation -= moveDistance * right;
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveForward)))
        {
            controller_comp.translation += moveDistance * controller_comp.cameraFront;
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveBackward)))
        {
            controller_comp.translation -= moveDistance * controller_comp.cameraFront;
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveUp)))
        {
            controller_comp.translation.y += moveDistance;
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveDown)))
        {
            controller_comp.translation.y -= moveDistance;
        }
    }
    
    // Handle escape key to unlock mouse
    if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::UnlockCamera)))
    {
        unlockMouse();
    }
    
    // 1 key now locks the mouse for testing (can be removed later)
    if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::LockCamera)))
    {
        lockMouse();
    }
} 