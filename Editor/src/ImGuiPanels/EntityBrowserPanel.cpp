#include "EntityBrowserPanel.h"
#include "Logger/Log.h"

namespace Rapture {

void EntityBrowserPanel::render(TestLayer* testLayer) {
    ImGui::Begin("Entity Browser");
    
    if (testLayer) {
        auto& registry = testLayer->getActiveScene()->getRegistry();
        
        // Get all entities with tag components
        auto view = registry.view<Rapture::TagComponent>();
        
        // Count total entities
        ImGui::Text("Total Entities: %d", static_cast<int>(view.size()));
        ImGui::Separator();
        
        // Create collections for different entity types
        std::vector<entt::entity> independentEntities; // Entities without EntityNodeComponent
        std::vector<entt::entity> rootEntities;        // Root entities with EntityNodeComponent
        
        // First pass: collect all entity nodes
        for (auto entityHandle : view) {
            Rapture::Entity entity(entityHandle, testLayer->getActiveScene().get());
            
            if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
                auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
                
                // If this entity has no parent, it's a root entity
                if (!nodeComp.entity_node->getParent()) {
                    rootEntities.push_back(entityHandle);
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
                    Rapture::Entity entity(entityHandle, testLayer->getActiveScene().get());
                    std::string entityName = entity.hasComponent<Rapture::TagComponent>() ? 
                        entity.getComponent<Rapture::TagComponent>().tag : 
                        std::to_string((uint32_t)entityHandle);
                        
                    // Display as a simple selectable item
                    if (ImGui::Selectable(entityName.c_str())) {
                        // TODO: Select this entity in properties panel
                    }
                    
                    // Context menu for actions
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Properties")) {
                            // TODO: Select this entity in properties panel
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
                ImGui::Indent(10.0f);
                
                // Display each root entity and its hierarchy
                for (auto rootEntity : rootEntities) {
                    displayEntityHierarchy(rootEntity, 0, testLayer->getActiveScene().get());
                }
                
                ImGui::Unindent(10.0f);
            }
        }
    } else {
        ImGui::Text("No active scene available");
    }
    
    ImGui::End();
}

void EntityBrowserPanel::displayEntityHierarchy(entt::entity entityHandle, int depth, Scene* scene) {
    Rapture::Entity entity(entityHandle, scene);
    
    // Get the entity's name from TagComponent
    std::string entityName = entity.hasComponent<Rapture::TagComponent>() ? 
        entity.getComponent<Rapture::TagComponent>().tag : 
        std::to_string((uint32_t)entityHandle);
    
    // Indentation based on depth
    ImGui::Indent(depth * 20.0f);
    
    // Check if entity has children for tree node display
    bool hasChildren = false;
    std::vector<entt::entity> childrenHandles;
    
    if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
        auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
        auto children = nodeComp.entity_node->getChildren();
        hasChildren = !children.empty();
        
        // Collect child entity handles
        for (auto& child : children) {
            if (auto childEntity = child->getEntity()) {
                childrenHandles.push_back(childEntity->m_EntityHandle);
            }
        }
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf; // No arrow for leaf nodes
    
    // Display tree node for this entity
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entityHandle, flags, "%s", entityName.c_str());
    
    // Handle right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Properties")) {
            // TODO: Select this entity in properties panel
        }
        ImGui::EndPopup();
    }
    
    // Display entity details if node is open
    if (nodeOpen) {
        // List components
        if (ImGui::TreeNode("Components")) {
            // Check for and list common components
            if (entity.hasComponent<Rapture::TransformComponent>())
                ImGui::BulletText("TransformComponent");
            
            if (entity.hasComponent<Rapture::MeshComponent>())
                ImGui::BulletText("MeshComponent");
            
            if (entity.hasComponent<Rapture::MaterialComponent>()) {
                auto& material = entity.getComponent<Rapture::MaterialComponent>();
                ImGui::BulletText("MaterialComponent (%s)", material.materialName.c_str());
            }
            
            if (entity.hasComponent<Rapture::CameraControllerComponent>())
                ImGui::BulletText("CameraControllerComponent");
            
            if (entity.hasComponent<Rapture::EntityNodeComponent>())
                ImGui::BulletText("EntityNodeComponent");
            
            ImGui::TreePop();
        }
        
        // Display children recursively
        for (auto childHandle : childrenHandles) {
            displayEntityHierarchy(childHandle, depth + 1, scene); // Increment depth for proper indentation
        }
        
        ImGui::TreePop();
    }
    
    // Reset indentation
    ImGui::Indent(-depth * 20.0f);
}

}  // namespace Rapture 