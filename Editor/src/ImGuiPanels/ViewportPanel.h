#pragma once

#include "imgui.h"
#include <glm/glm.hpp>
#include "../vendor/ImGuizmo/ImGuizmo.h"
#include <functional>
#include <memory>

class TestLayer;
namespace Rapture { class Entity; }

// Callback types
using EntityTransformCallback = std::function<bool(std::shared_ptr<Rapture::Entity>, glm::mat4&)>;

class ViewportPanel {
public:
    ViewportPanel() : firstTime(true) {}
    ~ViewportPanel() = default;

    void renderSceneViewport(TestLayer* testLayer);
    void renderDepthBufferViewport(TestLayer* testLayer);
    
    // Get the absolute position of the viewport in window coordinates
    ImVec2 getViewportPosition() const { return m_viewportPosition; }
    
    // Get the size of the viewport
    ImVec2 getViewportSize() const { return lastSize; }
    
    // Convert window mouse coordinates to viewport-local coordinates
    // Returns true if the mouse is inside the viewport, false otherwise
    bool windowToViewportCoordinates(float& viewportX, float& viewportY) const;
    
    // Check if a point (in window coordinates) is inside the viewport
    bool isMouseInViewport() const;

    // Add a method to render ImGuizmo for the selected entity
    void renderEntityGizmo(TestLayer* testLayer);
    
    // ImGuizmo state controls
    ImGuizmo::OPERATION getCurrentGizmoOperation() const { return m_currentGizmoOperation; }
    ImGuizmo::MODE getCurrentGizmoMode() const { return m_currentGizmoMode; }
    
    void setGizmoOperation(ImGuizmo::OPERATION operation) { m_currentGizmoOperation = operation; }
    void setGizmoMode(ImGuizmo::MODE mode) { m_currentGizmoMode = mode; }
    
    // Toggle between local and world mode
    void toggleGizmoMode() {
        m_currentGizmoMode = (m_currentGizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }
    
    // Callback setters for ImGuizmo
    void setEntityTransformCallback(EntityTransformCallback callback) { m_entityTransformCallback = callback; }
    
    // Camera matrices (from TestLayer callback)
    void setCameraMatrices(const glm::mat4& view, const glm::mat4& projection);

private:
    ImVec2 m_viewportPosition;  // Window position
    ImVec2 lastSize;           // Last known viewport size
    bool firstTime;            // First render flag
    
    // ImGuizmo state
    ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_currentGizmoMode = ImGuizmo::WORLD;
    
    // Callback for entity transform manipulation
    EntityTransformCallback m_entityTransformCallback;
    
    // Camera matrices (obtained via callback)
    glm::mat4 m_viewMatrix = glm::mat4(1.0f);
    glm::mat4 m_projectionMatrix = glm::mat4(1.0f);
    bool m_hasCameraMatrices = false;
};

