#pragma once

#include "Layers/Layer.h"
#include "Events/Events.h"
#include "Events/MouseEvents.h"
#include "Events/InputEvents.h"
#include "ImGuiPanels/EntityBrowserPanel.h"
#include "ImGuiPanels/PropertiesPanel.h"
#include "ImGuiPanels/ViewportPanel.h"
#include "ImGuiPanels/StatsPanel.h"


namespace Rapture {

class ImGuiLayer : public Layer
{
public:
    ImGuiLayer();
    ~ImGuiLayer() = default;
    
    virtual void onAttach() override;
    virtual void onDetach() override;
    virtual void onUpdate(float ts) override;
    virtual void onEvent(Event& event) override;
    
    void begin(); // Begin new ImGui frame
    void end();   // End ImGui frame and render

private:
    float m_Time = 0.0f;
    // Panel instances
    EntityBrowserPanel m_EntityBrowserPanel;
    PropertiesPanel m_PropertiesPanel;
    ViewportPanel m_ViewportPanel;
    StatsPanel m_StatsPanel;
};

} 