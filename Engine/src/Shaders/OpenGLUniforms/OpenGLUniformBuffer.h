#pragma once

#include "../UniformBuffer.h"
#include <cstdint>

namespace Rapture
{
	class OpenGLUniformBuffer : public UniformBuffer
	{
	public:
		OpenGLUniformBuffer(uint64_t size, uint32_t binding);
		~OpenGLUniformBuffer() override;

		// Data update methods
		void updateAllBufferData(const void* data) override;
		void updateBufferData(int64_t offset, uint64_t size, const void* data) override;
		
		// Buffer binding/state methods
		void bind() const override;
		void unbind() const override;
		void flush() const override;
		
		// Accessors
		uint32_t getRendererID() const override { return m_uboID; }
		uint32_t getBindingPoint() const override { return m_binding; }
		uint64_t getSize() const override { return m_size; }
		const void* getData() const override { return m_data; }

	private:
		uint32_t m_uboID = 0;
		uint32_t m_binding = 0;
		uint64_t m_size = 0;
		void* m_data = nullptr;
	};
}

