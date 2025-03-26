#include "ViewportPanel.h"
#include "Logger/Log.h"


void ViewportPanel::renderSceneViewport(TestLayer* testLayer) {
    ImGui::Begin("Scene Viewport");
    
    // Get the size of the ImGui window viewport
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
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

