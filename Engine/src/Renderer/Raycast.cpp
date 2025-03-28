#include "Raycast.h"
#include "../Debug/TracyProfiler.h"
#include "../Logger/Log.h"
#include "../Scenes/Scene.h"
#include "Frustum.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "../Scenes/Components/BoundingBox.h"

namespace Rapture
{
    // Static member initialization
    std::vector<Raycast::PendingRaycast> Raycast::s_pendingRaycasts;
    std::mutex Raycast::s_raycastMutex;

    void Raycast::init()
    {
        s_pendingRaycasts.clear();
        GE_RENDER_INFO("Raycast system initialized");
    }

    void Raycast::shutdown()
    {
        std::lock_guard<std::mutex> lock(s_raycastMutex);
        s_pendingRaycasts.clear();
        GE_RENDER_INFO("Raycast system shutdown");
    }

    void Raycast::onFrameEnd(const std::vector<Entity>& visibleEntities)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        // Make a local copy of the pending raycasts to avoid long lock
        std::vector<PendingRaycast> pendingCopy;
        {
            std::lock_guard<std::mutex> lock(s_raycastMutex);
            pendingCopy = std::move(s_pendingRaycasts);
            s_pendingRaycasts.clear();
        }
        
        if (pendingCopy.empty()) {
            return; // Nothing to process
        }
        
        // Process each pending raycast
        for (auto& pendingRaycast : pendingCopy) {
            Scene* scene = pendingRaycast.scene;
            if (!scene || !pendingRaycast.callback) {
                continue;
            }
            
            float closestDistance = std::numeric_limits<float>::max();
            std::optional<RaycastHit> closestHit;
            
            // Only test against visible entities
            for (auto entity : visibleEntities) {
                if (!entity.hasComponent<BoundingBoxComponent>()) {
                    continue;
                }
                
                auto& boundingBoxComponent = entity.getComponent<BoundingBoxComponent>();
                
                // Skip if the bounding box needs update
                if (boundingBoxComponent.needsUpdate) {
                    continue;
                }
                
                float distance = 0.0f;
                glm::vec3 hitPoint;
                
                if (rayIntersectsBoundingBox(
                    pendingRaycast.origin, 
                    pendingRaycast.direction, 
                    boundingBoxComponent.worldBoundingBox, 
                    distance, 
                    hitPoint))
                {
                    // Only update if this hit is closer than previous hits
                    if (distance < closestDistance) {
                        closestDistance = distance;
                        
                        RaycastHit hit;
                        hit.entity = entity;
                        hit.distance = distance;
                        hit.hitPoint = hitPoint;
                        closestHit = hit;
                    }
                }
            }
            
            // Invoke the callback with the result
            pendingRaycast.callback(closestHit);
        }
    }

    glm::vec3 Raycast::screenToWorldRay(
        float screenX, float screenY,
        float screenWidth, float screenHeight,
        const glm::mat4& projectionMatrix,
        const glm::mat4& viewMatrix)
    {
        RAPTURE_PROFILE_FUNCTION();

        // Convert screen coordinates to normalized device coordinates (-1 to 1)
        float ndcX = (2.0f * screenX) / screenWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY) / screenHeight; // Flip Y for OpenGL/Vulkan coordinates

        // Create the ray in clip space
        glm::vec4 clipCoords(ndcX, ndcY, -1.0f, 1.0f);

        // Convert to eye space
        glm::mat4 invProjection = glm::inverse(projectionMatrix);
        glm::vec4 eyeCoords = invProjection * clipCoords;
        eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f); // Reset Z to look along -Z, w=0 for direction

        // Convert to world space
        glm::mat4 invView = glm::inverse(viewMatrix);
        glm::vec4 worldRay = invView * eyeCoords;
        glm::vec3 rayDirection = glm::normalize(glm::vec3(worldRay));

        return rayDirection;
    }

    bool Raycast::rayIntersectsBoundingBox(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const BoundingBox& boundingBox,
        float& outDistance,
        glm::vec3& outHitPoint)
    {
        RAPTURE_PROFILE_FUNCTION();

        // Early out for invalid bounding boxes
        if (!boundingBox.isValid()) {
            return false;
        }

        glm::vec3 boxMin = boundingBox.getMin();
        glm::vec3 boxMax = boundingBox.getMax();
        
        // Check for division by zero in rayDirection
        glm::vec3 invRayDir;
        invRayDir.x = rayDirection.x != 0.0f ? 1.0f / rayDirection.x : std::numeric_limits<float>::infinity();
        invRayDir.y = rayDirection.y != 0.0f ? 1.0f / rayDirection.y : std::numeric_limits<float>::infinity();
        invRayDir.z = rayDirection.z != 0.0f ? 1.0f / rayDirection.z : std::numeric_limits<float>::infinity();

        // Calculate intersections with slab method
        float t1 = (boxMin.x - rayOrigin.x) * invRayDir.x;
        float t2 = (boxMax.x - rayOrigin.x) * invRayDir.x;
        float t3 = (boxMin.y - rayOrigin.y) * invRayDir.y;
        float t4 = (boxMax.y - rayOrigin.y) * invRayDir.y;
        float t5 = (boxMin.z - rayOrigin.z) * invRayDir.z;
        float t6 = (boxMax.z - rayOrigin.z) * invRayDir.z;

        float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

        // Ray misses the box or box is behind the ray
        if (tmax < 0 || tmin > tmax) {
            return false;
        }

        // Hit distance is either tmin or 0 if the ray starts inside the box
        outDistance = tmin < 0 ? 0.0f : tmin;
        outHitPoint = rayOrigin + rayDirection * outDistance;
        return true;
    }

    std::vector<RaycastHit> Raycast::raycastAll(
        Scene* scene,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        std::vector<RaycastHit> hits;
        
        if (!scene) {
            GE_RENDER_ERROR("Raycast::raycastAll: Scene is null");
            return hits;
        }

        auto& registry = scene->getRegistry();
        auto view = registry.view<BoundingBoxComponent>();
        
        for (auto entityHandle : view) {
            Entity entity(entityHandle, scene);
            auto& boundingBoxComponent = entity.getComponent<BoundingBoxComponent>();
            
            // Skip if the bounding box needs update
            if (boundingBoxComponent.needsUpdate) {
                continue;
            }
            
            float distance = 0.0f;
            glm::vec3 hitPoint;
            
            if (rayIntersectsBoundingBox(
                rayOrigin, 
                rayDirection, 
                boundingBoxComponent.worldBoundingBox, 
                distance, 
                hitPoint))
            {
                RaycastHit hit;
                hit.entity = entity;
                hit.distance = distance;
                hit.hitPoint = hitPoint;
                hits.push_back(hit);
            }
        }
        
        // Sort hits by distance (closest first)
        std::sort(hits.begin(), hits.end(), [](const RaycastHit& a, const RaycastHit& b) {
            return a.distance < b.distance;
        });
        
        return hits;
    }

    std::optional<RaycastHit> Raycast::raycastClosest(
        Scene* scene,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        if (!scene) {
            GE_RENDER_ERROR("Raycast::raycastClosest: Scene is null");
            return std::nullopt;
        }

        auto& registry = scene->getRegistry();
        auto view = registry.view<BoundingBoxComponent>();
        
        float closestDistance = std::numeric_limits<float>::max();
        std::optional<RaycastHit> closestHit;
        
        for (auto entityHandle : view) {
            Entity entity(entityHandle, scene);
            auto& boundingBoxComponent = entity.getComponent<BoundingBoxComponent>();
            
            // Skip if the bounding box needs update
            if (boundingBoxComponent.needsUpdate) {
                continue;
            }
            
            float distance = 0.0f;
            glm::vec3 hitPoint;
            
            if (rayIntersectsBoundingBox(
                rayOrigin, 
                rayDirection, 
                boundingBoxComponent.worldBoundingBox, 
                distance, 
                hitPoint))
            {
                // Only update if this hit is closer than previous hits
                if (distance < closestDistance) {
                    closestDistance = distance;
                    
                    RaycastHit hit;
                    hit.entity = entity;
                    hit.distance = distance;
                    hit.hitPoint = hitPoint;
                    closestHit = hit;
                }
            }
        }
        
        return closestHit;
    }

    void Raycast::queueRaycast(
        float screenX, float screenY,
        float screenWidth, float screenHeight,
        Scene* scene,
        const glm::mat4& projectionMatrix,
        const glm::mat4& viewMatrix,
        RaycastCallback callback)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        if (!scene) {
            GE_RENDER_ERROR("Raycast::queueRaycast: Scene is null");
            if (callback) {
                callback(std::nullopt);
            }
            return;
        }
        
        // Get camera position (inverse view origin)
        glm::mat4 invView = glm::inverse(viewMatrix);
        glm::vec3 cameraPosition = glm::vec3(invView[3]);
        
        // Generate ray direction
        glm::vec3 rayDirection = screenToWorldRay(
            screenX, screenY, 
            screenWidth, screenHeight, 
            projectionMatrix, viewMatrix);
        
        // Create pending raycast
        PendingRaycast pendingRaycast;
        pendingRaycast.scene = scene;
        pendingRaycast.origin = cameraPosition;
        pendingRaycast.direction = rayDirection;
        pendingRaycast.callback = callback;
        
        // Add to queue
        std::lock_guard<std::mutex> lock(s_raycastMutex);
        s_pendingRaycasts.push_back(pendingRaycast);
    }

    std::optional<RaycastHit> Raycast::raycastFromScreen(
        float screenX, float screenY,
        float screenWidth, float screenHeight,
        Scene* scene,
        const glm::mat4& projectionMatrix,
        const glm::mat4& viewMatrix)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        if (!scene) {
            GE_RENDER_ERROR("Raycast::raycastFromScreen: Scene is null");
            return std::nullopt;
        }
        
        // Get camera position (inverse view origin)
        glm::mat4 invView = glm::inverse(viewMatrix);
        glm::vec3 cameraPosition = glm::vec3(invView[3]);
        
        // Generate ray direction
        glm::vec3 rayDirection = screenToWorldRay(
            screenX, screenY, 
            screenWidth, screenHeight, 
            projectionMatrix, viewMatrix);
        
        // Perform immediate raycast
        return raycastClosest(scene, cameraPosition, rayDirection);
    }

} // namespace Rapture 