#include "EntityBrowserPanel.h"
#include "Logger/Log.h"


void EntityBrowserPanel::render(Rapture::Scene* scene, EntitySelectionCallback callback) {
    ImGui::Begin("Entity Browser");
    m_entitySelectionCallback = callback;
    if (scene) {
        auto& registry = scene->getRegistry();
        
        // Get all entities with tag components
        auto view = registry.view<Rapture::TagComponent>();
        
        // Count total entities
        ImGui::Text("Total Entities: %d", static_cast<int>(view.size()));
        ImGui::Separator();
        
        // Set to keep track of processed entities to avoid duplicates
        std::unordered_set<entt::entity> processedEntities;
        
        // Collections for different entity types
        std::vector<entt::entity> rootEntities;        // Root entities (no parent)
        std::vector<entt::entity> independentEntities; // Entities without EntityNodeComponent
        
        // First scan: collect all entities and find roots
        for (auto entityHandle : view) {
            Rapture::Entity entity(entityHandle, scene);
            
            if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
                // For entities with EntityNodeComponent, trace up to find the root
                entt::entity rootHandle = findRootEntity(entityHandle, scene);
                
                // Add root to our list if not already added
                if (scene->getRegistry().valid(rootHandle) && processedEntities.find(rootHandle) == processedEntities.end()) {
                    rootEntities.push_back(rootHandle);
                    processedEntities.insert(rootHandle);
                }
            } else {
                // Entities without EntityNodeComponent are independent
                independentEntities.push_back(entityHandle);
            }
        }
        
        // Section 1: Independent Entities (no relationships)
        if (!independentEntities.empty()) {
            if (ImGui::CollapsingHeader("Independent Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(10.0f);
                
                for (auto entityHandle : independentEntities) {
                    Rapture::Entity entity(entityHandle, scene);
                    std::string entityName = entity.hasComponent<Rapture::TagComponent>() ? 
                        entity.getComponent<Rapture::TagComponent>().tag : 
                        std::to_string((uint32_t)entityHandle);
                        
                    // Display as a selectable item with highlighting for the selected entity
                    bool isSelected = (m_selectedEntity.m_EntityHandle == entityHandle);
                    if (ImGui::Selectable(entityName.c_str(), isSelected)) {
                        m_selectedEntity = entity;
                        if (m_entitySelectionCallback) {
                            m_entitySelectionCallback(m_selectedEntity);
                        }
                        Rapture::GE_INFO("Selected entity: {}", entityName);
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
        if (!rootEntities.empty()) {
            if (ImGui::CollapsingHeader("Entity Hierarchies", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Create a fresh set for tracking displayed entities within the hierarchy
                std::unordered_set<entt::entity> displayedEntities;
                
                // Display each root entity and its hierarchy
                for (auto rootEntity : rootEntities) {
                    displayEntityHierarchy(rootEntity, 0, scene, displayedEntities);
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

void EntityBrowserPanel::displayEntityHierarchy(entt::entity entityHandle, int depth, Rapture::Scene* scene, 
                                              std::unordered_set<entt::entity>& displayedEntities) {
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
            Rapture::GE_INFO("Selected entity: {}", entityName);
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
