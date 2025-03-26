#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <limits>

namespace Rapture {

    class BoundingBox {
    private:
        glm::vec3 _min;
        glm::vec3 _max;
        bool _isValid;

    public:
        // Default constructor creates an invalid bounding box
        BoundingBox() : 
            _min(std::numeric_limits<float>::max()),
            _max(std::numeric_limits<float>::lowest()),
            _isValid(false) {}

        // Constructor with min and max points
        BoundingBox(const glm::vec3& min, const glm::vec3& max) : 
            _min(min), 
            _max(max),
            _isValid(true) {}
            
        // Calculate bounding box from vertices
        static BoundingBox calculateFromVertices(const std::vector<float>& vertices, size_t stride, size_t offset);
        
        // Transform the bounding box by a matrix
        BoundingBox transform(const glm::mat4& matrix) const;
        
        // Merge with another bounding box
        BoundingBox merge(const BoundingBox& other) const;
        
        // Check if a point is inside the bounding box
        bool contains(const glm::vec3& point) const;
        
        // Check if this bounding box intersects with another
        bool intersects(const BoundingBox& other) const;
        
        // Check if the bounding box is valid
        bool isValid() const { return _isValid; }
        
        // Reset to an invalid state
        void reset();
        
        // Getters
        glm::vec3 getMin() const { return _min; }
        glm::vec3 getMax() const { return _max; }
        glm::vec3 getCenter() const { return (_min + _max) * 0.5f; }
        glm::vec3 getExtents() const { return _max - _min; }
        glm::vec3 getSize() const { return _max - _min; }
        
        // Debug helper
        void logBounds() const;
    };

    struct BoundingBoxComponent
    {
        // Original bounding box in local space (before any transformations)
        BoundingBox localBoundingBox;
        
        // Transformed bounding box in world space (updated when transform changes)
        BoundingBox worldBoundingBox;
        
        // Flag to indicate if the world bounding box needs to be updated
        bool needsUpdate = true;
        
        // Flag to indicate if this bounding box should be rendered
        bool isVisible = false;
        
        // Constructor
        BoundingBoxComponent() = default;
        
        // Constructor with local bounding box
        BoundingBoxComponent(const BoundingBox& localBox) : 
            localBoundingBox(localBox), worldBoundingBox(localBox), needsUpdate(true), isVisible(false) {}
            
        // Update the world bounding box using the transform matrix
        void updateWorldBoundingBox(const glm::mat4& transformMatrix) {
            if (needsUpdate && localBoundingBox.isValid()) {
                worldBoundingBox = localBoundingBox.transform(transformMatrix);
                needsUpdate = false;
            }
        }
        
        // Mark the bounding box as needing update (call when transform changes)
        void markForUpdate() {
            needsUpdate = true;
        }
    };

} // namespace Rapture 