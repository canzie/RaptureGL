#pragma once

#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "Scenes/Components/Components.h"
#include "TestLayer.h"

#include <functional>
#include <vector>
#include <string>

#include "imgui.h"


class EntityBrowserPanel {
public:
    EntityBrowserPanel() = default;
    ~EntityBrowserPanel() = default;

    void render(TestLayer* testLayer);
    
    // Get the currently selected entity
    entt::entity getSelectedEntity() const { return _selectedEntity; }
    
    // Check if an entity is selected
    bool hasSelectedEntity() const { return _selectedEntity != entt::null; }

private:
    // Helper function to display an entity and its children recursively
    void displayEntityHierarchy(entt::entity entityHandle, int depth, Rapture::Scene* scene);
    
    // Currently selected entity
    entt::entity _selectedEntity = entt::null;
};

