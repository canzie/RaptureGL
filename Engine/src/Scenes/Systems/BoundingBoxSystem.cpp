#include "BoundingBoxSystem.h"
#include "../../Debug/TracyProfiler.h"

namespace Rapture {

    BoundingBox BoundingBoxSystem::calculateFromVertexData(const void* data, size_t dataSize, size_t stride, size_t positionOffset) {
        if (!data || dataSize == 0 || stride < 3) {
            return BoundingBox(); // Return invalid bounding box
        }

        RAPTURE_PROFILE_SCOPE("Calculate Bounding Box from Vertex Data");
        
        // Cast the void pointer to float pointer for vertex coordinates
        const float* vertexData = static_cast<const float*>(data);
        size_t vertexCount = dataSize / (stride * sizeof(float));
        
        glm::vec3 min(std::numeric_limits<float>::max());
        glm::vec3 max(std::numeric_limits<float>::lowest());
        
        // Iterate through each vertex position
        for (size_t i = 0; i < vertexCount; i++) {
            size_t baseIndex = i * stride + positionOffset;
            
            // Make sure we don't go out of bounds
            if (baseIndex + 2 >= dataSize / sizeof(float)) {
                break;
            }
            
            float x = vertexData[baseIndex];
            float y = vertexData[baseIndex + 1];
            float z = vertexData[baseIndex + 2];
            
            // Update min and max
            min.x = std::min(min.x, x);
            min.y = std::min(min.y, y);
            min.z = std::min(min.z, z);
            
            max.x = std::max(max.x, x);
            max.y = std::max(max.y, y);
            max.z = std::max(max.z, z);
        }
        
        return BoundingBox(min, max);
    }

    void BoundingBoxSystem::updateBoundingBoxes(Scene* scene) {
        if (!scene) {
            return;
        }
        
        RAPTURE_PROFILE_SCOPE("Update Bounding Boxes");
        
        auto view = scene->getRegistry().view<TransformComponent, BoundingBoxComponent>();
        
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& boundingBox = view.get<BoundingBoxComponent>(entity);
            
            // Only update if needed
            if (boundingBox.needsUpdate) {
                boundingBox.updateWorldBoundingBox(transform.transformMatrix());
            }
        }
    }

    void BoundingBoxSystem::addBoundingBoxToEntity(Entity entity, const BoundingBox& localBounds) {
        // Skip if entity is already invalid
        if (!entity) {
            GE_CORE_WARN("Cannot add BoundingBoxComponent: Entity invalid");
            return;
        }
        
        // Skip if the entity already has a bounding box component
        if (entity.hasComponent<BoundingBoxComponent>()) {
            return;
        }
        
        if (localBounds.isValid()) {
            // Add the bounding box component to the entity
            entity.addComponent<BoundingBoxComponent>(localBounds);
            entity.getComponent<BoundingBoxComponent>().initSharedResources();
            GE_CORE_INFO("Added BoundingBoxComponent to entity '{}'", 
                entity.hasComponent<TagComponent>() ? entity.getComponent<TagComponent>().tag : "unnamed");
            
            // Log the bounds for debugging
            localBounds.logBounds();
        } else {
            GE_CORE_WARN("Could not add BoundingBoxComponent: Invalid bounding box provided");
        }
    }

    bool BoundingBoxSystem::checkIntersection(Entity entity1, Entity entity2) {
        if (!entity1 || !entity2) {
            return false;
        }
        
        if (!entity1.hasComponent<BoundingBoxComponent>() || !entity2.hasComponent<BoundingBoxComponent>()) {
            return false;
        }
        
        // Get the bounding box components
        auto& bbox1 = entity1.getComponent<BoundingBoxComponent>();
        auto& bbox2 = entity2.getComponent<BoundingBoxComponent>();
        
        // Make sure both bounding boxes are updated
        if (bbox1.needsUpdate && entity1.hasComponent<TransformComponent>()) {
            bbox1.updateWorldBoundingBox(entity1.getComponent<TransformComponent>().transformMatrix());
        }
        
        if (bbox2.needsUpdate && entity2.hasComponent<TransformComponent>()) {
            bbox2.updateWorldBoundingBox(entity2.getComponent<TransformComponent>().transformMatrix());
        }
        
        // Check for intersection
        return bbox1.worldBoundingBox.intersects(bbox2.worldBoundingBox);
    }

} // namespace Rapture 