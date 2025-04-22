#include "TextureViewPanel.h"
#include "Logger/Log.h"

#include "Renderer/RadianceCascades/RadianceCascades.h"

TextureViewPanel::TextureViewPanel(std::shared_ptr<Rapture::Texture2D> texture) 
    : m_enabled(true)
    , m_panelName("Texture Viewer")
    , m_tintColor(1.0f, 1.0f, 1.0f, 1.0f)
    , m_borderColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_zoomLevel(1.0f)
{
    // Clear texture data to default safe values with inverted UVs
    m_textureData.textureID = 0;
    m_textureData.width = 100.0f;
    m_textureData.height = 100.0f;
    m_textureData.uv0 = ImVec2(0, 1); // Inverted top-left
    m_textureData.uv1 = ImVec2(1, 0); // Inverted bottom-right
    
    if (texture) {
        try {
            m_textureData.textureID = texture->getRendererID();
            m_textureData.width = static_cast<float>(texture->getWidth());
            m_textureData.height = static_cast<float>(texture->getHeight());
        } catch (...) {
            Rapture::GE_ERROR("TextureViewPanel: Failed to initialize texture data");
        }
    }
}

TextureViewPanel::TextureViewPanel(uint64_t textureID, float width, float height)
    : m_enabled(true)
    , m_panelName("Texture Viewer")
    , m_tintColor(1.0f, 1.0f, 1.0f, 1.0f)
    , m_borderColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_zoomLevel(1.0f)
{
    // Set texture data with proper defaults and inverted UVs
    m_textureData.textureID = textureID;
    m_textureData.width = width > 0.0f ? width : 100.0f;
    m_textureData.height = height > 0.0f ? height : 100.0f;
    m_textureData.uv0 = ImVec2(0, 1); // Inverted top-left
    m_textureData.uv1 = ImVec2(1, 0); // Inverted bottom-right
}

void TextureViewPanel::setTexture(std::shared_ptr<Rapture::Texture2D> texture)
{
    if (!texture) {
        Rapture::GE_WARN("TextureViewPanel: Null texture provided");
        m_textureData.textureID = 0;
        m_textureData.uv0 = ImVec2(0, 1); // Reset to inverted default
        m_textureData.uv1 = ImVec2(1, 0); // Reset to inverted default
        return;
    }
    
    try {
        m_textureData.textureID = texture->getRendererID();
        m_textureData.width = static_cast<float>(texture->getWidth());
        m_textureData.height = static_cast<float>(texture->getHeight());
        m_textureData.uv0 = ImVec2(0, 1); // Set inverted UVs
        m_textureData.uv1 = ImVec2(1, 0); // Set inverted UVs
        m_zoomLevel = 1.0f;
    } catch (...) {
        Rapture::GE_ERROR("TextureViewPanel: Failed to set texture data");
        m_textureData.textureID = 0;
    }
}

void TextureViewPanel::setTexture(uint64_t textureID, float width, float height)
{
    m_textureData.textureID = textureID;
    m_textureData.width = width > 0.0f ? width : 100.0f;
    m_textureData.height = height > 0.0f ? height : 100.0f;
    m_textureData.uv0 = ImVec2(0, 1); // Set inverted UVs
    m_textureData.uv1 = ImVec2(1, 0); // Set inverted UVs
    m_zoomLevel = 1.0f;
}

void TextureViewPanel::setupTextureData(std::shared_ptr<Rapture::Texture2D> texture)
{
    // Simple direct initialization, no complex logic
    if (!texture) {
        m_textureData.textureID = 0;
        return;
    }
    
    try {
        m_textureData.textureID = texture->getRendererID();
        m_textureData.width = static_cast<float>(texture->getWidth());
        m_textureData.height = static_cast<float>(texture->getHeight());
    } catch (...) {
        m_textureData.textureID = 0;
        Rapture::GE_ERROR("TextureViewPanel: Exception in setupTextureData");
    }
}

void TextureViewPanel::render()
{
    // Only attempt to render if the panel is enabled
    if (!m_enabled) {
        return;
    }
    
    // Begin panel with reference to enable state for close button
    bool panelOpen = m_enabled;
    if (ImGui::Begin(m_panelName.c_str(), &panelOpen)) {
        // Update enabled state from ImGui
        m_enabled = panelOpen;
        
        // Display texture info, but only if we have a valid texture
        if (m_textureData.textureID != 0) {
            ImGui::Text("Texture ID: %llu", m_textureData.textureID);
            
            if (m_textureData.width > 0 && m_textureData.height > 0) {
                ImGui::Text("Dimensions: %dx%d", 
                           static_cast<int>(m_textureData.width), 
                           static_cast<int>(m_textureData.height));
                
                // Add zoom slider
                if (ImGui::SliderFloat("Zoom", &m_zoomLevel, 0.1f, 5.0f)) {
                    // Update UV coordinates based on zoom with inverted Y
                    float zoomFactor = 1.0f / m_zoomLevel;
                    float offsetX = (1.0f - zoomFactor) * 0.5f;
                    float offsetY = (1.0f - zoomFactor) * 0.5f;
                    m_textureData.uv0 = ImVec2(offsetX, 1.0f - offsetY); // Inverted V0
                    m_textureData.uv1 = ImVec2(1.0f - offsetX, offsetY);   // Inverted V1
                }
                
                // Calculate display size based on content area and aspect ratio
                ImVec2 contentSize = ImGui::GetContentRegionAvail();
                ImVec2 displaySize;
                
                // Maintain aspect ratio
                float aspectRatio = m_textureData.width / m_textureData.height;
                if (aspectRatio > 1.0f) {
                    // Width-constrained
                    displaySize.x = contentSize.x;
                    displaySize.y = contentSize.x / aspectRatio;
                } else {
                    // Height-constrained (or square)
                    displaySize.y = contentSize.y;
                    displaySize.x = contentSize.y * aspectRatio;
                    
                    // But ensure we don't exceed the width
                    if (displaySize.x > contentSize.x) {
                        displaySize.x = contentSize.x;
                        displaySize.y = contentSize.x / aspectRatio;
                    }
                }
                
                // Render the image directly - no helper functions
                try {
                    ImGui::Image(
                        (ImTextureID)m_textureData.textureID,
                        displaySize,
                        m_textureData.uv0,
                        m_textureData.uv1,
                        m_tintColor,
                        m_borderColor
                    );
                } catch (...) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                                      "Error rendering texture");
                }
            }
        } else {
            // No texture
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "No texture selected");
            ImGui::TextWrapped("Use the 'View in Texture Viewer' button from a texture component to display a texture here.");
        }
    }
    ImGui::End();
} 