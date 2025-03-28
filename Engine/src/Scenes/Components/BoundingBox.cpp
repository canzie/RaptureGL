#include "BoundingBox.h"
#include "../../Debug/TracyProfiler.h"
#include "../../Logger/Log.h"
#include "../../Renderer/PrimitiveShapes.h"
#include "../../Materials/MaterialLibrary.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Rapture {

    // Initialize static members
    std::shared_ptr<Mesh> BoundingBoxComponent::s_visualizationMesh = nullptr;
    std::shared_ptr<Material> BoundingBoxComponent::s_visualizationMaterial = nullptr;
    
    void BoundingBoxComponent::initSharedResources() {
        // Create a cube mesh if it doesn't exist
        if (!s_visualizationMesh) {
            // Create a static cube for visualization
            // This will be transformed to match the bounding box dimensions and position
            Cube visualizationCube(
                glm::vec3(0.0f),       // position at origin (transformed during rendering)
                glm::vec3(0.0f),       // no rotation
                glm::vec3(1.0f),       // unit size (scaled during rendering)
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // default green color
                false                   // wireframe mode
            );
            
            // Store the mesh for reuse
            s_visualizationMesh = visualizationCube.getMesh();
            
            // Store the material for reuse
            s_visualizationMaterial = visualizationCube.getMaterial();
            
            GE_CORE_INFO("BoundingBoxComponent: Initialized shared visualization resources");
        }
    }
    
    void BoundingBoxComponent::shutdownSharedResources() {
        // Reset shared resources
        s_visualizationMesh.reset();
        s_visualizationMaterial.reset();
        
        GE_CORE_INFO("BoundingBoxComponent: Shutdown shared visualization resources");
    }

    void BoundingBox::reset() {
        _min = glm::vec3(std::numeric_limits<float>::max());
        _max = glm::vec3(std::numeric_limits<float>::lowest());
        _isValid = false;
    }

    BoundingBox BoundingBox::calculateFromVertices(const std::vector<float>& vertices, size_t stride, size_t offset) {
        RAPTURE_PROFILE_FUNCTION();
        
        if (vertices.empty()|| stride < 3) {
            return BoundingBox(); // Return invalid bounding box
        }
        
        glm::vec3 min(std::numeric_limits<float>::max());
        glm::vec3 max(std::numeric_limits<float>::lowest());
        
        // Process each vertex
        for (size_t i = offset; i < vertices.size(); i += stride) {
            // Ensure we have enough data for a position
            if (i + 2 >= vertices.size()) break;
            
            // Get the position from the vertex data
            glm::vec3 pos(vertices[i], vertices[i + 1], vertices[i + 2]);
            
            // Update min and max
            min = glm::min(min, pos);
            max = glm::max(max, pos);
        }
        
        return BoundingBox(min, max);
    }

    BoundingBox BoundingBox::transform(const glm::mat4& matrix) const {
        RAPTURE_PROFILE_FUNCTION();
        
        if (!_isValid) {
            return BoundingBox(); // Return invalid bounding box
        }
        
        // Transform all 8 corners of the bounding box
        glm::vec3 corners[8];
        corners[0] = glm::vec3(_min.x, _min.y, _min.z);
        corners[1] = glm::vec3(_min.x, _min.y, _max.z);
        corners[2] = glm::vec3(_min.x, _max.y, _min.z);
        corners[3] = glm::vec3(_min.x, _max.y, _max.z);
        corners[4] = glm::vec3(_max.x, _min.y, _min.z);
        corners[5] = glm::vec3(_max.x, _min.y, _max.z);
        corners[6] = glm::vec3(_max.x, _max.y, _min.z);
        corners[7] = glm::vec3(_max.x, _max.y, _max.z);
        
        // Initialize new bounds
        glm::vec3 newMin(std::numeric_limits<float>::max());
        glm::vec3 newMax(std::numeric_limits<float>::lowest());
        
        // Transform each corner and update bounds
        for (int i = 0; i < 8; ++i) {
            glm::vec4 transformedCorner = matrix * glm::vec4(corners[i], 1.0f);
            transformedCorner /= transformedCorner.w; // Perspective division
            
            glm::vec3 transformedVec3(transformedCorner);
            
            newMin = glm::min(newMin, transformedVec3);
            newMax = glm::max(newMax, transformedVec3);
        }
        
        return BoundingBox(newMin, newMax);
    }

    BoundingBox BoundingBox::merge(const BoundingBox& other) const {
        // If either box is invalid, return the other one
        if (!_isValid && !other.isValid()) {
            return BoundingBox(); // Both invalid, return invalid
        }
        
        if (!_isValid) {
            return other;
        }
        
        if (!other.isValid()) {
            return *this;
        }
        
        // Both valid, calculate merged bounds
        glm::vec3 newMin = glm::min(_min, other.getMin());
        glm::vec3 newMax = glm::max(_max, other.getMax());
        
        return BoundingBox(newMin, newMax);
    }

    bool BoundingBox::contains(const glm::vec3& point) const {
        if (!_isValid) {
            return false;
        }
        
        return point.x >= _min.x && point.x <= _max.x &&
               point.y >= _min.y && point.y <= _max.y &&
               point.z >= _min.z && point.z <= _max.z;
    }

    bool BoundingBox::intersects(const BoundingBox& other) const {
        if (!_isValid || !other.isValid()) {
            return false;
        }
        
        // Check for overlap in all three dimensions
        return (_min.x <= other.getMax().x && _max.x >= other.getMin().x) &&
               (_min.y <= other.getMax().y && _max.y >= other.getMin().y) &&
               (_min.z <= other.getMax().z && _max.z >= other.getMin().z);
    }

    void BoundingBox::logBounds() const {
        if (_isValid) {
            GE_CORE_INFO("BoundingBox: Min({:.2f}, {:.2f}, {:.2f}), Max({:.2f}, {:.2f}, {:.2f})", 
                _min.x, _min.y, _min.z, _max.x, _max.y, _max.z);
        } else {
            GE_CORE_WARN("BoundingBox: Invalid");
        }
    }

} // namespace Rapture 