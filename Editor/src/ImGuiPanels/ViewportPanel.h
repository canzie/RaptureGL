#pragma once

#include "TestLayer.h"
#include "imgui.h"

namespace Rapture {

class ViewportPanel {
public:
    ViewportPanel() = default;
    ~ViewportPanel() = default;

    void renderSceneViewport(TestLayer* testLayer);
    void renderDepthBufferViewport(TestLayer* testLayer);

private:
    ImVec2 lastSize = ImVec2(0, 0);
    bool firstTime = true;
};

}  // namespace Rapture 