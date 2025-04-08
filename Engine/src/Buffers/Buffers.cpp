#include "Buffers.h"
#include "glad/glad.h"
#include <iostream>
#include "GLFW/glfw3.h"
#include "OpenGLBuffers/VertexBuffers/OpenGLVertexBuffer.h"
#include "OpenGLBuffers/IndexBuffers/OpenGLIndexBuffer.h"
#include "OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h"


namespace Rapture {


    // Buffer factory method
	std::shared_ptr<Buffer> Buffer::Create(BufferType type, size_t size, BufferUsage usage, const void* data) {
		switch (type) {
			case BufferType::Vertex:
				return std::make_shared<VertexBuffer>(size, usage, data);
			case BufferType::Index:
				return std::make_shared<IndexBuffer>(size, GL_UNSIGNED_INT, usage, data);
			case BufferType::Uniform:
				return std::make_shared<UniformBuffer>(size, usage, data);
			case BufferType::ShaderStorage:
				return std::make_shared<ShaderStorageBuffer>(size, usage, data);
			default:
				GE_CORE_ERROR("Unknown buffer type");
				return nullptr;
		}
	}




}