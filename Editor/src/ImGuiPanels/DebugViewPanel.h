#pragma once

#include "imgui.h"
#include <memory>

namespace Rapture
{
    class GBuffer;
}

class DebugViewPanel
{
public:
    DebugViewPanel() = default;
    ~DebugViewPanel() = default;

    void render();

    // Return true if the panel is enabled and should be rendered
    bool isEnabled() const { return m_enabled; }
    
    // Enable or disable the panel
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled = true;
    ImVec2 m_lastSize = ImVec2(0, 0);
    
    // Helper method to display a texture with label
    void displayTexture(const char* label, uint32_t textureID, ImVec2 size);
};
