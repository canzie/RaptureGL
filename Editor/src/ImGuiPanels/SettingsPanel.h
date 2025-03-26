#pragma once

#include <imgui.h>
#include <string>
#include "../../Engine/src/WindowContext/OpenGLWindowContext/OpenGLWindowContext.h"

namespace Rapture {
    class WindowContext;
}

class SettingsPanel {
public:
    SettingsPanel(Rapture::WindowContext* context);
    ~SettingsPanel() = default;

    void render();

    // Accessor for triple buffering state
    bool isTripleBufferingEnabled() const { 
        return m_currentSwapMode == Rapture::SwapMode::AdaptiveVSync ||
               m_currentSwapMode == Rapture::SwapMode::TripleBuffering;
    }

private:
    enum class TabType {
        Graphics,
        Performance,
        Rendering
    };

    void renderGraphicsSettings();
    void renderPerformanceSettings();
    void renderRenderingSettings();
    
    // Window context reference for applying settings
    Rapture::WindowContext* m_windowContext = nullptr;
    
    // UI state
    bool m_vsyncEnabled = false;
    bool m_tripleBufferingEnabled = false;
    bool m_frustumCullingEnabled = true; // Default to true
    
    // Current settings
    Rapture::SwapMode m_currentSwapMode = Rapture::SwapMode::Immediate;
    
    // Tab management
    TabType m_activeTab = TabType::Graphics;
}; 