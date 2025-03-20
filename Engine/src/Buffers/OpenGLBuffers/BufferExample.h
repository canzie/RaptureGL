#pragma once

#include <memory>
#include <vector>
#include "../Buffers.h"
#include "OpenGLBuffers.h"

namespace Rapture {

    // Example showing how to use the new buffer system
    class BufferExample {
    public:
        // Example of creating and using a static vertex buffer
        static void StaticBufferExample() {
            // Create a simple triangle
            std::vector<float> vertices = {
                -0.5f, -0.5f, 0.0f,  // Position
                 0.0f,  0.5f, 0.0f,  // Position
                 0.5f, -0.5f, 0.0f   // Position
            };

            // Create a static vertex buffer
            auto vertexBuffer = std::make_shared<VertexBuffer>(
                reinterpret_cast<const unsigned char*>(vertices.data()),
                vertices.size() * sizeof(float),
                BufferUsage::Static
            );

            // Set a debug label for easier debugging
            vertexBuffer->setDebugLabel("TriangleVertices");

            // Bind the buffer for use
            vertexBuffer->bind();

            // Unbind when done
            vertexBuffer->unbind();
        }

        // Example of dynamic buffer usage
        static void DynamicBufferExample() {
            // Initial data
            std::vector<float> initialData = {
                -0.5f, -0.5f, 0.0f,
                 0.0f,  0.5f, 0.0f,
                 0.5f, -0.5f, 0.0f
            };

            // Create a dynamic buffer
            auto vertexBuffer = std::make_shared<VertexBuffer>(
                initialData.size() * sizeof(float),
                BufferUsage::Dynamic
            );

            // Set initial data
            vertexBuffer->setData(
                reinterpret_cast<const unsigned char*>(initialData.data()),
                initialData.size() * sizeof(float)
            );

            // Later, update part of the buffer
            std::vector<float> newVertices = {
                -0.3f, -0.3f, 0.0f  // Updated first vertex
            };
            
            // Update just the first vertex (first 3 floats = 12 bytes)
            vertexBuffer->setData(
                reinterpret_cast<const unsigned char*>(newVertices.data()),
                newVertices.size() * sizeof(float),
                0  // offset from start
            );
        }

        // Example of using index buffer
        static void IndexBufferExample() {
            // Create vertices for a quad (4 vertices)
            std::vector<float> vertices = {
                -0.5f, -0.5f, 0.0f,  // Bottom-left
                 0.5f, -0.5f, 0.0f,  // Bottom-right
                 0.5f,  0.5f, 0.0f,  // Top-right
                -0.5f,  0.5f, 0.0f   // Top-left
            };

            // Create indices to form two triangles from the quad
            std::vector<unsigned int> indices = {
                0, 1, 2,  // First triangle
                2, 3, 0   // Second triangle
            };

            // Create vertex buffer
            auto vertexBuffer = std::make_shared<VertexBuffer>(
                reinterpret_cast<const unsigned char*>(vertices.data()),
                vertices.size() * sizeof(float),
                BufferUsage::Static
            );

            // Create index buffer
            auto indexBuffer = std::make_shared<IndexBuffer>(
                reinterpret_cast<const unsigned char*>(indices.data()),
                indices.size() * sizeof(unsigned int),
                GL_UNSIGNED_INT,
                BufferUsage::Static
            );

            // Use them together
            vertexBuffer->bind();
            indexBuffer->bind();

            // Draw using indices
            // glDrawElements(GL_TRIANGLES, indexBuffer->getCount(), indexBuffer->getComponentType(), nullptr);

            // Unbind
            indexBuffer->unbind();
            vertexBuffer->unbind();
        }

        // Example of using uniform buffer objects
        static void UniformBufferExample() {
            // Create a uniform buffer for common transform data
            struct TransformUBO {
                glm::mat4 projection;
                glm::mat4 view;
                glm::vec4 cameraPosition;
            };

            TransformUBO transform;
            transform.projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
            transform.view = glm::lookAt(
                glm::vec3(0.0f, 0.0f, 3.0f),  // Camera position
                glm::vec3(0.0f, 0.0f, 0.0f),  // Target
                glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
            );
            transform.cameraPosition = glm::vec4(0.0f, 0.0f, 3.0f, 1.0f);

            // Create UBO
            auto ubo = std::make_shared<UniformBuffer>(
                sizeof(TransformUBO),
                BufferUsage::Dynamic,
                &transform
            );

            // Bind to binding point 0
            ubo->bindBase(0);

            // Later, update a part of the UBO (just the camera position)
            glm::vec4 newCameraPos(1.0f, 2.0f, 3.0f, 1.0f);
            ubo->setData(
                &newCameraPos,
                sizeof(glm::vec4),
                offsetof(TransformUBO, cameraPosition)
            );
        }

        // Example of using buffer pool for efficient buffer reuse
        static void BufferPoolExample() {
            // Get the buffer pool instance
            auto& bufferPool = BufferPool::getInstance();

            // Acquire a vertex buffer from the pool
            auto vertexBuffer = bufferPool.acquireBuffer(
                BufferType::Vertex,
                1024 * sizeof(float),  // 1024 floats
                BufferUsage::Dynamic
            );

            // Use the buffer
            // ...

            // When done, return it to the pool for reuse
            bufferPool.releaseBuffer(vertexBuffer);

            // At the end of each frame
            bufferPool.nextFrame();
        }

        // Example of using shared storage buffer objects (for compute shaders)
        static void StorageBufferExample() {
            // Create a SSBO for particle data (1000 particles with position, velocity, color)
            struct Particle {
                glm::vec4 position;
                glm::vec4 velocity;
                glm::vec4 color;
            };

            // Allocate storage for particles
            const size_t particleCount = 1000;
            auto ssbo = std::make_shared<ShaderStorageBuffer>(
                sizeof(Particle) * particleCount,
                BufferUsage::Dynamic
            );

            // Optionally initialize with data
            std::vector<Particle> initialParticles(particleCount);
            for (size_t i = 0; i < particleCount; i++) {
                initialParticles[i].position = glm::vec4(0.0f);
                initialParticles[i].velocity = glm::vec4(
                    static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f,  // Random velocity
                    static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f,
                    static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f,
                    0.0f
                );
                initialParticles[i].color = glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);
            }

            // Set initial data
            ssbo->setData(initialParticles.data(), sizeof(Particle) * particleCount);

            // Bind to shader storage binding point 0
            ssbo->bindBase(0);

            // For compute shader use (would typically be part of a compute shader dispatch)
            // glDispatchCompute(particleCount / 64, 1, 1);
            // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // Can also map the buffer for CPU access
            Particle* mappedData = static_cast<Particle*>(ssbo->map());
            if (mappedData) {
                // Access/modify data directly
                mappedData[0].position = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
                
                // Unmap when done
                ssbo->unmap();
            }
        }
    };

} 