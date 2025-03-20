#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../Buffers.h"
#include "OpenGLBuffers.h"
#include "../VertexArray.h"

namespace Rapture {

    // Example showing how to use instanced rendering with the new buffer system
    class InstancedRenderingExample {
    public:
        static void SetupInstancedRendering(int instanceCount = 100) {
            // 1. Create instance data (transformation matrices for each instance)
            std::vector<glm::mat4> instanceMatrices(instanceCount);
            
            // Position instances in a grid pattern
            for (int i = 0; i < instanceCount; i++) {
                // Calculate position in a grid
                float x = (i % 10) * 2.0f - 9.0f;  // -9 to 9 in steps of 2
                float z = (i / 10) * 2.0f - 9.0f;  // -9 to 9 in steps of 2
                
                // Create a transformation matrix for this instance
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(x, 0.0f, z));
                
                // Add some variation to rotation and scale
                float rotationAngle = static_cast<float>(i) * 10.0f;
                model = glm::rotate(model, glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
                
                float scale = 0.5f + (static_cast<float>(i % 5) * 0.1f);
                model = glm::scale(model, glm::vec3(scale));
                
                instanceMatrices[i] = model;
            }
            
            // 2. Create a vertex buffer for instance data
            auto instanceBuffer = std::make_shared<VertexBuffer>(
                reinterpret_cast<const unsigned char*>(instanceMatrices.data()),
                instanceMatrices.size() * sizeof(glm::mat4),
                BufferUsage::Static
            );
            instanceBuffer->setDebugLabel("InstanceTransforms");
            
            // 3. Create mesh data (a simple cube)
            // Cube vertex positions
            std::vector<float> vertices = {
                // Front face
                -0.5f, -0.5f,  0.5f,
                 0.5f, -0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                
                // Back face
                -0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f,
                -0.5f,  0.5f, -0.5f,
            };
            
            // Cube indices
            std::vector<unsigned int> indices = {
                // Front face
                0, 1, 2, 2, 3, 0,
                
                // Right face
                1, 5, 6, 6, 2, 1,
                
                // Back face
                5, 4, 7, 7, 6, 5,
                
                // Left face
                4, 0, 3, 3, 7, 4,
                
                // Top face
                3, 2, 6, 6, 7, 3,
                
                // Bottom face
                4, 5, 1, 1, 0, 4
            };
            
            // 4. Create vertex and index buffers for the mesh
            auto vertexBuffer = std::make_shared<VertexBuffer>(
                reinterpret_cast<const unsigned char*>(vertices.data()),
                vertices.size() * sizeof(float),
                BufferUsage::Static
            );
            vertexBuffer->setDebugLabel("CubeVertices");
            
            auto indexBuffer = std::make_shared<IndexBuffer>(
                reinterpret_cast<const unsigned char*>(indices.data()),
                indices.size() * sizeof(unsigned int),
                GL_UNSIGNED_INT,
                BufferUsage::Static
            );
            indexBuffer->setDebugLabel("CubeIndices");
            
            // 5. Create and set up vertex array
            auto vao = std::make_shared<VertexArray>();
            vao->setDebugLabel("InstancedCubeVAO");
            
            // Set up the vertex buffer
            vao->setVertexBuffer(vertexBuffer);
            
            // Define attributes for the vertex buffer
            BufferLayout vertexLayout;
            BufferAttribute positionAttr;
            positionAttr.name = "POSITION";
            positionAttr.type = "VEC3";
            positionAttr.componentType = GL_FLOAT;
            positionAttr.offset = 0;
            vertexLayout.buffer_attribs.push_back(positionAttr);
            
            vao->setBufferLayout(vertexLayout);
            
            // Set the index buffer
            vao->setIndexBuffer(indexBuffer);
            
            // Now set up the instance buffer with custom attribute layout
            // Note: This directly uses OpenGL calls as our current VertexArray doesn't
            // have built-in support for multiple buffer bindings
            
            // Bind the VAO and instance buffer
            vao->bind();
            instanceBuffer->bind();
            
            // Set up the instance transformation matrix attribute
            BufferAttribute transformAttr;
            transformAttr.name = "TRANSFORM_MAT";
            transformAttr.type = "MAT4";
            transformAttr.componentType = GL_FLOAT;
            transformAttr.offset = 0;
            
            vao->setAttribLayout(transformAttr);
            
            // 6. Rendering (pseudo-code)
            // vao->bind();
            // glDrawElementsInstanced(GL_TRIANGLES, indexBuffer->getCount(), indexBuffer->getComponentType(), nullptr, instanceCount);
            // vao->unbind();
        }
        
        // Example of dynamically updating instance data
        static void UpdateInstanceData(std::shared_ptr<VertexBuffer> instanceBuffer, int instanceCount, float time) {
            // Create updated instance matrices based on time
            std::vector<glm::mat4> instanceMatrices(instanceCount);
            
            for (int i = 0; i < instanceCount; i++) {
                float x = (i % 10) * 2.0f - 9.0f;
                float z = (i / 10) * 2.0f - 9.0f;
                
                // Create a transformation matrix with animation
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(x, sin(time + i * 0.1f) * 0.5f, z));
                
                // Rotate based on time
                float rotationAngle = time * 50.0f + i * 10.0f;
                model = glm::rotate(model, glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
                
                // Scale based on time
                float scale = 0.5f + sin(time * 2.0f + i * 0.1f) * 0.2f;
                model = glm::scale(model, glm::vec3(scale));
                
                instanceMatrices[i] = model;
            }
            
            // Update instance buffer with new data
            instanceBuffer->setData(
                reinterpret_cast<const unsigned char*>(instanceMatrices.data()),
                instanceMatrices.size() * sizeof(glm::mat4)
            );
        }
        
        // Advanced example: Instanced rendering with multiple attributes
        static void SetupAdvancedInstancing(int instanceCount = 1000) {
            // For more advanced instancing, we might want to store more than just matrices
            struct InstanceData {
                glm::mat4 transform;
                glm::vec4 color;       // Instance-specific color
                float     timeOffset;  // Instance-specific animation offset
                float     padding[3];  // Padding for alignment
            };
            
            // Create instance data
            std::vector<InstanceData> instanceData(instanceCount);
            
            for (int i = 0; i < instanceCount; i++) {
                // Position in a 3D grid
                float x = (i % 10) * 2.0f - 9.0f;
                float y = ((i / 10) % 10) * 2.0f - 9.0f;
                float z = (i / 100) * 2.0f - 9.0f;
                
                // Initialize transform
                instanceData[i].transform = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                
                // Generate a distinct color for each instance
                instanceData[i].color = glm::vec4(
                    static_cast<float>(i % 255) / 255.0f,
                    static_cast<float>((i + 85) % 255) / 255.0f,
                    static_cast<float>((i + 170) % 255) / 255.0f,
                    1.0f
                );
                
                // Random time offset
                instanceData[i].timeOffset = static_cast<float>(i) * 0.01f;
            }
            
            // Create the instance buffer
            auto instanceBuffer = std::make_shared<VertexBuffer>(
                reinterpret_cast<const unsigned char*>(instanceData.data()),
                instanceData.size() * sizeof(InstanceData),
                BufferUsage::Dynamic  // Using dynamic since we'll update it
            );
            
            // Normally, you'd now set up the vertex array with multiple attributes
            // for the instance data (transform matrix, color, timeOffset)
            // This would involve custom VAO setup with attribute divisors
            
            // For demonstration purposes, let's use the native OpenGL API directly
            // (In a real implementation, you'd add these capabilities to VertexArray)
            
            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            
            // Bind the instance buffer
            instanceBuffer->bind();
            
            // Setup matrix attribute (4 attributes for the 4 vec4 columns)
            for (int i = 0; i < 4; i++) {
                glEnableVertexAttribArray(3 + i);  // Attribute locations 3, 4, 5, 6
                glVertexAttribPointer(
                    3 + i,                          // Location
                    4,                              // Size (vec4)
                    GL_FLOAT,                       // Type
                    GL_FALSE,                       // Normalized
                    sizeof(InstanceData),           // Stride
                    (const void*)(sizeof(float) * 4 * i)  // Offset (shifts by a vec4 each time)
                );
                glVertexAttribDivisor(3 + i, 1);   // This makes it per-instance
            }
            
            // Setup color attribute
            glEnableVertexAttribArray(7);  // Attribute location 7
            glVertexAttribPointer(
                7,                               // Location
                4,                               // Size (vec4)
                GL_FLOAT,                        // Type
                GL_FALSE,                        // Normalized
                sizeof(InstanceData),            // Stride
                (const void*)(sizeof(glm::mat4)) // Offset (after the mat4)
            );
            glVertexAttribDivisor(7, 1);     // This makes it per-instance
            
            // Setup timeOffset attribute
            glEnableVertexAttribArray(8);  // Attribute location 8
            glVertexAttribPointer(
                8,                                         // Location
                1,                                         // Size (float)
                GL_FLOAT,                                  // Type
                GL_FALSE,                                  // Normalized
                sizeof(InstanceData),                      // Stride
                (const void*)(sizeof(glm::mat4) + sizeof(glm::vec4))  // Offset
            );
            glVertexAttribDivisor(8, 1);     // This makes it per-instance
            
            glBindVertexArray(0);
            
            // In a real implementation, you would also set up the mesh vertices,
            // indices, and bind them to the VAO before rendering
        }
    };

} 