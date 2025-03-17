#pragma once

#include "../UniformBuffer.h"

namespace Rapture
{
	class OpenGLUniformBuffer : public UniformBuffer
	{
	public:
		OpenGLUniformBuffer(unsigned long long size, unsigned int idx);
		~OpenGLUniformBuffer() override;

		void updateAllBufferData(const void* data) override;
		void updateBufferData(long long offset, unsigned long long size, const void* data) override;

	private:
		unsigned int m_uboID;
		unsigned long long m_size;
	};
}

