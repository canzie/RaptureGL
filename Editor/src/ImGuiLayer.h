#pragma once

#include "Layers/Layer.h"
#include "Events/Events.h"
#include "Events/MouseEvents.h"
#include "Events/InputEvents.h"

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
};

} 