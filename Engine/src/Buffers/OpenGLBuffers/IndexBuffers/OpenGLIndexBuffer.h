#pragma once

#include "../../Buffers.h"

namespace Rapture {
	class IndexBuffer : public Buffer {
	public:
		IndexBuffer(size_t size, unsigned int componentType, BufferUsage usage = BufferUsage::Static, const void* data = nullptr);
		IndexBuffer(const std::vector<unsigned char>& data, unsigned int componentType, BufferUsage usage = BufferUsage::Static);
		virtual ~IndexBuffer();

		virtual void bind() override;
		virtual void unbind() override;
		void bindBase(uint32_t bindingPoint) const;

		void setData(const void* data, size_t size, size_t offset = 0);
		void setData(const std::vector<unsigned char>& data, size_t offset = 0);
		
		virtual void setDebugLabel(const std::string& label) override;
		virtual unsigned int getID() const override { return m_rendererId; }

		unsigned int getCount() const { return m_count; }
		unsigned int getComponentType() const { return m_componentType; }
		
		// Compatibility method for existing code
		unsigned int getID__DEBUG() const { return m_rendererId; }
		unsigned int getIndexCount() const { return m_count; }
		unsigned int getIndexType() const { return m_componentType; }

        uint64_t getBufferHandle() const { return m_bufferHandle; }
		
		// Legacy method for compatibility
		void addSubIndices(std::vector<unsigned char>& indices);
		
    private:
        void generateBufferHandle();

	private:
		unsigned int m_rendererId;
		unsigned int m_count;
		unsigned int m_componentType;
		size_t m_size;
		BufferUsage m_usage;
		bool m_isImmutable;

        uint64_t m_bufferHandle;
		
		// For legacy support
		size_t m_idx_last_element;
	};
}
