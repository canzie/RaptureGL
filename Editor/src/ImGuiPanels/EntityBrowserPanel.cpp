#include "EntityBrowserPanel.h"
#include "Logger/Log.h"
#include "Debug/TracyProfiler.h"

void EntityBrowserPanel::render(Rapture::Scene* scene, EntitySelectionCallback callback) {
    RAPTURE_PROFILE_FUNCTION();
    
    ImGui::Begin("Entity Browser");
    m_entitySelectionCallback = callback;
    
    if (scene) {
        auto& registry = scene->getRegistry();
        
        // Get count of entities with tag components
        auto view = registry.view<Rapture::TagComponent>();
        uint32_t entityCount = static_cast<uint32_t>(view.size());
        
        // Count total entities
        ImGui::Text("Total Entities: %d", entityCount);
        
        // Refresh button
        if (ImGui::Button("Refresh Hierarchy")) {
            m_needsHierarchyRebuild = true;
        }
        
        ImGui::SameLine();
        

        
        ImGui::Separator();
        
        // Check if we need to rebuild hierarchy cache
        bool sceneChanged = (m_cachedScene != scene);
        bool entityCountChanged = (m_lastEntityCount != entityCount);
        
        if (sceneChanged || entityCountChanged || m_needsHierarchyRebuild) {
            RAPTURE_PROFILE_SCOPE("Rebuild Hierarchy Cache");
            buildHierarchyCache(scene);
            m_cachedScene = scene;
            m_lastEntityCount = entityCount;
            m_needsHierarchyRebuild = false;
        }
        
        // Increment frame counter
        m_frameCounter++;
        
        // Display hierarchy from cache
        // Section 1: Independent Entities (no relationships)
        if (!m_independentEntities.empty()) {
            if (ImGui::CollapsingHeader("Independent Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(10.0f);
                
                for (auto& node : m_independentEntities) {
                    if (!scene->getRegistry().valid(node->entityHandle)) {
                        continue; // Skip invalid entities
                    }
                    
                    Rapture::Entity entity(node->entityHandle, scene);
                    
                    // Display as a selectable item with highlighting for the selected entity
                    bool isSelected = (m_selectedEntity.m_EntityHandle == node->entityHandle);
                    if (ImGui::Selectable(node->entityName.c_str(), isSelected)) {
                        m_selectedEntity = entity;
                        if (m_entitySelectionCallback) {
                            m_entitySelectionCallback(m_selectedEntity);
                        }
                    }
                    
                    // Context menu for actions
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Properties")) {
                            m_selectedEntity = entity;
                            if (m_entitySelectionCallback) {
                                m_entitySelectionCallback(m_selectedEntity);
                            }
                        }
                        ImGui::EndPopup();
                    }
                }
                
                ImGui::Unindent(10.0f);
            }
        }
        
        // Section 2: Entity Hierarchies
        if (!m_rootEntities.empty()) {
            if (ImGui::CollapsingHeader("Entity Hierarchies", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& root : m_rootEntities) {
                    displayCachedHierarchy(root, 0, scene);
                }
            }
        }
    } else {
        ImGui::Text("No active scene available");
    }
    
    ImGui::End();
}

// Helper method to find the root entity by traversing up the hierarchy
entt::entity EntityBrowserPanel::findRootEntity(entt::entity entityHandle, Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    Rapture::Entity entity(entityHandle, scene);
    
    // If entity doesn't have a node component, it's not part of a hierarchy
    if (!entity.hasComponent<Rapture::EntityNodeComponent>()) {
        return entt::null;
    }
    
    // Get the entity node component
    auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
    
    // If no parent, this is a root
    if (!nodeComp.entity_node->getParent()) {
        return entityHandle;
    }
    
    // Otherwise, recursively find the parent's root
    auto parentNode = nodeComp.entity_node->getParent();
    if (!parentNode) {
        return entityHandle; // Safeguard against inconsistent state
    }
    
    auto parentEntity = parentNode->getEntity();
    if (!parentEntity) {
        return entityHandle; // Another safeguard against corrupted hierarchy
    }
    
    // Recursive call to find the ultimate root
    return findRootEntity(parentEntity->m_EntityHandle, scene);
}

// Builds the cached hierarchy from scratch
void EntityBrowserPanel::buildHierarchyCache(Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    // Clear existing cache
    m_independentEntities.clear();
    m_rootEntities.clear();
    
    // Skip if scene is null
    if (!scene) {
        return;
    }
    
    auto& registry = scene->getRegistry();
    
    // Get all entities with tag components
    auto view = registry.view<Rapture::TagComponent>();
    
    // Set to keep track of processed entities to avoid duplicates
    std::unordered_set<entt::entity> processedEntities;
    
    // Map of entity handle to hierarchy node
    std::unordered_map<entt::entity, std::shared_ptr<HierarchyNode>> entityNodeMap;
    
    // First pass: create nodes for all entities
    for (auto entityHandle : view) {
        Rapture::Entity entity(entityHandle, scene);
        
        std::string entityName = entity.hasComponent<Rapture::TagComponent>() ? 
            entity.getComponent<Rapture::TagComponent>().tag : 
            std::to_string((uint32_t)entityHandle);
            
        entityNodeMap[entityHandle] = std::make_shared<HierarchyNode>(entityHandle, entityName);
    }
    
    // Second pass: build hierarchies
    for (auto entityHandle : view) {
        Rapture::Entity entity(entityHandle, scene);
        
        if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
            // For entities with EntityNodeComponent, trace up to find the root
            entt::entity rootHandle = findRootEntity(entityHandle, scene);
            
            // Add root to our list if not already added
            if (scene->getRegistry().valid(rootHandle) && 
                processedEntities.find(rootHandle) == processedEntities.end()) {
                
                // Find the root node in our map
                auto rootNodeIt = entityNodeMap.find(rootHandle);
                if (rootNodeIt != entityNodeMap.end()) {
                    m_rootEntities.push_back(rootNodeIt->second);
                    processedEntities.insert(rootHandle);
                }
            }
            
            // If this entity has a parent, add it as a child
            if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
                auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
                
                if (nodeComp.entity_node && nodeComp.entity_node->getParent()) {
                    auto parentNode = nodeComp.entity_node->getParent();
                    if (parentNode && parentNode->getEntity()) {
                        entt::entity parentHandle = parentNode->getEntity()->m_EntityHandle;
                        
                        // Find parent in map
                        auto parentNodeIt = entityNodeMap.find(parentHandle);
                        auto childNodeIt = entityNodeMap.find(entityHandle);
                        
                        if (parentNodeIt != entityNodeMap.end() && 
                            childNodeIt != entityNodeMap.end()) {
                            parentNodeIt->second->children.push_back(childNodeIt->second);
                            processedEntities.insert(entityHandle); // Mark as processed
                        }
                    }
                }
            }
        } else {
            // Entities without EntityNodeComponent are independent
            if (processedEntities.find(entityHandle) == processedEntities.end()) {
                auto nodeIt = entityNodeMap.find(entityHandle);
                if (nodeIt != entityNodeMap.end()) {
                    m_independentEntities.push_back(nodeIt->second);
                    processedEntities.insert(entityHandle);
                }
            }
        }
    }
}

// Display entities from the cached hierarchy
void EntityBrowserPanel::displayCachedHierarchy(const std::shared_ptr<HierarchyNode>& node, int depth, Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    if (!node || !scene->getRegistry().valid(node->entityHandle)) {
        return;
    }
    
    // Base indentation for all entities
    ImGui::Indent(10.0f);
    
    // Additional indentation based on depth
    if (depth > 0) {
        ImGui::Indent(depth * 20.0f);
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf; // No arrow for leaf nodes
    
    // Add selected flag if this entity is currently selected
    if (m_selectedEntity.m_EntityHandle == node->entityHandle)
        flags |= ImGuiTreeNodeFlags_Selected;
    
    // Display tree node for this entity
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node->entityHandle, flags, "%s", node->entityName.c_str());
    
    // Handle selection when clicked
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        // Create entity wrapper
        Rapture::Entity entity(node->entityHandle, scene);
        if (scene->getRegistry().valid(node->entityHandle)) {
            m_selectedEntity = entity;
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(m_selectedEntity);
            }
        }
    }
    
    // Handle right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Properties")) {
            Rapture::Entity entity(node->entityHandle, scene);
            if (scene->getRegistry().valid(node->entityHandle)) {
                m_selectedEntity = entity;
                if (m_entitySelectionCallback) {
                    m_entitySelectionCallback(m_selectedEntity);
                }
            }
        }
        ImGui::EndPopup();
    }
    
    // Display children if node is open
    if (nodeOpen) {
        for (auto& child : node->children) {
            displayCachedHierarchy(child, depth + 1, scene);
        }
        
        ImGui::TreePop();
    }
    
    // Reset indentation
    if (depth > 0) {
        ImGui::Unindent(depth * 20.0f);
    }
    ImGui::Unindent(10.0f);
}

// This method is kept for compatibility, but we're now using the cached version
void EntityBrowserPanel::displayEntityHierarchy(entt::entity entityHandle, int depth, Rapture::Scene* scene, 
                                              std::unordered_set<entt::entity>& displayedEntities) {
    RAPTURE_PROFILE_FUNCTION();
    
    // Skip if already displayed to avoid cycles and duplicates
    if (displayedEntities.find(entityHandle) != displayedEntities.end()) {
        return;
    }
    
    // Validate entity exists in registry
    if (!scene->getRegistry().valid(entityHandle)) {
        Rapture::GE_WARN("Invalid entity handle encountered in hierarchy: {}", (uint32_t)entityHandle);
        return;
    }
    
    // Mark as displayed
    displayedEntities.insert(entityHandle);
    
    Rapture::Entity entity(entityHandle, scene);
    
    // Validate entity has required components
    if (!entity.hasComponent<Rapture::TagComponent>()) {
        Rapture::GE_WARN("Entity missing TagComponent: {}", (uint32_t)entityHandle);
        return;
    }
    
    // Get the entity's name from TagComponent
    std::string entityName = entity.getComponent<Rapture::TagComponent>().tag;
    
    // Base indentation for all entities (including root)
    ImGui::Indent(10.0f);
    
    // Additional indentation based on depth for non-root entities
    if (depth > 0) {
        ImGui::Indent(depth * 20.0f);
    }
    
    // Check if entity has children for tree node display
    bool hasChildren = false;
    std::vector<entt::entity> childrenHandles;
    
    if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
        auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
        
        // Validate node component
        if (!nodeComp.entity_node) {
            Rapture::GE_WARN("Invalid EntityNodeComponent for entity: {}", entityName);
            return;
        }
        
        auto children = nodeComp.entity_node->getChildren();
        hasChildren = !children.empty();
        
        // Collect child entity handles with validation
        for (auto& child : children) {
            if (!child) {
                Rapture::GE_WARN("Invalid child node in entity: {}", entityName);
                continue;
            }
            
            if (auto childEntity = child->getEntity()) {
                if (scene->getRegistry().valid(childEntity->m_EntityHandle)) {
                    childrenHandles.push_back(childEntity->m_EntityHandle);
                } else {
                    Rapture::GE_WARN("Invalid child entity handle in entity: {}", entityName);
                }
            }
        }
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf; // No arrow for leaf nodes
    
    // Add selected flag if this entity is currently selected
    if (m_selectedEntity.m_EntityHandle == entityHandle)
        flags |= ImGuiTreeNodeFlags_Selected;
    
    // Display tree node for this entity
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entityHandle, flags, "%s", entityName.c_str());
    
    // Handle selection when clicked
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        // Validate entity before selection
        if (scene->getRegistry().valid(entityHandle) && 
            entity.hasComponent<Rapture::TagComponent>() && 
            entity.hasComponent<Rapture::EntityNodeComponent>()) {
            m_selectedEntity = entity;
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(m_selectedEntity);
            }
        } else {
            Rapture::GE_WARN("Attempted to select invalid entity: {}", entityName);
        }
    }
    
    // Handle right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Properties")) {
            // Validate entity before selection
            if (scene->getRegistry().valid(entityHandle) && 
                entity.hasComponent<Rapture::TagComponent>() && 
                entity.hasComponent<Rapture::EntityNodeComponent>()) {
                m_selectedEntity = entity;
                if (m_entitySelectionCallback) {
                    m_entitySelectionCallback(m_selectedEntity);
                }
            } else {
                Rapture::GE_WARN("Attempted to show properties for invalid entity: {}", entityName);
            }
        }
        ImGui::EndPopup();
    }
    
    // Display children if node is open
    if (nodeOpen) {
        // Process and display children recursively
        for (auto childHandle : childrenHandles) {
            displayEntityHierarchy(childHandle, depth + 1, scene, displayedEntities);
        }
        
        ImGui::TreePop();
    }
    
    // Reset indentation
    if (depth > 0) {
        ImGui::Unindent(depth * 20.0f);
    }
    ImGui::Unindent(10.0f);
}
