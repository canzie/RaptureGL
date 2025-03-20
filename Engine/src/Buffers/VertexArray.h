#pragma once

#include <memory>
#include "Buffers.h"

#include "OpenGLBuffers/VertexBuffers/OpenGLVertexBuffer.h"
#include "OpenGLBuffers/IndexBuffers/OpenGLIndexBuffer.h"


namespace Rapture {

	// Legacy code kept for reference
	/*
	class VertexArray
	{
	public:

		VertexArray();
		~VertexArray();

		void bind() const;
		void unbind() const;

		void addVertexBuffer(std::shared_ptr<VertexBuffer>& vertexBuffer) ;
		void setIndexBuffer(std::shared_ptr<IndexBuffer>& indexBuffer) ;

		const std::shared_ptr<VertexBuffer>& getVertexBuffers() const { return m_vertexBuffer; };
		const std::shared_ptr<IndexBuffer>& getIndexBuffer() const { return m_indexBuffer; };

		inline unsigned int getID() { return m_bufferID; }

	private:
		unsigned int m_bufferID;
		std::shared_ptr<VertexBuffer> m_vertexBuffer;
		std::shared_ptr<IndexBuffer> m_indexBuffer;

	};
	*/

    struct BufferLayoutElement {
        std::string name;          // Name of the element (e.g., "POSITION", "NORMAL", "TEXCOORD")
        unsigned int componentType; // Data type (GL_FLOAT, GL_INT, etc.)
        std::string type;          // Component structure (SCALAR, VEC2, VEC3, VEC4, etc.)
        unsigned int stride;       // Bytes between consecutive vertices
        size_t offset;             // Byte offset within the vertex
    };

	struct BufferAttribute
	{
		std::string name;
		unsigned int componentType; // GL_FLOAT, GL_INT, ... 
		std::string type; // SCALAR, VEC2, VEC3, VEC4, ...
		size_t offset;

		bool operator==(const BufferAttribute& other) const
		{
			return (other.name == name &&
				other.offset == offset &&
				other.componentType == componentType &&
				other.type == type);
		}

		bool operator!=(const BufferAttribute& other) const
		{
			return !(*this == other);
		}
	};

	struct BufferLayout
	{
		std::vector<BufferAttribute> buffer_attribs;

		BufferAttribute& getAttribute(const std::string& name)
		{
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				if (buffer_attribs[i].name == name)
				{
					return buffer_attribs[i];
				}
			}
			GE_CORE_ERROR("Attribute not found: {0}", name);
            return buffer_attribs[0];
		}

		bool operator==(const BufferLayout& other) const
		{
			if (other.buffer_attribs.size() != buffer_attribs.size()) return false;
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				if (buffer_attribs[i] != other.buffer_attribs[i]) return false;
			}
			return true;
		}

		void print_buffer_layout() const
		{
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				GE_CORE_TRACE("'{0}': {1}, {2}, {3}", buffer_attribs[i].name, buffer_attribs[i].componentType, buffer_attribs[i].type, buffer_attribs[i].offset);
			}
		}
	};

	class VertexArray
	{
	public:
		VertexArray();
		~VertexArray();

		void bind() const;
		void unbind() const;

		void setAttribLayout(BufferAttribute& el);
		void setBufferLayout(const BufferLayout& el);
        BufferLayout& getBufferLayout() { return m_buffer_layout; }
		
		// Modern vertex buffer management
		void setVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer);
		void setIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer);
		
		// Legacy methods for compatibility
		void setVertexBuffer(std::vector<unsigned char>& vertices);
		void setVertexBuffer(unsigned long long buffer_length);
		void setIndexBuffer(unsigned long long buffer_length, unsigned int comp_count);

		// Accessors
		const std::shared_ptr<IndexBuffer>& getIndexBuffer() const { return m_indexBuffer; }
		const std::shared_ptr<VertexBuffer>& getVertexBuffer() const { return m_vertexBuffer; }
		
		// Debug utilities
		void setDebugLabel(const std::string& label);
		unsigned int getID() const { return m_rendererId; }

	private:
		unsigned int m_rendererId;
		BufferLayout m_buffer_layout;
		std::shared_ptr<VertexBuffer> m_vertexBuffer;
		std::shared_ptr<IndexBuffer> m_indexBuffer;
	};
}