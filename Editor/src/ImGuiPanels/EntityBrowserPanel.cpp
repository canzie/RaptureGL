#include "EntityBrowserPanel.h"
#include "Logger/Log.h"
#include "Debug/TracyProfiler.h"
#include "ImGuiPanels/imGuiPanelStyle.h"

void EntityBrowserPanel::render(Rapture::Scene* scene, EntitySelectionCallback callback) {
    RAPTURE_PROFILE_FUNCTION();
    
    // Push bold font for the title
    ImGui::PushFont(ImGuiPanelStyle::GetBoldFont());
    ImGui::Begin("Entity Browser");
    ImGui::PopFont();
    
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
        
        // Add entity button
        if (ImGui::Button("Create Entity")) {
            // Create a new entity in the scene
            Rapture::Entity newEntity = scene->createEntity("New Entity");
            
            // Select the newly created entity
            m_selectedEntity = std::make_shared<Rapture::Entity>(newEntity);
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(m_selectedEntity);
            }
            
            // Rebuild hierarchy to include the new entity
            m_needsHierarchyRebuild = true;
        }
        
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
            // Push bold font for the header
            ImGui::PushFont(ImGuiPanelStyle::GetBoldFont());
            bool headerOpen = ImGui::CollapsingHeader("Independent Entities", ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopFont();
            
            if (headerOpen) {
                ImGui::Indent(5.0f);
                
                for (auto& node : m_independentEntities) {
                    if (!node->entity || !node->entity->isValid()) {
                        continue; // Skip invalid entities
                    }
                    
                    // Set up flags for selectable with consistent style
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                    
                    // Set selected flag if this is the current entity
                    bool isSelected = (m_selectedEntity && node->entity && m_selectedEntity->getID() == node->entity->getID());
                    if (isSelected) {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }
                    
                    // Use TreeNodeEx instead of Selectable for consistent styling
                    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node->entity->getID(), flags, "%s", node->entityName.c_str());
                    
                    // Handle selection when clicked
                    if (ImGui::IsItemClicked()) {
                        m_selectedEntity = node->entity;
                        if (m_entitySelectionCallback) {
                            m_entitySelectionCallback(m_selectedEntity);
                        }
                    }
                    
                    // Context menu for actions
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Properties")) {
                            m_selectedEntity = node->entity;
                            if (m_entitySelectionCallback) {
                                m_entitySelectionCallback(m_selectedEntity);
                            }
                        }
                        ImGui::EndPopup();
                    }
                    
                    if (nodeOpen) {
                        ImGui::TreePop();
                    }
                }
                
                ImGui::Unindent(5.0f);
            }
        }
        
        // Section 2: Entity Hierarchies
        if (!m_rootEntities.empty()) {
            // Push bold font for the header
            ImGui::PushFont(ImGuiPanelStyle::GetBoldFont());
            bool headerOpen = ImGui::CollapsingHeader("Entity Hierarchies", ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopFont();
            
            if (headerOpen) {
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
std::shared_ptr<Rapture::Entity> EntityBrowserPanel::findRootEntity(std::shared_ptr<Rapture::Entity> entity, Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    // If entity is invalid or doesn't have a node component, it's not part of a hierarchy
    if (!entity || !entity->isValid() || !entity->hasComponent<Rapture::EntityNodeComponent>()) {
        return nullptr;
    }
    
    // Get the entity node component
    auto& nodeComp = entity->getComponent<Rapture::EntityNodeComponent>();
    
    // If no parent, this is a root
    if (!nodeComp.entity_node->getParent()) {
        return entity;
    }
    
    // Otherwise, recursively find the parent's root
    auto parentNode = nodeComp.entity_node->getParent();
    if (!parentNode) {
        return entity; // Safeguard against inconsistent state
    }
    
    auto parentEntity = parentNode->getEntity();
    if (!parentEntity) {
        return entity; // Another safeguard against corrupted hierarchy
    }
    
    // Create a shared_ptr to the parent entity
    std::shared_ptr<Rapture::Entity> parentPtr = std::make_shared<Rapture::Entity>(*parentEntity);
    
    // Recursive call to find the ultimate root
    return findRootEntity(parentPtr, scene);
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
    std::unordered_set<uint32_t> processedEntities;
    
    // Map of entity ID to hierarchy node
    std::unordered_map<uint32_t, std::shared_ptr<HierarchyNode>> entityNodeMap;
    
    // First pass: create nodes for all entities
    for (auto entityHandle : view) {
        Rapture::Entity entity(entityHandle, scene);
        
        std::string entityName = entity.hasComponent<Rapture::TagComponent>() ? 
            entity.getComponent<Rapture::TagComponent>().tag : 
            std::to_string(entity.getID());
        
        std::shared_ptr<Rapture::Entity> entityPtr = std::make_shared<Rapture::Entity>(entity);
        entityNodeMap[entity.getID()] = std::make_shared<HierarchyNode>(entityPtr, entityName);
    }
    
    // Second pass: build hierarchies
    for (auto entityHandle : view) {
        Rapture::Entity entity(entityHandle, scene);
        std::shared_ptr<Rapture::Entity> entityPtr = std::make_shared<Rapture::Entity>(entity);
        
        if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
            // For entities with EntityNodeComponent, trace up to find the root
            std::shared_ptr<Rapture::Entity> rootEntity = findRootEntity(entityPtr, scene);
            
            // Add root to our list if not already added and it's valid
            if (rootEntity && rootEntity->isValid() && 
                processedEntities.find(rootEntity->getID()) == processedEntities.end()) {
                
                // Find the root node in our map
                auto rootNodeIt = entityNodeMap.find(rootEntity->getID());
                if (rootNodeIt != entityNodeMap.end()) {
                    m_rootEntities.push_back(rootNodeIt->second);
                    processedEntities.insert(rootEntity->getID());
                }
            }
            
            // If this entity has a parent, add it as a child
            if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
                auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
                
                if (nodeComp.entity_node && nodeComp.entity_node->getParent()) {
                    auto parentNode = nodeComp.entity_node->getParent();
                    if (parentNode && parentNode->getEntity()) {
                        uint32_t parentID = parentNode->getEntity()->getID();
                        
                        // Find parent in map
                        auto parentNodeIt = entityNodeMap.find(parentID);
                        auto childNodeIt = entityNodeMap.find(entity.getID());
                        
                        if (parentNodeIt != entityNodeMap.end() && 
                            childNodeIt != entityNodeMap.end()) {
                            parentNodeIt->second->children.push_back(childNodeIt->second);
                            processedEntities.insert(entity.getID()); // Mark as processed
                        }
                    }
                }
            }
        } else {
            // Entities without EntityNodeComponent are independent
            if (processedEntities.find(entity.getID()) == processedEntities.end()) {
                auto nodeIt = entityNodeMap.find(entity.getID());
                if (nodeIt != entityNodeMap.end()) {
                    m_independentEntities.push_back(nodeIt->second);
                    processedEntities.insert(entity.getID());
                }
            }
        }
    }
}

// Display entities from the cached hierarchy
void EntityBrowserPanel::displayCachedHierarchy(const std::shared_ptr<HierarchyNode>& node, int depth, Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    if (!node || !node->entity || !node->entity->isValid()) {
        return;
    }
    
    // Base indentation for all entities - reduced to bring text closer to tree lines
    ImGui::Indent(5.0f);
    
    // Additional indentation based on depth - reduced spacing
    if (depth > 0) {
        ImGui::Indent(depth * 15.0f);
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    if (node->children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf; // No arrow for leaf nodes
    
    // Add selected flag if this entity is currently selected
    bool isSelected = m_selectedEntity && node->entity && 
                      m_selectedEntity->isValid() && node->entity->isValid() && 
                      m_selectedEntity->getID() == node->entity->getID();
    
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;
    
    // Display tree node for this entity
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node->entity->getID(), flags, "%s", node->entityName.c_str());
    
    // Get node rectangle for line drawing
    ImRect nodeRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    
    // Handle selection when clicked
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        // Create entity wrapper
        if (node->entity && node->entity->isValid()) {
            m_selectedEntity = node->entity;
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(m_selectedEntity);
            }
        }
    }
    
    // Handle right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Properties")) {
            if (node->entity && node->entity->isValid()) {
                m_selectedEntity = node->entity;
                if (m_entitySelectionCallback) {
                    m_entitySelectionCallback(m_selectedEntity);
                }
            }
        }
        ImGui::EndPopup();
    }
    
    // Display children if node is open
    if (nodeOpen) {
        if (!node->children.empty()) {
            // Set up for drawing lines
            const ImColor treeLineColor = ImGui::GetColorU32(ImGuiCol_Text);
            const float smallOffsetX = 9.0f; // Adjusted to align better with the arrow symbol
            const float horizontalLineSize = 7.0f; // Increased from 5.0f to make lines slightly longer
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Start position for the vertical line
            ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();
            verticalLineStart.x += smallOffsetX;
            ImVec2 verticalLineEnd = verticalLineStart;
            
            // Store child rectangles for line drawing
            std::vector<ImRect> childRects;
            
            // Process and display all children first to collect their rectangles
            for (auto& child : node->children) {
                if (child && child->entity && child->entity->isValid()) {
                    ImGui::PushID((void*)(intptr_t)child->entity->getID());
                    ImRect childRect = displayCachedHierarchyWithRect(child, depth + 1, scene);
                    childRects.push_back(childRect);
                    ImGui::PopID();
                }
            }
            
            // Now draw lines to connect all children
            for (const auto& childRect : childRects) {
                // Calculate the vertical midpoint of the child node
                float midpoint = (childRect.Min.y + childRect.Max.y) / 2.0f;
                
                // Draw horizontal line from vertical line to child node
                drawList->AddLine(
                    ImVec2(verticalLineStart.x, midpoint), 
                    ImVec2(verticalLineStart.x + horizontalLineSize, midpoint), 
                    treeLineColor
                );
                
                // Update the end point of the vertical line
                verticalLineEnd.y = midpoint;
            }
            
            // Draw the vertical line connecting all children
            if (!childRects.empty()) {
                drawList->AddLine(
                    ImVec2(verticalLineStart.x, verticalLineStart.y), 
                    ImVec2(verticalLineEnd.x, verticalLineEnd.y), 
                    treeLineColor
                );
            }
        } else {
            // Process children normally if not drawing lines
            for (auto& child : node->children) {
                if (child && child->entity && child->entity->isValid()) {
                    displayCachedHierarchy(child, depth + 1, scene);
                }
            }
        }
        
        ImGui::TreePop();
    }
    
    // Reset indentation
    if (depth > 0) {
        ImGui::Unindent(depth * 15.0f);
    }
    ImGui::Unindent(5.0f);
}

// Helper function that returns the node rectangle for line drawing
ImRect EntityBrowserPanel::displayCachedHierarchyWithRect(const std::shared_ptr<HierarchyNode>& node, int depth, Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    if (!node || !node->entity || !node->entity->isValid()) {
        return ImRect();
    }
    
    // Base indentation for all entities - reduced to bring text closer to tree lines
    ImGui::Indent(5.0f);
    
    // Additional indentation based on depth - reduced spacing
    if (depth > 0) {
        ImGui::Indent(depth * 15.0f);
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    if (node->children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf; // No arrow for leaf nodes
    
    // Add selected flag if this entity is currently selected
    bool isSelected = m_selectedEntity && node->entity && 
                      m_selectedEntity->isValid() && node->entity->isValid() && 
                      m_selectedEntity->getID() == node->entity->getID();
                      
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;
    
    // Display tree node for this entity
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node->entity->getID(), flags, "%s", node->entityName.c_str());
    
    // Get node rectangle for line drawing
    ImRect nodeRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    
    // Handle selection when clicked
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        // Create entity wrapper
        if (node->entity && node->entity->isValid()) {
            m_selectedEntity = node->entity;
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(m_selectedEntity);
            }
        }
    }
    
    // Handle right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Properties")) {
            if (node->entity && node->entity->isValid()) {
                m_selectedEntity = node->entity;
                if (m_entitySelectionCallback) {
                    m_entitySelectionCallback(m_selectedEntity);
                }
            }
        }
        ImGui::EndPopup();
    }
    
    // Display children if node is open
    if (nodeOpen) {
        if (!node->children.empty()) {
            // Set up for drawing lines
            const ImColor treeLineColor = ImGui::GetColorU32(ImGuiCol_Text);
            const float smallOffsetX = 9.0f; // Adjusted to align better with the arrow symbol
            const float horizontalLineSize = 7.0f; // Increased from 5.0f to make lines slightly longer
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Start position for the vertical line
            ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();
            verticalLineStart.x += smallOffsetX;
            ImVec2 verticalLineEnd = verticalLineStart;
            
            // Store child rectangles for line drawing
            std::vector<ImRect> childRects;
            
            // Process and display all children first to collect their rectangles
            for (auto& child : node->children) {
                if (child && child->entity && child->entity->isValid()) {
                    ImGui::PushID((void*)(intptr_t)child->entity->getID());
                    ImRect childRect = displayCachedHierarchyWithRect(child, depth + 1, scene);
                    childRects.push_back(childRect);
                    ImGui::PopID();
                }
            }
            
            // Now draw lines to connect all children
            for (const auto& childRect : childRects) {
                // Calculate the vertical midpoint of the child node
                float midpoint = (childRect.Min.y + childRect.Max.y) / 2.0f;
                
                // Draw horizontal line from vertical line to child node
                drawList->AddLine(
                    ImVec2(verticalLineStart.x, midpoint), 
                    ImVec2(verticalLineStart.x + horizontalLineSize, midpoint), 
                    treeLineColor
                );
                
                // Update the end point of the vertical line
                verticalLineEnd.y = midpoint;
            }
            
            // Draw the vertical line connecting all children
            if (!childRects.empty()) {
                drawList->AddLine(
                    ImVec2(verticalLineStart.x, verticalLineStart.y), 
                    ImVec2(verticalLineEnd.x, verticalLineEnd.y), 
                    treeLineColor
                );
            }
        } else {
            // Process children normally if not drawing lines
            for (auto& child : node->children) {
                if (child && child->entity && child->entity->isValid()) {
                    displayCachedHierarchyWithRect(child, depth + 1, scene);
                }
            }
        }
        
        ImGui::TreePop();
    }
    
    // Reset indentation
    if (depth > 0) {
        ImGui::Unindent(depth * 15.0f);
    }
    ImGui::Unindent(5.0f);
    
    return nodeRect;
}

// This method is kept for compatibility, but we're now using the cached version
void EntityBrowserPanel::displayEntityHierarchy(std::shared_ptr<Rapture::Entity> entity, int depth, Rapture::Scene* scene, 
                                              std::unordered_set<uint32_t>& displayedEntities) {
    RAPTURE_PROFILE_FUNCTION();
    
    // Skip if entity is invalid
    if (!entity || !entity->isValid()) {
        return;
    }
    
    // Skip if already displayed to avoid cycles and duplicates
    uint32_t entityId = entity->getID();
    if (displayedEntities.find(entityId) != displayedEntities.end()) {
        return;
    }
    
    // Mark as displayed
    displayedEntities.insert(entityId);
    
    // Validate entity has required components
    if (!entity->hasComponent<Rapture::TagComponent>()) {
        Rapture::GE_WARN("Entity missing TagComponent: {}", entityId);
        return;
    }
    
    // Get the entity's name from TagComponent
    std::string entityName = entity->getComponent<Rapture::TagComponent>().tag;
    
    // Base indentation for all entities (including root)
    ImGui::Indent(10.0f);
    
    // Additional indentation based on depth for non-root entities
    if (depth > 0) {
        ImGui::Indent(depth * 20.0f);
    }
    
    // Check if entity has children for tree node display
    bool hasChildren = false;
    std::vector<std::shared_ptr<Rapture::Entity>> childrenEntities;
    
    if (entity->hasComponent<Rapture::EntityNodeComponent>()) {
        auto& nodeComp = entity->getComponent<Rapture::EntityNodeComponent>();
        
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
                if (childEntity->isValid()) {
                    childrenEntities.push_back(std::make_shared<Rapture::Entity>(*childEntity));
                } else {
                    Rapture::GE_WARN("Invalid child entity in entity: {}", entityName);
                }
            }
        }
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    // Add connecting lines for better visualization of hierarchy
    flags |= ImGuiTreeNodeFlags_SpanFullWidth;
    
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf; // No arrow for leaf nodes
    
    // Add selected flag if this entity is currently selected
    if (m_selectedEntity && m_selectedEntity->getID() == entity->getID())
        flags |= ImGuiTreeNodeFlags_Selected;
    
    // Display tree node for this entity
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity->getID(), flags, "%s", entityName.c_str());
    
    // Handle selection when clicked
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        // Validate entity before selection
        if (entity->isValid() && 
            entity->hasComponent<Rapture::TagComponent>()) {
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
            if (entity->isValid() && 
                entity->hasComponent<Rapture::TagComponent>()) {
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
        for (auto& childEntity : childrenEntities) {
            displayEntityHierarchy(childEntity, depth + 1, scene, displayedEntities);
        }
        
        ImGui::TreePop();
    }
    
    // Reset indentation
    if (depth > 0) {
        ImGui::Unindent(depth * 20.0f);
    }
    ImGui::Unindent(10.0f);
}
