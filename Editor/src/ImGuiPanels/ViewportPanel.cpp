#include "ViewportPanel.h"
#include "Logger/Log.h"


void ViewportPanel::renderSceneViewport(TestLayer* testLayer) {
    ImGui::Begin("Scene Viewport");
    
    // Get the size of the ImGui window viewport
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    // Store the current cursor position - this is the top-left corner of the viewport
    m_viewportPosition = ImGui::GetCursorScreenPos();
    
    if (testLayer) {
        // Get the framebuffer from the test layer
        unsigned int textureID = testLayer->getFramebuffer()->getColorAttachmentRendererID();
        
        // Check if viewport size changed and resize the framebuffer if needed
        if (viewportPanelSize.x != lastSize.x || viewportPanelSize.y != lastSize.y || firstTime) {
            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
                // Update framebuffer size to match viewport
                testLayer->getFramebuffer()->resize(
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
    } else {
        ImGui::Text("Scene View not available");
    }
    
    ImGui::End();
}

void ViewportPanel::renderDepthBufferViewport(TestLayer* testLayer) {
    ImGui::Begin("Depth Buffer Viewport");
    
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    if (testLayer) {
        // Get the depth texture directly from the framebuffer
        unsigned int depthTexID = testLayer->getFramebuffer()->getDepthAttachmentRendererID();
        
        if (depthTexID) {
            // Display the depth texture (inverted Y coordinates to match OpenGL)
            ImTextureID texID = (ImTextureID)(intptr_t)depthTexID;
            ImGui::Image(texID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
            
            ImGui::Text("Raw depth buffer - may appear mostly black");
            ImGui::Text("The z-buffer stores non-linear depth values");
        } else {
            ImGui::Text("No depth attachment available");
        }
    } else {
        ImGui::Text("Depth buffer view not available");
    }
    
    ImGui::End();
}

bool ViewportPanel::windowToViewportCoordinates(float windowX, float windowY, float& viewportX, float& viewportY) const {
    // Check if the point is inside the viewport
    if (!isMouseInViewport(windowX, windowY)) {
        viewportX = -1.0f;
        viewportY = -1.0f;
        return false;
    }
    
    // Convert from window coordinates to viewport-local coordinates
    viewportX = windowX - m_viewportPosition.x;
    viewportY = windowY - m_viewportPosition.y;
    return true;
}

bool ViewportPanel::isMouseInViewport(float windowX, float windowY) const {
    // Check if the given window coordinates are inside the viewport
    return (windowX >= m_viewportPosition.x && 
            windowX < m_viewportPosition.x + lastSize.x &&
            windowY >= m_viewportPosition.y && 
            windowY < m_viewportPosition.y + lastSize.y);
}

