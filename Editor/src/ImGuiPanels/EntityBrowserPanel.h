#pragma once

#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "Scenes/Components/Components.h"
#include "TestLayer.h"

#include <functional>
#include <vector>
#include <string>
#include <unordered_set>

#include "imgui.h"


class EntityBrowserPanel {
public:
    // Define the callback type for entity selection
    using EntitySelectionCallback = std::function<void(Rapture::Entity&)>;

    EntityBrowserPanel() = default;
    ~EntityBrowserPanel() = default;

    void render(Rapture::Scene* scene, EntitySelectionCallback callback);
    
    
    // Get the currently selected entity
    Rapture::Entity getSelectedEntity() const { return m_selectedEntity; }
    
    // Check if an entity is selected
    bool hasSelectedEntity() const { return m_selectedEntity; }

private:
    // Helper method to find the root entity by traversing up the hierarchy
    entt::entity findRootEntity(entt::entity entityHandle, Rapture::Scene* scene);
    
    // Helper function to display an entity and its children recursively
    void displayEntityHierarchy(entt::entity entityHandle, int depth, Rapture::Scene* scene, 
                              std::unordered_set<entt::entity>& displayedEntities);
    
    // Currently selected entity
    Rapture::Entity m_selectedEntity;
    
    // Callback for entity selection
    EntitySelectionCallback m_entitySelectionCallback;
};

