#include "../../Buffers.h"

namespace Rapture {


	class ShaderStorageBuffer : public Buffer {
	public:
		ShaderStorageBuffer(size_t size, BufferUsage usage = BufferUsage::Dynamic, const void* data = nullptr);
		virtual ~ShaderStorageBuffer();

		virtual void bind() override;
		virtual void unbind() override;
		void bindBase(unsigned int index);
		
		void setData(const void* data, size_t size, size_t offset = 0);
		void* map(size_t offset = 0, size_t size = 0);
		void unmap();
		
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
