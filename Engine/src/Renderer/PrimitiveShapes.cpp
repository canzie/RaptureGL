#include "PrimitiveShapes.h"
#include "../Buffers/VertexArray.h"
#include "../Buffers/Buffers.h"
#include "../Mesh/Mesh.h"
#include "../Materials/Material.h"
#include "../Materials/MaterialLibrary.h"
#include "../AssetsManager/AssetManager.h"
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

    // Helper function to create a mesh with position, normal, and texcoord attributes
    static std::shared_ptr<Mesh> createFullAttributeMesh(
        const std::vector<float>& positions, 
        const std::vector<float>& normals, 
        const std::vector<float>& texcoords, 
        const std::vector<uint32_t>& indices) 
    {
        auto mesh = std::make_shared<Mesh>();
        
        // Create an interleaved buffer with position, normal, and texcoord attributes
        BufferLayout layout;
        
        // Position attribute
        BufferAttribute posAttrib;
        posAttrib.name = "POSITION";
        posAttrib.componentType = GL_FLOAT;
        posAttrib.type = "VEC3";
        posAttrib.offset = 0;
        layout.buffer_attribs.push_back(posAttrib);
        
        // Normal attribute
        BufferAttribute normAttrib;
        normAttrib.name = "NORMAL";
        normAttrib.componentType = GL_FLOAT;
        normAttrib.type = "VEC3";
        normAttrib.offset = 3 * sizeof(float);
        layout.buffer_attribs.push_back(normAttrib);
        
        // Texcoord attribute
        BufferAttribute texAttrib;
        texAttrib.name = "TEXCOORD_0";
        texAttrib.componentType = GL_FLOAT;
        texAttrib.type = "VEC2";
        texAttrib.offset = 6 * sizeof(float);
        layout.buffer_attribs.push_back(texAttrib);
        
        layout.isInterleaved = true;
        layout.vertexSize = (3 + 3 + 2) * sizeof(float); // position(3) + normal(3) + texcoord(2)
        
        // Interleave the data
        std::vector<float> interleavedData;
        interleavedData.reserve(positions.size() + normals.size() + texcoords.size());
        
        // Ensure vector sizes are consistent
        size_t vertexCount = positions.size() / 3;
        if (normals.size() / 3 != vertexCount || texcoords.size() / 2 != vertexCount) {
            GE_CORE_ERROR("createFullAttributeMesh: Attribute sizes are inconsistent");
            return nullptr;
        }
        
        // Interleave data: [pos.x, pos.y, pos.z, norm.x, norm.y, norm.z, tex.u, tex.v, ...]
        for (size_t i = 0; i < vertexCount; i++) {
            // Position (x, y, z)
            interleavedData.push_back(positions[i * 3 + 0]);
            interleavedData.push_back(positions[i * 3 + 1]);
            interleavedData.push_back(positions[i * 3 + 2]);
            
            // Normal (x, y, z)
            interleavedData.push_back(normals[i * 3 + 0]);
            interleavedData.push_back(normals[i * 3 + 1]);
            interleavedData.push_back(normals[i * 3 + 2]);
            
            // Texcoord (u, v)
            interleavedData.push_back(texcoords[i * 2 + 0]);
            interleavedData.push_back(texcoords[i * 2 + 1]);
        }
        
        // Set the mesh data using our layout
        mesh->setMeshData(
            layout, 
            interleavedData.data(), 
            interleavedData.size() * sizeof(float),
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

    Cube::Cube()
    : Cube(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f), false)
    {
    }

    Cube::Cube(bool isCubeMap)
    : Cube(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1000.0f), glm::vec4(1.0f), true, false)
    {
        if (isCubeMap) {
            m_material = MaterialLibrary::createCubeMapMaterial("CubeMap_Material");
        }
    }

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
    
    // Overloaded constructor that includes normals and texture coordinates
    Cube::Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, bool useTexCoords)
        : m_position(position), m_rotation(rotation), m_scale(scale), m_color(color), m_filled(filled)
    {
        if (!useTexCoords) {
            // Fall back to the simpler version if we don't need texture coordinates
            *this = Cube(position, rotation, scale, color, filled);
            return;
        }

        if (!filled) {
            // For wireframe, we don't need normals and texcoords
            *this = Cube(position, rotation, scale, color, false);
            return;
        }

        // Vertices for a cube centered at origin with size 1
        // We need unique vertices for each face for proper normals and texture coordinates
        std::vector<float> positions = {
            // Front face
            -0.5f, -0.5f,  0.5f,  // 0
             0.5f, -0.5f,  0.5f,  // 1
             0.5f,  0.5f,  0.5f,  // 2
            -0.5f,  0.5f,  0.5f,  // 3
            
            // Back face
            -0.5f, -0.5f, -0.5f,  // 4
             0.5f, -0.5f, -0.5f,  // 5
             0.5f,  0.5f, -0.5f,  // 6
            -0.5f,  0.5f, -0.5f,  // 7
            
            // Left face
            -0.5f, -0.5f, -0.5f,  // 8
            -0.5f, -0.5f,  0.5f,  // 9
            -0.5f,  0.5f,  0.5f,  // 10
            -0.5f,  0.5f, -0.5f,  // 11
            
            // Right face
             0.5f, -0.5f,  0.5f,  // 12
             0.5f, -0.5f, -0.5f,  // 13
             0.5f,  0.5f, -0.5f,  // 14
             0.5f,  0.5f,  0.5f,  // 15
            
            // Bottom face
            -0.5f, -0.5f, -0.5f,  // 16
             0.5f, -0.5f, -0.5f,  // 17
             0.5f, -0.5f,  0.5f,  // 18
            -0.5f, -0.5f,  0.5f,  // 19
            
            // Top face
            -0.5f,  0.5f,  0.5f,  // 20
             0.5f,  0.5f,  0.5f,  // 21
             0.5f,  0.5f, -0.5f,  // 22
            -0.5f,  0.5f, -0.5f   // 23
        };
        
        std::vector<float> normals = {
            // Front face
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            
            // Back face
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            
            // Left face
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            
            // Right face
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            
            // Bottom face
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            
            // Top face
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
        };
        
        std::vector<float> texcoords = {
            // Front face
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
            
            // Back face
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
            
            // Left face
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
            
            // Right face
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
            
            // Bottom face
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
            
            // Top face
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
        };
        
        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 5, 6, 6, 7, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Bottom face
            16, 17, 18, 18, 19, 16,
            // Top face
            20, 21, 22, 22, 23, 20
        };
        
        auto mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a solid color material using MaterialLibrary
        std::string materialName = "Cube_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }
    
    // Add constructor implementation for textured Cube
    Cube::Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, const std::string& texturePath)
        : m_position(position), m_rotation(rotation), m_scale(scale), m_color(color), m_filled(filled)
    {
        if (!filled) {
            // For wireframe, we don't need textures
            *this = Cube(position, rotation, scale, color, false);
            return;
        }

        // Vertices for a cube centered at origin with size 1
        // We need unique vertices for each face for proper normals and texture coordinates
        std::vector<float> positions = {
            // Front face
            -0.5f, -0.5f,  0.5f,  // 0
             0.5f, -0.5f,  0.5f,  // 1
             0.5f,  0.5f,  0.5f,  // 2
            -0.5f,  0.5f,  0.5f,  // 3
            
            // Back face
            -0.5f, -0.5f, -0.5f,  // 4
             0.5f, -0.5f, -0.5f,  // 5
             0.5f,  0.5f, -0.5f,  // 6
            -0.5f,  0.5f, -0.5f,  // 7
            
            // Left face
            -0.5f, -0.5f, -0.5f,  // 8
            -0.5f, -0.5f,  0.5f,  // 9
            -0.5f,  0.5f,  0.5f,  // 10
            -0.5f,  0.5f, -0.5f,  // 11
            
            // Right face
             0.5f, -0.5f,  0.5f,  // 12
             0.5f, -0.5f, -0.5f,  // 13
             0.5f,  0.5f, -0.5f,  // 14
             0.5f,  0.5f,  0.5f,  // 15
            
            // Bottom face
            -0.5f, -0.5f, -0.5f,  // 16
             0.5f, -0.5f, -0.5f,  // 17
             0.5f, -0.5f,  0.5f,  // 18
            -0.5f, -0.5f,  0.5f,  // 19
            
            // Top face
            -0.5f,  0.5f,  0.5f,  // 20
             0.5f,  0.5f,  0.5f,  // 21
             0.5f,  0.5f, -0.5f,  // 22
            -0.5f,  0.5f, -0.5f   // 23
        };
        
        std::vector<float> normals = {
            // Front face
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            
            // Back face
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            
            // Left face
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            
            // Right face
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            
            // Bottom face
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            
            // Top face
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
        };
        
        std::vector<float> texcoords = {
            // Front face
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
            
            // Back face
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
            
            // Left face
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
            
            // Right face
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
            
            // Bottom face
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
            
            // Top face
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
        };
        
        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 5, 6, 6, 7, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Bottom face
            16, 17, 18, 18, 19, 16,
            // Top face
            20, 21, 22, 22, 23, 20
        };
        
        auto mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a textured material
        std::string materialName = "Cube_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }
    
    //-----------------------------------------------------------------------------
    // Quad Implementation
    //-----------------------------------------------------------------------------
    Quad::Quad()
        : Quad(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f), true)
    {
        
    }

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

    // Overloaded constructor that includes normals and texture coordinates
    Quad::Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool useTexCoords)
        : m_position(position), m_rotation(rotation), m_scale(scale), m_color(color)
    {
        if (!useTexCoords) {
            // Fall back to the simpler version if we don't need texture coordinates
            *this = Quad(position, rotation, scale, color);
            return;
        }

        // Vertices for a quad in the XY plane, centered at origin with size 1
        std::vector<float> positions = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        
        // Normals for quad (all facing positive Z)
        std::vector<float> normals = {
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f
        };
        
        // Texture coordinates
        std::vector<float> texcoords = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f
        };
        
        // Indices for filled quad (two triangles)
        std::vector<uint32_t> indices = {
            0, 1, 2,
            2, 3, 0
        };
        
        auto mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a solid color material using MaterialLibrary
        std::string materialName = "Quad_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }

    // Add constructor implementation for textured Quad
    Quad::Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, const std::string& texturePath)
        : m_position(position), m_rotation(rotation), m_scale(scale), m_color(color)
    {
        // Vertices for a quad in the XY plane, centered at origin with size 1
        std::vector<float> positions = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        
        // Normals for quad (all facing positive Z)
        std::vector<float> normals = {
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f
        };
        
        // Texture coordinates
        std::vector<float> texcoords = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f
        };
        
        // Indices for filled quad (two triangles)
        std::vector<uint32_t> indices = {
            0, 1, 2,
            2, 3, 0
        };
        
        auto mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        
        // Store the mesh as a member variable
        m_mesh = mesh;
        
        // Create a textured material
        std::string materialName = "Quad_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(color));
    }

    Sphere::Sphere(float radius)
    {
        // Generate sphere mesh
        const int segments = 32;
        const int rings = 16;
        
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<uint32_t> indices;
        
        // Generate vertices
        for (int ring = 0; ring <= rings; ++ring) {
            float phi = ring * glm::pi<float>() / rings;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            
            for (int segment = 0; segment <= segments; ++segment) {
                float theta = segment * 2.0f * glm::pi<float>() / segments;
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);
                
                // Position
                float x = sinPhi * cosTheta * radius;
                float y = cosPhi * radius;
                float z = sinPhi * sinTheta * radius;
                positions.push_back(x);
                positions.push_back(y);
                positions.push_back(z);
                
                // Normal (normalize the position)
                glm::vec3 normal = glm::normalize(glm::vec3(x / radius, y / radius, z / radius));
                normals.push_back(normal.x);
                normals.push_back(normal.y);
                normals.push_back(normal.z);
                
                // Texture coordinates
                float u = static_cast<float>(segment) / segments;
                float v = static_cast<float>(ring) / rings;
                texcoords.push_back(u);
                texcoords.push_back(v);
            }
        }
        
        // Generate indices
        for (int ring = 0; ring < rings; ++ring) {
            for (int segment = 0; segment < segments; ++segment) {
                // Calculate indices for the current quad
                uint32_t current = ring * (segments + 1) + segment;
                uint32_t next = current + 1;
                uint32_t currentBelow = (ring + 1) * (segments + 1) + segment;
                uint32_t nextBelow = currentBelow + 1;
                
                // Create two triangles for the quad
                indices.push_back(current);
                indices.push_back(nextBelow);
                indices.push_back(next);
                
                indices.push_back(current);
                indices.push_back(currentBelow);
                indices.push_back(nextBelow);
            }
        }
        
        // Create the mesh with all attributes
        m_mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        
        // Get default material from AssetManager
        auto [material, handle] = AssetManager::getDefaultAsset<Material>(AssetType::Material);
        if (material) {
            m_material = material;
            m_sphereAsset = handle;
        } else {
            GE_CORE_ERROR("Sphere: Failed to get default material");
        }
    }

    void Sphere::setMaterial(const AssetHandle &handle)
    {
        auto mat = AssetManager::getAsset<Material>(handle);
        if (mat) {
            m_material = mat;
            m_sphereAsset = handle;
        }
    }

} // namespace Rapture