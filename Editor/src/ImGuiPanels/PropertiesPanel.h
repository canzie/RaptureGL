#pragma once

#include "Scenes/Scene.h"
#include "Scenes/Entity.h"
#include "Scenes/Components/Components.h"
#include "Scenes/Components/BoundingBox.h"
#include "ImGuiPanels/EntityBrowserPanel.h"
#include "Textures/Texture.h"
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include <string>
#include <functional>

// Forward declarations
namespace Rapture {
    class Material;
}

class PropertiesPanel {
public:
    PropertiesPanel() : 
        positionLocked(false), 
        rotationLocked(false), 
        scaleLocked(false) {}
    ~PropertiesPanel() = default;

    // Original method that gets entity from EntityBrowserPanel
    void render(std::shared_ptr<Rapture::Entity> entity);
    void render();

    // Helper function to display a help marker with tooltip
    static void HelpMarker(const char* desc);
    
    // Callback for texture viewing - replaces direct panel reference
    using TextureViewCallback = std::function<void(std::shared_ptr<Rapture::Texture2D>)>;
    void setTextureViewCallback(TextureViewCallback callback) { m_TextureViewCallback = callback; }

private:
    // Transform component UI state
    bool positionLocked;
    bool rotationLocked;
    bool scaleLocked;
    glm::vec3 lastScale = glm::vec3(1.0f);
    
    // Light component UI state
    int selectedLightType = 0;
    
    // Material UI state
    std::string selectedTextureName;
    
    // Callback for texture viewing
    TextureViewCallback m_TextureViewCallback;
    
    // Helper method to render entity properties
    void renderEntityProperties(std::shared_ptr<Rapture::Entity> entity);
    void drawMaterialTextures(std::shared_ptr<Rapture::Entity> entity);
    const char* getLightTypeString(int type);
    
    // Helper method to render animation controls
    void renderAnimationControls(std::shared_ptr<Rapture::Entity> entity);
    
    // Helper method to render skeleton bone transforms
    void renderSkeletonBones(std::shared_ptr<Rapture::Entity> entity);
    
    // Added methods for material drag-drop support
    void renderMaterialComponent(std::shared_ptr<Rapture::Entity> entity);
    bool handleMaterialDragDrop(std::shared_ptr<Rapture::Entity> entity);
    
    // Helper method to render shadow component
    void renderShadowComponent(std::shared_ptr<Rapture::Entity> entity);
    
    // Helper method to render cascaded shadow component
    void renderCascadedShadowComponent(std::shared_ptr<Rapture::Entity> entity);
    
    // Helper method to render compute texture component
    void renderComputeTextureComponent(std::shared_ptr<Rapture::Entity> entity);
};

