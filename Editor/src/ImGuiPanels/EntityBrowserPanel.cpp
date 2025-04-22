#include "EntityBrowserPanel.h"
#include "Logger/Log.h"
#include "Debug/TracyProfiler.h"
#include "ImGuiPanels/imGuiPanelStyle.h"

#include "imgui_internal.h" // Required for TableSetBgColor

void EntityBrowserPanel::render(Rapture::Scene* scene, EntitySelectionCallback callback) {
    RAPTURE_PROFILE_FUNCTION();
    
    ImGui::PushFont(ImGuiPanelStyle::GetBoldFont());
    ImGui::Begin("Entity Browser");
    ImGui::PopFont();
    
    m_entitySelectionCallback = callback;
    
    if (scene) {
        auto& registry = scene->getRegistry();
        
        auto view = registry.view<Rapture::TagComponent>();
        uint32_t entityCount = static_cast<uint32_t>(view.size());
        
        // Display total entities
        ImGui::Text("Total Entities: %d", entityCount);
        
        // Refresh button on the right
        ImGui::SameLine(ImGui::GetWindowWidth() - 80.0f); // Adjust position as needed
        if (ImGui::Button("Refresh")) { // TODO: Replace with icon button if FontAwesome is integrated
            m_needsHierarchyRebuild = true;
        }
        
        ImGui::Separator();
        
        // Check if hierarchy cache needs rebuilding
        bool sceneChanged = (m_cachedScene != scene);
        bool entityCountChanged = (m_lastEntityCount != entityCount);
        
        if (sceneChanged || entityCountChanged || m_needsHierarchyRebuild) {
            RAPTURE_PROFILE_SCOPE("Rebuild Hierarchy Cache");
            buildHierarchyCache(scene);
            m_cachedScene = scene;
            m_lastEntityCount = entityCount;
            m_needsHierarchyRebuild = false;
        }
        
        // Define table structure
        const int columnCount = 3;
        ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable 
                                   | ImGuiTableFlags_RowBg; // Enable RowBg for alternating colors
        
        if (ImGui::BeginTable("EntityHierarchyTable", columnCount, tableFlags)) {
            // Setup columns
            ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f); // Fixed width for type
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100.0f); // Fixed width for actions
            ImGui::TableHeadersRow();
            
            // Render hierarchy rows
            int rowIndex = 0;
            for (const auto& rootNode : m_hierarchyRoots) {
                if (rootNode && rootNode->entity && rootNode->entity->isValid()) {
                    renderHierarchyRow(rootNode, 0, rowIndex);
                }
            }
            
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("No active scene available");
    }
    
    ImGui::End();
}

// Builds the cached hierarchy from scratch, populating a single root list
void EntityBrowserPanel::buildHierarchyCache(Rapture::Scene* scene) {
    RAPTURE_PROFILE_FUNCTION();
    
    m_hierarchyRoots.clear();
    
    if (!scene) {
        return;
    }
    
    auto& registry = scene->getRegistry();
    auto view = registry.view<Rapture::TagComponent>();
    
    // Map to store all nodes temporarily
    std::unordered_map<uint32_t, std::shared_ptr<HierarchyNode>> entityNodeMap;
    // Set to keep track of entities that have been added as children
    std::unordered_set<uint32_t> childrenEntities;
    
    // First pass: Create nodes for all entities
    for (auto entityHandle : view) {
        Rapture::Entity entity(entityHandle, scene);
        if (!entity.isValid()) continue;
        
        std::string entityName = entity.hasComponent<Rapture::TagComponent>() ? 
            entity.getComponent<Rapture::TagComponent>().tag : 
            ("Entity " + std::to_string(entity.getID())); // Default name
        
        std::shared_ptr<Rapture::Entity> entityPtr = std::make_shared<Rapture::Entity>(entity);
        entityNodeMap[entity.getID()] = std::make_shared<HierarchyNode>(entityPtr, entityName);
    }
    
    // Second pass: Build parent-child relationships
    for (auto entityHandle : view) {
        Rapture::Entity entity(entityHandle, scene);
        if (!entity.isValid()) continue;
        
        uint32_t entityID = entity.getID();
        if (entity.hasComponent<Rapture::EntityNodeComponent>()) {
            auto& nodeComp = entity.getComponent<Rapture::EntityNodeComponent>();
            if (nodeComp.entity_node && nodeComp.entity_node->getParent()) {
                auto parentNode = nodeComp.entity_node->getParent();
                if (parentNode && parentNode->getEntity()) {
                    uint32_t parentID = parentNode->getEntity()->getID();
                    
                    // Find parent and child in map
                    auto parentNodeIt = entityNodeMap.find(parentID);
                    auto childNodeIt = entityNodeMap.find(entityID);
                    
                    if (parentNodeIt != entityNodeMap.end() && childNodeIt != entityNodeMap.end()) {
                        parentNodeIt->second->children.push_back(childNodeIt->second);
                        childrenEntities.insert(entityID); // Mark this entity as a child
                    }
                }
            }
        }
    }
    
    // Third pass: Add root nodes (entities that are not children of anyone)
    for (auto const& [id, node] : entityNodeMap) {
        if (childrenEntities.find(id) == childrenEntities.end()) {
            m_hierarchyRoots.push_back(node);
        }
    }
}

// Recursively renders a row in the hierarchy table
void EntityBrowserPanel::renderHierarchyRow(const std::shared_ptr<HierarchyNode>& node, int depth, int& rowIndex) {
    RAPTURE_PROFILE_FUNCTION();
    
    if (!node || !node->entity || !node->entity->isValid()) {
        return;
    }
    
    ImGui::TableNextRow();
    rowIndex++; // Increment row index for striping
    
    // --- Name Column ---    
    ImGui::TableSetColumnIndex(0);
    
    // Setup flags for the tree node
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow 
                             | ImGuiTreeNodeFlags_SpanAllColumns; // Make the node span all columns
    
    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // Leaf node specifics
    }
    
    // Selection state
    bool isSelected = m_selectedEntity && m_selectedEntity->isValid() && (m_selectedEntity->getID() == node->entity->getID());
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Indentation
    float indentSize = depth * 20.0f; // Adjust indentation size as needed
    ImGui::Indent(indentSize);
    
    // Alternating row background color
    ImU32 rowBgColor = ImGui::ColorConvertFloat4ToU32(
        (rowIndex % 2 == 0) ? ImGuiPanelStyle::BACKGROUND_SECONDARY : ImGuiPanelStyle::BACKGROUND_PRIMARY
    );
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0 + (rowIndex % 2), rowBgColor);
    
    // Render the tree node itself
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node->entity->getID(), flags, "%s", node->entityName.c_str());
    
    // Handle click for selection (only if not toggling open/close)
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        m_selectedEntity = node->entity;
        if (m_entitySelectionCallback) {
            m_entitySelectionCallback(m_selectedEntity);
        }
    }
    
    // Context Menu (Example)
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Properties")) {
             m_selectedEntity = node->entity;
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(m_selectedEntity);
            }
        }
        // Add other actions like Delete, Duplicate, Add Child etc.
        if (ImGui::MenuItem("Delete Entity")) {
             // TODO: Implement entity deletion logic 
             Rapture::GE_CORE_WARN("Delete entity requested but not implemented yet.");
        }
        ImGui::EndPopup();
    }
    
    ImGui::Unindent(indentSize);
    
    // --- Type Column ---    
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("Entity"); // Simple type for now
    
    // --- Actions Column ---    
    ImGui::TableSetColumnIndex(2);
    // Add buttons here later, e.g., visibility toggle
    // ImGui::PushID((void*)(intptr_t)node->entity->getID()); // Ensure unique IDs for buttons
    // if (ImGui::Button("...")) {} 
    // ImGui::PopID();
    ImGui::TextUnformatted(""); // Placeholder
    
    // Recurse for children if the node is open and it's not a leaf
    if (!(flags & ImGuiTreeNodeFlags_Leaf) && nodeOpen) {
        for (const auto& childNode : node->children) {
            renderHierarchyRow(childNode, depth + 1, rowIndex);
        }
        ImGui::TreePop(); // Pop the node if it was opened and not a leaf
    }
}
