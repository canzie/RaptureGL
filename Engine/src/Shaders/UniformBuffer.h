#pragma once

namespace Rapture
{
	class UniformBuffer
	{
	public:
		virtual ~UniformBuffer() = default;

		virtual void updateAllBufferData(const void* data) = 0;
		virtual void updateBufferData(long long offset, unsigned long long size, const void* data) = 0;
	};
} 