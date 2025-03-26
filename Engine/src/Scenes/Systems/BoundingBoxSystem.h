#pragma once

#include "../Components/Components.h"
#include "../Scene.h"
#include "../../Logger/Log.h"

namespace Rapture {

    class BoundingBoxSystem {
    public:
        // Calculate bounding box from raw vertex data (for use during mesh loading)
        static BoundingBox calculateFromVertexData(const void* data, size_t dataSize, size_t stride, size_t positionOffset);
        
        // Update all bounding boxes in the scene
        static void updateBoundingBoxes(Scene* scene);
        
        // Add a bounding box component to an entity with a mesh component
        static void addBoundingBoxToEntity(Entity entity, const BoundingBox& localBounds);
        
        // Check if two entities' bounding boxes intersect
        static bool checkIntersection(Entity entity1, Entity entity2);
    };

} // namespace Rapture 