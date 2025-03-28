#include "PrimitiveShapes.h"
#include "../Buffers/VertexArray.h"
#include "../Buffers/Buffers.h"
#include "../Mesh/Mesh.h"
#include "../Materials/Material.h"
#include "../Materials/MaterialLibrary.h"
#include "Renderer.h"
#include "../Logger/Log.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>

namespace Rapture {

    // Helper function to create a mesh with position-only vertices
    static std::shared_ptr<Mesh> createPositionOnlyMesh(const std::vector<float>& positions, const std::vector<uint32_t>& indices) {
        auto mesh = std::make_shared<Mesh>();
        
        // Create a simple buffer layout with only position attribute
        BufferLayout layout;
        BufferAttribute posAttrib;
        posAttrib.name = "POSITION";
        posAttrib.componentType = GL_FLOAT;
        posAttrib.type = "VEC3";
        posAttrib.offset = 0;
        layout.buffer_attribs.push_back(posAttrib);
        layout.isInterleaved = true;
        layout.vertexSize = 3 * sizeof(float); // Only position (x,y,z)
        
        // Set the mesh data using our layout
        mesh->setMeshData(
            layout, 
            positions.data(), 
            positions.size() * sizeof(float),
            indices.data(),
            indices.size() * sizeof(uint32_t),
            indices.size(),
            GL_UNSIGNED_INT
        );
        
        return mesh;
    }

    //-----------------------------------------------------------------------------
    // Line Implementation
    //-----------------------------------------------------------------------------
    Line::Line(glm::vec3 start, glm::vec3 end, glm::vec4 color)
        : m_start(start), m_end(end), m_color(color) 
    {
        // Create the line mesh
        std::vector<float> positions = {
            start.x, start.y, start.z,
            end.x, end.y, end.z
        };

        std::vector<uint32_t> indices = { 0, 1 };
        
        auto mesh = createPositionOnlyMesh(positions, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a solid color material using MaterialLibrary
        std::string materialName = "Line_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }
    
    //-----------------------------------------------------------------------------
    // Cube Implementation
    //-----------------------------------------------------------------------------
    Cube::Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled)
        : m_position(position), m_rotation(rotation), m_scale(scale), m_color(color), m_filled(filled)
    {
        // Vertices for a cube centered at origin with size 1
        std::vector<float> positions = {
            // Front face
            -0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f,
            
            // Back face
            -0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f
        };
        
        std::vector<uint32_t> indices;
        
        if (filled) {
            // Indices for filled cube (triangles)
            indices = {
                // Front face
                0, 1, 2, 2, 3, 0,
                // Back face
                4, 5, 6, 6, 7, 4,
                // Left face
                0, 3, 7, 7, 4, 0,
                // Right face
                1, 5, 6, 6, 2, 1,
                // Bottom face
                0, 4, 5, 5, 1, 0,
                // Top face
                3, 2, 6, 6, 7, 3
            };
        } else {
            // Indices for wireframe cube (lines)
            indices = {
                // Front face
                0, 1, 1, 2, 2, 3, 3, 0,
                // Back face
                4, 5, 5, 6, 6, 7, 7, 4,
                // Connections between front and back
                0, 4, 1, 5, 2, 6, 3, 7
            };
        }
        
        auto mesh = createPositionOnlyMesh(positions, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a solid color material using MaterialLibrary
        std::string materialName = "Cube_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }
    
    //-----------------------------------------------------------------------------
    // Quad Implementation
    //-----------------------------------------------------------------------------
    Quad::Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color)
        : m_position(position), m_rotation(rotation), m_scale(scale), m_color(color)
    {
        // Vertices for a quad in the XY plane, centered at origin with size 1
        std::vector<float> positions = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        
        // Indices for filled quad (two triangles)
        std::vector<uint32_t> indices = {
            0, 1, 2,
            2, 3, 0
        };
        
        auto mesh = createPositionOnlyMesh(positions, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a solid color material using MaterialLibrary
        std::string materialName = "Quad_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }

} // namespace Rapture 