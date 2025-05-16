#pragma once

#include "../../Buffers.h"

namespace Rapture {

    enum class BufferInternalFormats {
        R32UI,
        R32F,
        R16UI,
        R16F,
        R8UI,
        R8F,
        RGBA8UI,
        RGBA8I,
        RGBA8,
        RGBA16F,
        RGBA32F,
        RGBA16UI,
        RGBA16I
    };

    struct SSBOBarrierFlags {
        bool atomic = false;
        bool bufferUpdate = false;
    };


	class ShaderStorageBuffer : public Buffer {
	public:
		ShaderStorageBuffer(size_t size, BufferUsage usage = BufferUsage::Dynamic, const void* data = nullptr);
		virtual ~ShaderStorageBuffer();

		virtual void bind() override;
		virtual void unbind() override;
		void bindBase(unsigned int bindingPoint);

        void resize(size_t newSize);
		
		void setData(const void* data, size_t size, size_t offset = 0);
		void* map(size_t offset = 0, size_t size = 0);
		void unmap();

        void* getPersistentPtr() { return m_persistentlyMappedPtr; }

        void barrier();
        static void barrier(SSBOBarrierFlags flags);

        void clear(BufferInternalFormats format=BufferInternalFormats::R32UI);
		
		virtual void setDebugLabel(const std::string& label) override;
		virtual unsigned int getID() const override { return m_rendererId; }
		
	private:
		unsigned int m_rendererId;
		size_t m_size;
		BufferUsage m_usage;
		bool m_isImmutable;
		bool m_isMapped;
		unsigned int m_bindingPoint = 0;

        void* m_persistentlyMappedPtr = nullptr;

	};
    
    
}
