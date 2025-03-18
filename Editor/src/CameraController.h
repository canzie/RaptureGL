#pragma once

#include "Scenes/Entity.h"
#include "Input/Input.h"
#include "Input/KeyBindings.h"
#include <glm/glm.hpp>

class CameraController
{
public:
    static void init(Rapture::Entity cameraEntity);
    static void update(float ts);
    static void setMousePosition(float x, float y);
    
    static void lockMouse();
    static void unlockMouse();
    static bool isMouseLocked() { return s_isMouseLocked; }
    
    // Called when window is clicked to lock the mouse
    static void onWindowClicked();

private:
    static void handleMouseInput(float ts);
    static void handleKeyboardInput(float ts);
    
    static Rapture::Entity s_cameraEntity;
    static float s_lastMouseX;
    static float s_lastMouseY;
    static float s_mouseSensitivity;
    static float s_moveSpeed;
    static bool s_isMouseLocked;
}; 