#pragma once

#include "../../Buffers.h"



namespace Rapture { 

class VertexBuffer : public Buffer {
	public:
		VertexBuffer(size_t size, BufferUsage usage = BufferUsage::Static, const void* data = nullptr);
		VertexBuffer(const std::vector<unsigned char>& data, BufferUsage usage = BufferUsage::Static);
		virtual ~VertexBuffer();

		virtual void bind() override;
		virtual void unbind() override;
		void bindBase(uint32_t bindingPoint) const;
        
        uint64_t getBufferHandle() const { return m_bufferHandle; }


		void setData(const void* data, size_t size, size_t offset = 0);
		void setData(const std::vector<unsigned char>& data, size_t offset = 0);
		
		virtual void setDebugLabel(const std::string& label) override;
		virtual unsigned int getID() const override { return m_rendererId; }

		// Compatibility method for existing code
		unsigned int getID__DEBUG() const { return m_rendererId; }
		

    private:
        void generateBufferHandle();
	
    private:
		unsigned int m_rendererId;
		size_t m_size;
		BufferUsage m_usage;
		bool m_isImmutable;

        uint64_t m_bufferHandle = 0;
        bool m_isResident = false;

	};
}
