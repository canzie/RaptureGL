#pragma once

#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "Scenes/Components/Components.h"
#include "Scenes/Components/BoundingBox.h"
#include "ImGuiPanels/EntityBrowserPanel.h"
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"


class PropertiesPanel {
public:
    PropertiesPanel() = default;
    ~PropertiesPanel() = default;

    // Original method that gets entity from EntityBrowserPanel
    void render(Rapture::Entity entity);
    void render();

private:
    bool positionLocked = false;
    bool rotationLocked = false;
    bool scaleLocked = false;
    glm::vec3 lastScale = {1.0f, 1.0f, 1.0f};
    
    // For texture preview
    std::string selectedTextureName;
    
    // Light component editor state
    int selectedLightType = 0;
    
    void drawMaterialTextures(Rapture::Entity entity);
    const char* getLightTypeString(int type);
    
    // Helper method to render entity properties
    void renderEntityProperties(Rapture::Entity entity);
};

