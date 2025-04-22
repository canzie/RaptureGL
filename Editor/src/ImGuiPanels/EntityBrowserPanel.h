#pragma once

#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "Scenes/Components/Components.h"
#include "TestLayer.h"

#include <functional>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h" // For ImRect

// Forward declaration
class HierarchyNode;

// Hierarchy node to cache entity hierarchy data
class HierarchyNode {
public:
    HierarchyNode(std::shared_ptr<Rapture::Entity> entity, const std::string& name) 
        : entity(entity), entityName(name) {}
    
    std::shared_ptr<Rapture::Entity> entity;
    std::string entityName;
    std::vector<std::shared_ptr<HierarchyNode>> children;
};

class EntityBrowserPanel {
public:
    // Define the callback type for entity selection
    using EntitySelectionCallback = std::function<void(std::shared_ptr<Rapture::Entity>)>;

    EntityBrowserPanel() = default;
    ~EntityBrowserPanel() = default;

    void render(Rapture::Scene* scene, EntitySelectionCallback callback);
    
    // Get the currently selected entity
    std::shared_ptr<Rapture::Entity> getSelectedEntity() const { return m_selectedEntity; }
    
    // Check if an entity is selected
    bool hasSelectedEntity() const { return m_selectedEntity != nullptr; }
    
    // Force rebuild of hierarchy cache
    void refreshHierarchyCache() { m_needsHierarchyRebuild = true; }

private:
    // Builds the cached hierarchy from scratch
    void buildHierarchyCache(Rapture::Scene* scene);

    // Recursively renders a row in the hierarchy table
    void renderHierarchyRow(const std::shared_ptr<HierarchyNode>& node, int depth, int& rowIndex);
    
    // Currently selected entity
    std::shared_ptr<Rapture::Entity> m_selectedEntity;
    
    // Callback for entity selection
    EntitySelectionCallback m_entitySelectionCallback;
    
    // Combined list of root nodes for the hierarchy (includes previously independent entities)
    std::vector<std::shared_ptr<HierarchyNode>> m_hierarchyRoots;
    
    // Scene handle for comparison to detect scene changes
    Rapture::Scene* m_cachedScene = nullptr;
    
    // Flag to force hierarchy rebuild
    bool m_needsHierarchyRebuild = true;
    
    // Entity count for scene modification detection
    uint32_t m_lastEntityCount = 0;
};

