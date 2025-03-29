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
    // Helper method to find the root entity by traversing up the hierarchy
    std::shared_ptr<Rapture::Entity> findRootEntity(std::shared_ptr<Rapture::Entity> entity, Rapture::Scene* scene);
    
    // Helper function to display an entity and its children recursively
    void displayEntityHierarchy(std::shared_ptr<Rapture::Entity> entity, int depth, Rapture::Scene* scene, 
                              std::unordered_set<uint32_t>& displayedEntities);
    
    // Builds the cached hierarchy from scratch
    void buildHierarchyCache(Rapture::Scene* scene);
    
    // Display entities from the cached hierarchy
    void displayCachedHierarchy(const std::shared_ptr<HierarchyNode>& node, int depth, Rapture::Scene* scene);
    
    // Helper function that returns the node rectangle for line drawing
    ImRect displayCachedHierarchyWithRect(const std::shared_ptr<HierarchyNode>& node, int depth, Rapture::Scene* scene);
    
    // Currently selected entity
    std::shared_ptr<Rapture::Entity> m_selectedEntity;
    
    // Callback for entity selection
    EntitySelectionCallback m_entitySelectionCallback;
    
    // Cached hierarchy data
    std::vector<std::shared_ptr<HierarchyNode>> m_independentEntities;
    std::vector<std::shared_ptr<HierarchyNode>> m_rootEntities;
    
    // Scene handle for comparison to detect scene changes
    Rapture::Scene* m_cachedScene = nullptr;
    
    // Flag to force hierarchy rebuild
    bool m_needsHierarchyRebuild = true;
    
    // Entity count for scene modification detection
    uint32_t m_lastEntityCount = 0;
    
    // Frame counter for periodic updates (in case entities are added/removed)
    uint32_t m_frameCounter = 0;
    const uint32_t CACHE_UPDATE_INTERVAL = 60; // Update every 60 frames
};

