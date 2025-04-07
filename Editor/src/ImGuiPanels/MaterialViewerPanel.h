#pragma once

#include "imgui.h"
#include <memory>
#include <string>

namespace Rapture {
    class Framebuffer;
    class Material;
    class Sphere;
    using AssetHandle = uint64_t;
}

class MaterialViewerPanel {
public:
    MaterialViewerPanel() : m_firstTime(true) {}
    ~MaterialViewerPanel() = default;

    // Render the material viewer panel with a given framebuffer and sphere
    void render(const std::shared_ptr<Rapture::Framebuffer>& framebuffer, 
                const std::shared_ptr<Rapture::Sphere>& sphere);

    // Get the viewport size
    ImVec2 getViewportSize() const { return m_lastSize; }

private:
    // Helper method to render material properties
    void renderMaterialProperties(const std::shared_ptr<Rapture::Material>& material);
    
    // Helper method to edit PBR material properties
    void editPBRProperties(const std::shared_ptr<Rapture::Material>& material);
    
    // Helper method to edit texture properties
    void editTextureProperties(const std::shared_ptr<Rapture::Material>& material);
    
    // Helper method to handle material drag and drop
    bool handleMaterialDragDrop(const std::shared_ptr<Rapture::Sphere>& sphere);

    ImVec2 m_viewportPosition;  // Window position
    ImVec2 m_lastSize;          // Last known viewport size
    bool m_firstTime;           // First render flag
    
    // Selected texture for preview
    std::string m_selectedTextureName;
};
