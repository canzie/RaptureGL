#pragma once

#include "../../Buffers.h"

namespace Rapture {

    
	// Modified UniformBuffer class with local CPU cache
	class UniformBuffer : public Buffer {
	public:
		UniformBuffer(size_t size, BufferUsage usage = BufferUsage::Dynamic, const void* data = nullptr, unsigned int bindingPoint = 0);
		virtual ~UniformBuffer();

		virtual void bind() override;
		virtual void unbind() override;
		void bindBase(unsigned int index);
		void bindBase();

		// Data modification methods
		void setData(const void* data, size_t size, size_t offset = 0);

		void* map(size_t offset = 0, size_t size = 0);
		void unmap();
		void flush() const; // Force synchronization
		
		// Added methods for CPU cache
		size_t getSize() const { return m_size; }
		unsigned int getBindingPoint() const { return m_bindingPoint; }
		
		virtual void setDebugLabel(const std::string& label) override;
		virtual unsigned int getID() const override { return m_rendererId; }
		

	private:
		unsigned int m_rendererId;
		size_t m_size;
		BufferUsage m_usage;
		bool m_isImmutable;
		bool m_isMapped;
		
		unsigned int m_bindingPoint = 0;
	};
    
}