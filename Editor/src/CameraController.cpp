#include "CameraController.h"
#include "Scenes/Components/Components.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Initialize static members
Rapture::Entity CameraController::s_cameraEntity;
float CameraController::s_lastMouseX = 0.0f;
float CameraController::s_lastMouseY = 0.0f;
float CameraController::s_mouseSensitivity = 5.0f;
float CameraController::s_moveSpeed = 0.1f;
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
    handleMouseInput(ts);
    handleKeyboardInput(ts);
    
    // Update camera view matrix
    auto& controller_comp = s_cameraEntity.getComponent<Rapture::CameraControllerComponent>();
    controller_comp.camera.updateViewMatrix(controller_comp.translation, controller_comp.cameraFront);
}

void CameraController::handleMouseInput(float ts)
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

    float sensitivity = s_mouseSensitivity * (ts/1000.0f);
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

void CameraController::handleKeyboardInput(float ts)
{
    auto& controller_comp = s_cameraEntity.getComponent<Rapture::CameraControllerComponent>();
    
    // Calculate right vector for strafing
    glm::vec3 right = glm::vec3(controller_comp.cameraFront.z, 0.0f, -controller_comp.cameraFront.x);

    // Only allow movement when mouse is locked
    if (s_isMouseLocked)
    {
        // Handle movement keys - using separate if statements allows for diagonal movement
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveLeft)))
        {
            controller_comp.translation += s_moveSpeed * (glm::vec3(1.0f) * right);
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveRight)))
        {
            controller_comp.translation -= s_moveSpeed * (glm::vec3(1.0f) * right);
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveForward)))
        {
            controller_comp.translation += s_moveSpeed * (glm::vec3(1.0f) * controller_comp.cameraFront);
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveBackward)))
        {
            controller_comp.translation -= s_moveSpeed * (glm::vec3(1.0f) * controller_comp.cameraFront);
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveUp)))
        {
            controller_comp.translation.y += s_moveSpeed;
        }
        
        if (Rapture::Input::isKeyPressed(KeyBindings::getKeyForAction(KeyAction::MoveDown)))
        {
            controller_comp.translation.y -= s_moveSpeed;
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