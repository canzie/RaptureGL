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
#include "Scenes/Entity.h"


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

private:
    float m_Time = 0.0f;
    float m_FontScale = 1.5f; // Default font scale
    

    // Panel instances
    EntityBrowserPanel m_EntityBrowserPanel;
    PropertiesPanel m_PropertiesPanel;
    ViewportPanel m_ViewportPanel;
    StatsPanel m_StatsPanel;
    LogPanel m_LogPanel;
    AssetsPanel m_AssetsPanel; // Using global namespace for AssetsPanel
    SettingsPanel* m_SettingsPanel = nullptr; // Created after Window context is available
    
    // Currently selected entity (shared between panels through callbacks)
    Rapture::Entity m_SelectedEntity;
};

