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
#include <string> // Required for std::to_string

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
        // Create the line mesh using the helper
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
        if (m_material) {
            m_material->setVec3("color", glm::vec3(m_color));
        }
    }
    
    void Line::setPoints(glm::vec3 start, glm::vec3 end)
    {
        m_start = start;
        m_end = end;

        // Recreate the mesh with the new points using the existing helper
        std::vector<float> positions = {
            start.x, start.y, start.z,
            end.x, end.y, end.z
        };
        std::vector<uint32_t> indices = { 0, 1 };

        // Replace the existing mesh object with a new one
        m_mesh = createPositionOnlyMesh(positions, indices);
    }

    void Line::setColor(const glm::vec4& color)
    {
        m_color = color;
        if (m_material) {
            m_material->setVec3("color", glm::vec3(m_color));
        }
    }
    
    //-----------------------------------------------------------------------------
    // Cube Implementation
    //-----------------------------------------------------------------------------

    // Private helper to initialize from config
    void Cube::initFromConfig(const PrimitiveConfig& config) {
        m_position = config.position;
        m_rotation = config.rotation;
        m_scale = config.scale;
        m_color = config.color;
        m_filled = config.isFilled;

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<uint32_t> indices;

        if (!m_filled) {
            // Wireframe Cube (Position Only)
             positions = {
                // Unique vertices for wireframe lines
                -0.5f, -0.5f,  0.5f, // 0 Front Bottom Left
                 0.5f, -0.5f,  0.5f, // 1 Front Bottom Right
                 0.5f,  0.5f,  0.5f, // 2 Front Top Right
                -0.5f,  0.5f,  0.5f, // 3 Front Top Left
                -0.5f, -0.5f, -0.5f, // 4 Back Bottom Left
                 0.5f, -0.5f, -0.5f, // 5 Back Bottom Right
                 0.5f,  0.5f, -0.5f, // 6 Back Top Right
                -0.5f,  0.5f, -0.5f  // 7 Back Top Left
            };
            indices = {
                // Front face
                0, 1, 1, 2, 2, 3, 3, 0,
                // Back face
                4, 5, 5, 6, 6, 7, 7, 4,
                // Connections
                0, 4, 1, 5, 2, 6, 3, 7
            };
            m_mesh = createPositionOnlyMesh(positions, indices);
        } else if (config.useTexCoords || config.texturePath.has_value()) {
            // Filled Cube (Full Attributes - Position, Normal, TexCoord)
             positions = {
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

             normals = {
                // Front face (0-3)
                0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                // Back face (4-7)
                0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f,
                // Left face (8-11)
                -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
                // Right face (12-15)
                1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                // Bottom face (16-19)
                0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
                // Top face (20-23)
                0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f
            };

             texcoords = {
                // Front face (0-3) - Standard UV Mapping
                0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                // Back face (4-7) - Reversed U for back
                1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                // Left face (8-11)
                0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                // Right face (12-15) - Reversed U for consistency? Or keep as is? Let's keep it simple for now.
                1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                // Bottom face (16-19)
                0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
                // Top face (20-23)
                0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
             };

             indices = {
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
             m_mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        } else {
            // Filled Cube (Position Only)
             positions = {
                 // Need 8 unique vertices for face definitions
                // Front face
                -0.5f, -0.5f,  0.5f, // 0
                 0.5f, -0.5f,  0.5f, // 1
                 0.5f,  0.5f,  0.5f, // 2
                -0.5f,  0.5f,  0.5f, // 3

                // Back face
                -0.5f, -0.5f, -0.5f, // 4
                 0.5f, -0.5f, -0.5f, // 5
                 0.5f,  0.5f, -0.5f, // 6
                -0.5f,  0.5f, -0.5f  // 7
            };
             indices = {
                // Front face
                0, 1, 2, 2, 3, 0,
                // Back face
                4, 5, 6, 6, 7, 4,
                // Left face (reuse vertices)
                4, 0, 3, 3, 7, 4, // Using 4,0,3 and 3,7,4
                // Right face (reuse vertices)
                1, 5, 6, 6, 2, 1, // Using 1,5,6 and 6,2,1
                // Bottom face (reuse vertices)
                4, 5, 1, 1, 0, 4, // Using 4,5,1 and 1,0,4
                // Top face (reuse vertices)
                3, 2, 6, 6, 7, 3  // Using 3,2,6 and 6,7,3
            };
             m_mesh = createPositionOnlyMesh(positions, indices);
        }


        // Material Creation
        if (config.createDefaultMaterial) {
            auto [defaultMat, matHandle] = AssetManager::getDefaultAsset<Material>(AssetType::Material);
            if (!defaultMat) {
                m_material = nullptr;
                return;
            }

            m_material = defaultMat;

            if (config.texturePath.has_value() && !config.texturePath.value().empty()) {
                // Attempt to create textured material
                auto [texture, textureHandle] = AssetManager::importAsset<Texture2D>(std::filesystem::path(config.texturePath.value()));
                m_material->setTexture(ParameterID::TEXTURE_ALBEDO, texture, textureHandle);
                

            } 
        } else {
            m_material = nullptr; // No default material requested
        }
    }

    // Primary Constructor
    Cube::Cube(const PrimitiveConfig& config) {
        initFromConfig(config);
    }

    // Delegating Constructors
    Cube::Cube()
        : Cube(PrimitiveConfig{}) // Use default config
    {}

    Cube::Cube(bool isCubeMap)
    {
        // Special case for CubeMap - uses specific settings
        PrimitiveConfig config;
        config.position = glm::vec3(0.0f);
        config.rotation = glm::vec3(0.0f);
        config.scale = glm::vec3(1000.0f); // Large scale typical for skyboxes
        config.color = glm::vec4(1.0f); // Color likely ignored by cubemap shader
        config.isFilled = true;
        config.useTexCoords = false; // Cubemap shader usually uses vertex positions
        config.createDefaultMaterial = false; // We create a specific material below

        initFromConfig(config); // Initialize mesh and properties

        // Override material with specific CubeMap material
        if (isCubeMap) {
            m_material = MaterialLibrary::createCubeMapMaterial("CubeMap_Material");
        } else {
            // If isCubeMap is false, perhaps default back to solid color?
             std::string materialName = "Cube_Material_" + std::to_string(reinterpret_cast<uintptr_t>(this));
             m_material = MaterialLibrary::createSolidMaterial(materialName, glm::vec3(m_color));
        }
    }

    Cube::Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled)
    {
        PrimitiveConfig config;
        config.position = position;
        config.rotation = rotation;
        config.scale = scale;
        config.color = color;
        config.isFilled = filled;
        config.useTexCoords = false; // Default constructor didn't use tex coords
        config.texturePath = std::nullopt;
        config.createDefaultMaterial = true;
        initFromConfig(config);
    }

    Cube::Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, bool useTexCoords)
    {
         PrimitiveConfig config;
        config.position = position;
        config.rotation = rotation;
        config.scale = scale;
        config.color = color;
        config.isFilled = filled;
        config.useTexCoords = useTexCoords;
        config.texturePath = std::nullopt;
         config.createDefaultMaterial = true;
        initFromConfig(config);
    }

    Cube::Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, const std::string& texturePath)
    {
         PrimitiveConfig config;
        config.position = position;
        config.rotation = rotation;
        config.scale = scale;
        config.color = color;
        config.isFilled = filled;
        config.useTexCoords = true; // Texture path implies tex coords needed
        config.texturePath = texturePath;
         config.createDefaultMaterial = true;
        initFromConfig(config);
    }

    //-----------------------------------------------------------------------------
    // Quad Implementation
    //-----------------------------------------------------------------------------

     // Private helper to initialize from config
    void Quad::initFromConfig(const PrimitiveConfig& config) {
        m_position = config.position;
        m_rotation = config.rotation;
        m_scale = config.scale;
        m_color = config.color;

         // Quad is always 'filled' (made of triangles)
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

        if (config.useTexCoords || config.texturePath.has_value()) {
             // Full Attributes (Position, Normal, TexCoord)
             // Normals for quad (all facing positive Z)
            std::vector<float> normals = {
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f
            };

            // Texture coordinates
            std::vector<float> texcoords = {
                0.0f, 1.0f, // Bottom-left
                1.0f, 1.0f, // Bottom-right
                1.0f, 0.0f, // Top-right
                0.0f, 0.0f  // Top-left
            };
             m_mesh = createFullAttributeMesh(positions, normals, texcoords, indices);
        } else {
            // Position Only
            m_mesh = createPositionOnlyMesh(positions, indices);
        }



        // Material Creation
        if (config.createDefaultMaterial) {
            auto [defaultMat, matHandle] = AssetManager::getDefaultAsset<Material>(AssetType::Material);
            if (!defaultMat) {
                m_material = nullptr;
                return;
            }

            m_material = defaultMat;

            if (config.texturePath.has_value() && !config.texturePath.value().empty()) {
                // Attempt to create textured material
                auto [texture, textureHandle] = AssetManager::importAsset<Texture2D>(std::filesystem::path(config.texturePath.value()));
                m_material->setTexture(ParameterID::TEXTURE_ALBEDO, texture, textureHandle);
                

            } 
        } else {
            m_material = nullptr; // No default material requested
        }
    }


    // Primary Constructor
     Quad::Quad(const PrimitiveConfig& config) {
         initFromConfig(config);
     }

    // Delegating Constructors
    Quad::Quad()
        : Quad(PrimitiveConfig{}) // Use default config
    {}

    Quad::Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color)
    {
        PrimitiveConfig config;
        config.position = position;
        config.rotation = rotation;
        config.scale = scale;
        config.color = color;
        config.isFilled = true; // Quads are always filled
        config.useTexCoords = false;
        config.texturePath = std::nullopt;
        config.createDefaultMaterial = true;
        initFromConfig(config);
    }

    Quad::Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool useTexCoords)
    {
        PrimitiveConfig config;
        config.position = position;
        config.rotation = rotation;
        config.scale = scale;
        config.color = color;
        config.isFilled = true;
        config.useTexCoords = useTexCoords;
        config.texturePath = std::nullopt;
        config.createDefaultMaterial = true;
        initFromConfig(config);
    }

    Quad::Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, const std::string& texturePath)
    {
         PrimitiveConfig config;
        config.position = position;
        config.rotation = rotation;
        config.scale = scale;
        config.color = color;
        config.isFilled = true;
        config.useTexCoords = true; // Texture path implies tex coords needed
        config.texturePath = texturePath;
        config.createDefaultMaterial = true;
        initFromConfig(config);
    }

    //-----------------------------------------------------------------------------
    // Sphere Implementation
    //-----------------------------------------------------------------------------

    // Private helper to initialize from config
    void Sphere::initFromConfig(const PrimitiveConfig& config) {
        m_position = config.position;
        m_rotation = config.rotation;
        m_scale = config.scale;
        m_color = config.color;
        m_radius = config.radius;

        // Generate sphere mesh data (always full attributes needed for smooth shading/texturing)
        const int segments = 32;
        const int rings = 16;
        float radius = m_radius; // Use the radius from config

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<uint32_t> indices;

        for (int ring = 0; ring <= rings; ++ring) {
            float phi = ring * glm::pi<float>() / rings; // Latitude
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (int segment = 0; segment <= segments; ++segment) {
                float theta = segment * 2.0f * glm::pi<float>() / segments; // Longitude
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                // Position (spherical to Cartesian)
                float x = sinPhi * cosTheta * radius;
                float y = cosPhi * radius;
                float z = sinPhi * sinTheta * radius;
                positions.push_back(x);
                positions.push_back(y);
                positions.push_back(z);

                // Normal (normalized position vector for a sphere centered at origin)
                 glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
                 if (glm::length(normal) < 0.001f) { // Handle pole singularity
                    normal = glm::vec3(0.0f, (y > 0.0f) ? 1.0f : -1.0f, 0.0f);
                 }
                 normals.push_back(normal.x);
                 normals.push_back(normal.y);
                 normals.push_back(normal.z);

                // Texture coordinates (spherical mapping)
                float u = static_cast<float>(segment) / segments;
                float v = static_cast<float>(ring) / rings;
                texcoords.push_back(u);
                texcoords.push_back(v);
            }
        }

        // Generate indices for triangles
        for (int ring = 0; ring < rings; ++ring) {
            for (int segment = 0; segment < segments; ++segment) {
                uint32_t current = ring * (segments + 1) + segment;
                uint32_t next = current + 1;
                uint32_t currentBelow = (ring + 1) * (segments + 1) + segment;
                uint32_t nextBelow = currentBelow + 1;

                // Triangle 1
                indices.push_back(current);
                indices.push_back(currentBelow);
                indices.push_back(next);


                // Triangle 2
                indices.push_back(next);
                indices.push_back(currentBelow);
                indices.push_back(nextBelow);

            }
        }

         m_mesh = createFullAttributeMesh(positions, normals, texcoords, indices);



        // Material Creation
        if (config.createDefaultMaterial) {
            auto [defaultMat, matHandle] = AssetManager::getDefaultAsset<Material>(AssetType::Material);
            if (!defaultMat) {
                return;
            }

            m_material = defaultMat;
            m_sphereAsset = matHandle;

            if (config.texturePath.has_value() && !config.texturePath.value().empty()) {
                // Attempt to create textured material
                auto [texture, textureHandle] = AssetManager::importAsset<Texture2D>(std::filesystem::path(config.texturePath.value()));
                m_material.lock()->setTexture(ParameterID::TEXTURE_ALBEDO, texture, textureHandle);
                

            } 
        }
    }


    // Primary Constructor
    Sphere::Sphere(const PrimitiveConfig& config) {
         initFromConfig(config);
    }


    // Delegating Constructor
    Sphere::Sphere(float radius)
    {
        PrimitiveConfig config;
        config.radius = radius;
        // Use defaults for other properties (pos, rot, scale, color, etc.)
        config.createDefaultMaterial = true; // Legacy constructor implies default material behavior
        config.useTexCoords = true; // Sphere mesh generation includes tex coords
        initFromConfig(config);
    }


    void Sphere::setMaterial(const AssetHandle &handle)
    {
        auto mat = AssetManager::getAsset<Material>(handle);
        if (mat) {
            m_material = mat; // Update the shared_ptr
            m_sphereAsset = handle; // Store the handle too
        } else {
             GE_CORE_ERROR("Sphere::setMaterial: Failed to get material with handle {}", handle);
        }
    }

} // namespace Rapture