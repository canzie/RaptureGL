#pragma once

#include "TestLayer.h"
#include "imgui.h"


class ViewportPanel {
public:
    ViewportPanel() = default;
    ~ViewportPanel() = default;

    void renderSceneViewport(TestLayer* testLayer);
    void renderDepthBufferViewport(TestLayer* testLayer);
    
    // Get the absolute position of the viewport in window coordinates
    ImVec2 getViewportPosition() const { return m_viewportPosition; }
    
    // Get the size of the viewport
    ImVec2 getViewportSize() const { return lastSize; }
    
    // Convert window mouse coordinates to viewport-local coordinates
    // Returns true if the mouse is inside the viewport, false otherwise
    bool windowToViewportCoordinates(float windowX, float windowY, float& viewportX, float& viewportY) const;
    
    // Check if a point (in window coordinates) is inside the viewport
    bool isMouseInViewport(float windowX, float windowY) const;

private:
    ImVec2 lastSize = ImVec2(0, 0);
    ImVec2 m_viewportPosition = ImVec2(0, 0); // Top-left corner of the viewport in window coordinates
    bool firstTime = true;
};

