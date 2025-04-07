#pragma once

#include "Layers/Layer.h"
#include "Events/Events.h"
#include "Events/MouseEvents.h"
#include "Events/InputEvents.h"
#include "ImGuiPanels/EntityBrowserPanel.h"
#include "ImGuiPanels/PropertiesPanel.h"
#include "ImGuiPanels/ViewportPanel.h"
#include "ImGuiPanels/StatsPanel.h"
#include "ImGuiPanels/LogPanel.h"
#include "ImGuiPanels/AssetsPanel.h"
#include "ImGuiPanels/SettingsPanel.h"
#include "ImGuiPanels/MaterialViewerPanel.h"
#include "ImGuiPanels/DebugViewPanel.h"
#include "Scenes/Entity.h"
#include "vendor/ImGuizmo/ImGuizmo.h"


class ImGuiLayer : public Rapture::Layer
{
public:
    ImGuiLayer();
    ~ImGuiLayer() = default;
    
    virtual void onAttach() override;
    virtual void onDetach() override;
    virtual void onUpdate(float ts) override;
    virtual void onEvent(Rapture::Event& event) override;
    
    void begin(); // Begin new ImGui frame
    void end();   // End ImGui frame and render
    
    // Getter/setter for the GBuffer debug view panel
    bool isGBufferDebugEnabled() const { return m_showGBufferDebug; }
    void setGBufferDebugEnabled(bool enabled) { m_showGBufferDebug = enabled; }

private:
    float m_Time = 0.0f;
    float m_FontScale = 1.5f; // Default font scale
    
    // GBuffer debug view toggle
    bool m_showGBufferDebug = true; // Default to true as requested
    
    // Panel instances
    EntityBrowserPanel m_EntityBrowserPanel;
    PropertiesPanel m_PropertiesPanel;
    ViewportPanel m_ViewportPanel;
    StatsPanel m_StatsPanel;
    LogPanel m_LogPanel;
    AssetsPanel m_AssetsPanel; // Using global namespace for AssetsPanel
    MaterialViewerPanel m_MaterialViewerPanel; // Material viewer panel
    DebugViewPanel m_DebugViewPanel; // GBuffer debug view panel
    SettingsPanel* m_SettingsPanel = nullptr; // Created after Window context is available
    
    // Currently selected entity (shared between panels through callbacks)
    std::shared_ptr<Rapture::Entity> m_SelectedEntity;
};

