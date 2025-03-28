#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <optional>
#include <functional>
#include <mutex>
#include "../Scenes/Entity.h"
#include "../Scenes/Components/BoundingBox.h"

namespace Rapture
{
    struct RaycastHit
    {
        Entity entity;
        float distance;
        glm::vec3 hitPoint;
    };

    // Forward declaration
    class Frustum;

    class Raycast
    {
    public:
        // Callback type for raycast results
        using RaycastCallback = std::function<void(const std::optional<RaycastHit>&)>;
        
        // Initialize the raycast system
        static void init();
        
        // Shutdown the raycast system
        static void shutdown();
        
        // Process all pending raycasts after frame rendering
        static void onFrameEnd(const std::vector<Entity>& visibleEntities);
        
        // Create a ray from screen coordinates
        static glm::vec3 screenToWorldRay(
            float screenX, float screenY,
            float screenWidth, float screenHeight,
            const glm::mat4& projectionMatrix,
            const glm::mat4& viewMatrix);

        // Test a ray against a bounding box
        static bool rayIntersectsBoundingBox(
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection,
            const BoundingBox& boundingBox,
            float& outDistance,
            glm::vec3& outHitPoint);

        // Find all entities hit by a ray
        static std::vector<RaycastHit> raycastAll(
            Scene* scene,
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection);

        // Find the closest entity hit by a ray
        static std::optional<RaycastHit> raycastClosest(
            Scene* scene,
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection);
            
        // Queue a raycast from screen coordinates to be processed at frame end
        // The callback will be invoked with the result when processing is complete
        static void queueRaycast(
            float screenX, float screenY,
            float screenWidth, float screenHeight,
            Scene* scene,
            const glm::mat4& projectionMatrix,
            const glm::mat4& viewMatrix,
            RaycastCallback callback);
        
        // Shorthand method to immediately cast a ray from screen coordinates
        // Note: This doesn't benefit from frustum culling optimization
        static std::optional<RaycastHit> raycastFromScreen(
            float screenX, float screenY,
            float screenWidth, float screenHeight,
            Scene* scene,
            const glm::mat4& projectionMatrix,
            const glm::mat4& viewMatrix);

    private:
        struct PendingRaycast {
            Scene* scene;
            glm::vec3 origin;
            glm::vec3 direction;
            RaycastCallback callback;
        };
        
        static std::vector<PendingRaycast> s_pendingRaycasts;
        static std::mutex s_raycastMutex;
    };

} // namespace Rapture 