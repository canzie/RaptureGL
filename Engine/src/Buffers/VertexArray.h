#pragma once

#include <memory>
#include "Buffers.h"

#include "OpenGLBuffers/VertexBuffers/OpenGLVertexBuffer.h"
#include "OpenGLBuffers/IndexBuffers/OpenGLIndexBuffer.h"


namespace Rapture {


	struct BufferAttribute
	{
		std::string name;
		unsigned int componentType; // GL_FLOAT, GL_INT, ... 
		std::string type; // SCALAR, VEC2, VEC3, VEC4, ...
		size_t offset;    // Byte offset from the start of the vertex or attribute array

		// Calculate size in bytes for this attribute
		size_t getSizeInBytes() const {
			size_t elementSize = 1; // For SCALAR
			if (type == "VEC2") elementSize = 2;
			else if (type == "VEC3") elementSize = 3;
			else if (type == "VEC4") elementSize = 4;
			else if (type == "MAT4") elementSize = 16;

			size_t componentSize = 1;
			switch (componentType) {
				case 0x1400: case 0x1401: componentSize = 1; break; // BYTE, UNSIGNED_BYTE
				case 0x1402: case 0x1403: componentSize = 2; break; // SHORT, UNSIGNED_SHORT
				case 0x1404: case 0x1406: componentSize = 4; break; // INT/UNSIGNED_INT, FLOAT
			}

			return elementSize * componentSize;
		}

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
		bool isInterleaved = false;     // Whether vertex data is interleaved (PNTPNT...) or not (PPP...NNN...TTT...)
		size_t vertexSize = 0;          // Total size of a vertex in bytes (used for interleaved format)

		// Calculate the total vertex size for interleaved format
		void calculateVertexSize() {
			vertexSize = 0;
			for (const auto& attrib : buffer_attribs) {
				vertexSize += attrib.getSizeInBytes();
			}
		}

		// Get an attribute by name
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

		// Update offsets based on layout type (interleaved or not)
		void updateOffsets() {
			if (isInterleaved) {
				// For interleaved format, offsets are relative to the start of each vertex
				size_t currentOffset = 0;
				for (auto& attrib : buffer_attribs) {
					attrib.offset = currentOffset;
					currentOffset += attrib.getSizeInBytes();
				}
				vertexSize = currentOffset;
			}
		}

		bool operator==(const BufferLayout& other) const
		{
			if (other.buffer_attribs.size() != buffer_attribs.size() ||
			    other.isInterleaved != isInterleaved) 
				return false;
			
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				if (buffer_attribs[i] != other.buffer_attribs[i]) return false;
			}
			return true;
		}

        size_t hash() const {
            size_t hash = 0;
            for (const auto& attrib : buffer_attribs) {
                // Combine hashes of the attribute properties
                size_t attribHash = std::hash<std::string>()(attrib.name) ^
                            (std::hash<unsigned int>()(attrib.componentType) << 1) ^
                            (std::hash<std::string>()(attrib.type) << 2) ^
                            (std::hash<size_t>()(attrib.offset) << 3);
                hash ^= attribHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            // Add interleaved flag to the hash
            hash ^= std::hash<bool>()(isInterleaved) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }


		void print() const
		{
			GE_CORE_TRACE("Buffer Layout: {0}", isInterleaved ? "Interleaved" : "Non-interleaved");
			GE_CORE_TRACE("Vertex size: {0} bytes", vertexSize);
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				GE_CORE_TRACE("'{0}': {1}, {2}, offset: {3}, size: {4}", 
					buffer_attribs[i].name, 
					buffer_attribs[i].componentType, 
					buffer_attribs[i].type, 
					buffer_attribs[i].offset,
					buffer_attribs[i].getSizeInBytes());
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