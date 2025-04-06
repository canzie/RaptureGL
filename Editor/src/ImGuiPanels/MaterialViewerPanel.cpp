#include "MaterialViewerPanel.h"
#include "Renderer/Framebuffer.h"

void MaterialViewerPanel::render(const std::shared_ptr<Rapture::Framebuffer>& framebuffer) {
    ImGui::Begin("Material Viewer");
    
    // Get the size of the ImGui window viewport
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    // Store the current cursor position - this is the top-left corner of the viewport
    m_viewportPosition = ImGui::GetCursorScreenPos();
    
    if (framebuffer) {
        // Get the texture ID from the framebuffer
        unsigned int textureID = framebuffer->getColorAttachmentRendererID();
        
        // Check if viewport size changed and resize the framebuffer if needed
        if (viewportPanelSize.x != m_lastSize.x || viewportPanelSize.y != m_lastSize.y || m_firstTime) {
            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
                // Update framebuffer size to match viewport
                framebuffer->resize(
                    static_cast<unsigned int>(viewportPanelSize.x), 
                    static_cast<unsigned int>(viewportPanelSize.y));
            }
            m_lastSize = viewportPanelSize;
            m_firstTime = false;
        }
        
        // Display the framebuffer texture in ImGui
        // ImGui::Image uses void* to store the texture ID, so we need to cast it
        ImTextureID texID = (ImTextureID)(intptr_t)textureID;
        ImGui::Image(texID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::Text("Material View not available");
    }
    
    ImGui::End();
} 