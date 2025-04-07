#include "ViewportPanel.h"
#include "TestLayer.h"
#include "Logger/Log.h"
#include <glm/gtc/type_ptr.hpp>
#include "Scenes/Components/Components.h"
#include "ImGuiPanelStyle.h"
#include "Scenes/Systems/BoundingBoxSystem.h"

void ViewportPanel::renderSceneViewport(TestLayer* testLayer) {
    ImGui::Begin("Scene Viewport");
    
    // Get the size of the ImGui window viewport
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    // Store the current cursor position - this is the top-left corner of the viewport
    m_viewportPosition = ImGui::GetCursorScreenPos();
    
    if (testLayer) {
            // Calculate position for the gizmo controls overlay
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 controlPos(
                windowPos.x + ImGui::GetWindowWidth() - 200, // Move further left
                windowPos.y + 40                            // Keep the same vertical position
            );
            
            // Save the current cursor position
            ImVec2 origCursorPos = ImGui::GetCursorPos();
            
            // Begin overlay for gizmo controls
            ImGui::SetNextWindowPos(controlPos);
            ImGui::SetNextWindowBgAlpha(0.85f); // Less transparent background
            ImGui::BeginChild("GizmoControls", ImVec2(160, 60), true, // Wider panel
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
            
            bool isTranslate = m_currentGizmoOperation == ImGuizmo::TRANSLATE;
            bool isRotate = m_currentGizmoOperation == ImGuizmo::ROTATE;
            bool isScale = m_currentGizmoOperation == ImGuizmo::SCALE;
            
            // Get the background and hover colors from style
            ImVec4 defaultBgColor = ImGuiPanelStyle::BACKGROUND_TERTIARY;
            ImVec4 selectedBgColor = ImGuiPanelStyle::HOVER_OVERLAY;
            ImVec4 textColor = ImGuiPanelStyle::TEXT_NORMAL;
            ImVec4 hoverColor = ImGuiPanelStyle::LIGHT_PURPLE;
            ImVec4 accentColor = ImGuiPanelStyle::ACCENT_PRIMARY;
            
            // Button size
            const float buttonSize = 39.0f;
            const float iconSize = 28.0f;
            const float spacing = 52.0f;
            float posX = ImGui::GetCursorPosX();
            
            // Translate button with icon
            if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
            else ImGui::PushStyleColor(ImGuiCol_Button, defaultBgColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
            
            bool translateClicked = ImGui::Button("##Translate", ImVec2(buttonSize, buttonSize));
            
            // Draw translate icon (arrows pointing outward)
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImVec2 buttonCenter = ImVec2((buttonMin.x + buttonMax.x) * 0.5f, (buttonMin.y + buttonMax.y) * 0.5f);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Draw 3 arrows for translate (X, Y, Z)
            float arrowSize = iconSize * 0.5f;
            // X arrow (red)
            drawList->AddLine(
                ImVec2(buttonCenter.x - arrowSize * 0.5f, buttonCenter.y),
                ImVec2(buttonCenter.x + arrowSize, buttonCenter.y),
                IM_COL32(255, 50, 50, 255), 2.0f);
            drawList->AddTriangleFilled(
                ImVec2(buttonCenter.x + arrowSize, buttonCenter.y - arrowSize * 0.3f),
                ImVec2(buttonCenter.x + arrowSize, buttonCenter.y + arrowSize * 0.3f),
                ImVec2(buttonCenter.x + arrowSize * 1.5f, buttonCenter.y),
                IM_COL32(255, 50, 50, 255));
                
            // Y arrow (green)
            drawList->AddLine(
                ImVec2(buttonCenter.x, buttonCenter.y + arrowSize * 0.5f),
                ImVec2(buttonCenter.x, buttonCenter.y - arrowSize),
                IM_COL32(50, 255, 50, 255), 2.0f);
            drawList->AddTriangleFilled(
                ImVec2(buttonCenter.x - arrowSize * 0.3f, buttonCenter.y - arrowSize),
                ImVec2(buttonCenter.x + arrowSize * 0.3f, buttonCenter.y - arrowSize),
                ImVec2(buttonCenter.x, buttonCenter.y - arrowSize * 1.5f),
                IM_COL32(50, 255, 50, 255));
                
            // Z arrow (blue, at 45 degrees)
            drawList->AddLine(
                ImVec2(buttonCenter.x - arrowSize * 0.35f, buttonCenter.y + arrowSize * 0.35f),
                ImVec2(buttonCenter.x - arrowSize * 0.7f, buttonCenter.y + arrowSize * 0.7f),
                IM_COL32(50, 50, 255, 255), 2.0f);
            drawList->AddTriangleFilled(
                ImVec2(buttonCenter.x - arrowSize * 0.7f - arrowSize * 0.2f, buttonCenter.y + arrowSize * 0.7f),
                ImVec2(buttonCenter.x - arrowSize * 0.7f, buttonCenter.y + arrowSize * 0.7f + arrowSize * 0.2f),
                ImVec2(buttonCenter.x - arrowSize * 0.7f - arrowSize * 0.3f, buttonCenter.y + arrowSize * 0.7f + arrowSize * 0.3f),
                IM_COL32(50, 50, 255, 255));
            
            ImGui::PopStyleColor(2);
            
            if (translateClicked) {
                m_currentGizmoOperation = ImGuizmo::TRANSLATE;
                Rapture::GE_CORE_INFO("Gizmo operation set to Translate");
            }
            
            ImGui::SameLine(posX + spacing);
            
            // Rotate button with icon
            if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
            else ImGui::PushStyleColor(ImGuiCol_Button, defaultBgColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
            
            bool rotateClicked = ImGui::Button("##Rotate", ImVec2(buttonSize, buttonSize));
            
            // Draw rotate icon (circular arrows)
            buttonMin = ImGui::GetItemRectMin();
            buttonMax = ImGui::GetItemRectMax();
            buttonCenter = ImVec2((buttonMin.x + buttonMax.x) * 0.5f, (buttonMin.y + buttonMax.y) * 0.5f);
            
            float radius = iconSize * 0.4f;
            float segments = 16;
            drawList->AddCircle(buttonCenter, radius, IM_COL32(255, 255, 255, 200), segments, 2.0f);
            
            // Add arrow head at the end of the circle
            float angle = 0.75f * 3.14159f * 2.0f; // About 270 degrees
            float arrowX = buttonCenter.x + radius * cosf(angle);
            float arrowY = buttonCenter.y + radius * sinf(angle);
            
            float arrowTipSize = 4.0f;
            drawList->AddTriangleFilled(
                ImVec2(arrowX, arrowY),
                ImVec2(arrowX + arrowTipSize * cosf(angle + 2.5f), arrowY + arrowTipSize * sinf(angle + 2.5f)),
                ImVec2(arrowX + arrowTipSize * cosf(angle - 2.5f), arrowY + arrowTipSize * sinf(angle - 2.5f)),
                IM_COL32(255, 255, 255, 200));
            
            ImGui::PopStyleColor(2);
            
            if (rotateClicked) {
                m_currentGizmoOperation = ImGuizmo::ROTATE;
                Rapture::GE_CORE_INFO("Gizmo operation set to Rotate");
            }
            
            ImGui::SameLine(posX + spacing * 2);
            
            // Scale button with icon
            if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
            else ImGui::PushStyleColor(ImGuiCol_Button, defaultBgColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
            
            bool scaleClicked = ImGui::Button("##Scale", ImVec2(buttonSize, buttonSize));
            
            // Draw scale icon (expanding box)
            buttonMin = ImGui::GetItemRectMin();
            buttonMax = ImGui::GetItemRectMax();
            buttonCenter = ImVec2((buttonMin.x + buttonMax.x) * 0.5f, (buttonMin.y + buttonMax.y) * 0.5f);
            
            float boxSize = iconSize * 0.4f;
            drawList->AddRect(
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y - boxSize),
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y + boxSize),
                IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);
            
            // Draw corner handles
            float handleSize = 3.0f;
            // Top-left corner
            drawList->AddLine(
                ImVec2(buttonCenter.x - boxSize - handleSize, buttonCenter.y - boxSize),
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y - boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            drawList->AddLine(
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y - boxSize - handleSize),
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y - boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            
            // Top-right corner
            drawList->AddLine(
                ImVec2(buttonCenter.x + boxSize + handleSize, buttonCenter.y - boxSize),
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y - boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            drawList->AddLine(
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y - boxSize - handleSize),
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y - boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            
            // Bottom-left corner
            drawList->AddLine(
                ImVec2(buttonCenter.x - boxSize - handleSize, buttonCenter.y + boxSize),
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y + boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            drawList->AddLine(
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y + boxSize + handleSize),
                ImVec2(buttonCenter.x - boxSize, buttonCenter.y + boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            
            // Bottom-right corner
            drawList->AddLine(
                ImVec2(buttonCenter.x + boxSize + handleSize, buttonCenter.y + boxSize),
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y + boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            drawList->AddLine(
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y + boxSize + handleSize),
                ImVec2(buttonCenter.x + boxSize, buttonCenter.y + boxSize),
                IM_COL32(255, 255, 255, 200), 2.0f);
            
            ImGui::PopStyleColor(2);
            
            if (scaleClicked) {
                m_currentGizmoOperation = ImGuizmo::SCALE;
                Rapture::GE_CORE_INFO("Gizmo operation set to Scale");
            }
            
            ImGui::EndChild();
            
            // Restore original cursor position
            ImGui::SetCursorPos(origCursorPos);
        
        
        // Get the framebuffer from the test layer
        unsigned int textureID = testLayer->getFramebuffer()->getColorAttachmentRendererID();
        
        // Check if viewport size changed and resize the framebuffer if needed
        if (viewportPanelSize.x != lastSize.x || viewportPanelSize.y != lastSize.y || firstTime) {
            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
                // Update framebuffer size to match viewport
                testLayer->getFramebuffer()->resize(
                    static_cast<unsigned int>(viewportPanelSize.x), 
                    static_cast<unsigned int>(viewportPanelSize.y));

                // Update the GBuffer size
                Rapture::DeferredRenderer::getGBuffer()->resize(
                    static_cast<unsigned int>(viewportPanelSize.x), 
                    static_cast<unsigned int>(viewportPanelSize.y));
            }
            lastSize = viewportPanelSize;
            firstTime = false;
        }
        
        // Display the framebuffer texture in ImGui
        // ImGui::Image uses void* to store the texture ID, so we need to cast it
        ImTextureID texID = (ImTextureID)(intptr_t)textureID;
        ImGui::Image(texID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
        
        // Render ImGuizmo if we have a selected entity
        renderEntityGizmo(testLayer);
        
    } else {
        ImGui::Text("Scene View not available");
    }
    
    ImGui::End();
}

void ViewportPanel::renderEntityGizmo(TestLayer* testLayer) {
    // Only proceed if we have valid data
    if (!testLayer || !testLayer->getSelectedEntity() || !m_hasCameraMatrices || !m_entityTransformCallback)
        return;
    
    // Get the selected entity
    auto selectedEntity = testLayer->getSelectedEntity();
    
    // Get the transform matrix using the callback
    glm::mat4 transformMatrix;
    if (!m_entityTransformCallback(selectedEntity, transformMatrix))
        return; // Callback returned false, can't proceed
    
    // Set up ImGuizmo
    ImGuizmo::SetOrthographic(false);  // Using perspective view
    ImGuizmo::SetDrawlist();
    
    // Set the ImGuizmo rect to match our viewport
    ImGuizmo::SetRect(
        m_viewportPosition.x, 
        m_viewportPosition.y, 
        lastSize.x, 
        lastSize.y
    );
    
    // Manipulate the transform
    ImGuizmo::Manipulate(
        glm::value_ptr(m_viewMatrix),      // View matrix (from camera callback)
        glm::value_ptr(m_projectionMatrix), // Projection matrix (from camera callback)
        m_currentGizmoOperation,           // Current operation (translate, rotate, scale)
        m_currentGizmoMode,                // Current mode (local or world)
        glm::value_ptr(transformMatrix),   // Transform matrix to manipulate
        nullptr,                           // Delta matrix (optional)
        nullptr                            // Snap values (optional)
    );
    
    // Check if gizmo is being used
    if (ImGuizmo::IsOver()) {
        auto* boundingBoxComponent = selectedEntity->tryGetComponent<Rapture::BoundingBoxComponent>();
        if (boundingBoxComponent) {
            Rapture::BoundingBoxSystem::updateBoundingBox(*selectedEntity.get());
        }
    }
    
    // If ImGuizmo changed the transform matrix, update the entity
    if (ImGuizmo::IsUsing()) {
        
        
        // Update the entity's transform component via callback
        // This is handled by the entity itself
        auto* transformComponent = selectedEntity->tryGetComponent<Rapture::TransformComponent>();
        if (transformComponent) {
            // Decompose the transform matrix
            glm::vec3 position, rotation, scale;
            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(transformMatrix),
                glm::value_ptr(position),
                glm::value_ptr(rotation),
                glm::value_ptr(scale)
            );
        
            // Update the transform component with new values using proper setters
            transformComponent->transforms.setTranslation(position);
            transformComponent->transforms.setRotation(glm::radians(rotation)); // Convert from degrees to radians
            transformComponent->transforms.setScale(scale);
            
            // Make sure to recalculate the transform matrix
            transformComponent->transforms.recalculateTransform();
            
            // Update bounding box if it exists
            if (auto* bb = selectedEntity->tryGetComponent<Rapture::BoundingBoxComponent>()) {
                bb->needsUpdate = true;
            }
            
        } else {
            Rapture::GE_CORE_ERROR("Selected entity doesn't have a TransformComponent");
        }
    }
}

void ViewportPanel::renderDepthBufferViewport(TestLayer* testLayer) {

}

bool ViewportPanel::windowToViewportCoordinates(float& viewportX, float& viewportY) const {
    // Get mouse position in screen coordinates
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    
    // Check if the point is inside the viewport
    if (!isMouseInViewport()) {
        viewportX = -1.0f;
        viewportY = -1.0f;
        return false;
    }
    
    // Convert from screen coordinates to viewport-local coordinates
    viewportX = mousePos.x - m_viewportPosition.x;
    viewportY = mousePos.y - m_viewportPosition.y;
    return true;
}

bool ViewportPanel::isMouseInViewport() const {
    // Get mouse position in screen coordinates (same space as m_viewportPosition)
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    
    // Check if screen-space mouse coordinates are inside viewport bounds
    return (mousePos.x >= m_viewportPosition.x && 
            mousePos.x < m_viewportPosition.x + lastSize.x &&
            mousePos.y >= m_viewportPosition.y && 
            mousePos.y < m_viewportPosition.y + lastSize.y);
}

// Camera matrices (from TestLayer callback)
void ViewportPanel::setCameraMatrices(const glm::mat4& view, const glm::mat4& projection) {
    m_viewMatrix = view;
    m_projectionMatrix = projection;
    m_hasCameraMatrices = true;
}