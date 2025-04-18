#pragma once

#include "imgui.h"
#include "Textures/Texture.h"
#include "PanelComponents.h"

#include <memory>
#include <string>

class TextureViewPanel {
public:
    // Default constructor with proper initialization
    TextureViewPanel() 
        : m_enabled(true)
        , m_panelName("Texture Viewer")
        , m_tintColor(1.0f, 1.0f, 1.0f, 1.0f)
        , m_borderColor(0.0f, 0.0f, 0.0f, 0.0f)
        , m_zoomLevel(1.0f)
    {
        // Initialize texture data with safe defaults
        m_textureData.textureID = 0;
        m_textureData.width = 100.0f;
        m_textureData.height = 100.0f;
        m_textureData.uv0 = ImVec2(0, 0);
        m_textureData.uv1 = ImVec2(1, 1);
    }
    
    // Constructor with Texture2D
    TextureViewPanel(std::shared_ptr<Rapture::Texture2D> texture);
    
    // Constructor with raw texture ID
    TextureViewPanel(uint64_t textureID, float width = 0.0f, float height = 0.0f);
    
    // Render the panel
    void render();
    
    // Set texture methods
    void setTexture(std::shared_ptr<Rapture::Texture2D> texture);
    void setTexture(uint64_t textureID, float width = 0.0f, float height = 0.0f);
    
    // Enable/disable the panel
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    // Set panel name
    void setPanelName(const std::string& name) { m_panelName = name; }
    
private:
    // Texture data
    TextureDisplayData m_textureData;
    
    // Panel state
    bool m_enabled = true;  // Start enabled by default
    std::string m_panelName = "Texture Viewer";
    
    // Rendering options
    ImVec4 m_tintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 m_borderColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    float m_zoomLevel = 1.0f;
    
    // Helper methods
    void setupTextureData(std::shared_ptr<Rapture::Texture2D> texture);
};
