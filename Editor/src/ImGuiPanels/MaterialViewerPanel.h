#pragma once

#include "imgui.h"
#include <memory>

namespace Rapture {
    class Framebuffer;
}

class MaterialViewerPanel {
public:
    MaterialViewerPanel() : m_firstTime(true) {}
    ~MaterialViewerPanel() = default;

    // Render the material viewer panel with a given framebuffer
    void render(const std::shared_ptr<Rapture::Framebuffer>& framebuffer);

    // Get the viewport size
    ImVec2 getViewportSize() const { return m_lastSize; }

private:
    ImVec2 m_viewportPosition;  // Window position
    ImVec2 m_lastSize;          // Last known viewport size
    bool m_firstTime;           // First render flag
};
