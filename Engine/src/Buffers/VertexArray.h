#pragma once

#include <memory>
#include "Buffers.h"


namespace Rapture {

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


	struct BufferLayoutElement
	{
		std::string name;
		unsigned int componentType; // GL_FLOAT, GL_INT, ... 
		std::string type; // SCALAR, VEC2, VEC3, VEC4, ...
		unsigned int stride;
		size_t offset;

	};

	struct BufferAttribute
	{
		std::string name;
		unsigned int componentType; // GL_FLOAT, GL_INT, ... 
		std::string type; // SCALAR, VEC2, VEC3, VEC4, ...
		//unsigned int stride;
		size_t offset;

		bool operator==(const BufferAttribute& other)
		{
			(other.name == name &&
				other.offset == offset &&
				other.componentType == componentType &&
				other.type == type  
				//other.stride == stride
				);
		}

	};
	struct BufferLayout
	{
		std::vector<BufferAttribute> buffer_attribs;

		BufferAttribute& getAttribute(std::string name)
		{
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				if (buffer_attribs[i].name == name)
				{
					return buffer_attribs[i];
				}
			}
			GE_CORE_ERROR("shit not found dude: {0}", name);
            return buffer_attribs[0];
		}

		bool operator==(const BufferLayout& other)
		{
			if (other.buffer_attribs.size() != buffer_attribs.size()) return false;
			for (int i = 0; i < buffer_attribs.size(); i++)
			{
				if (buffer_attribs[i] != other.buffer_attribs[i]) return false;
			}
			return true;
		}

		void print_buffer_layout()
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
		void setBufferLayout(BufferLayout el);

		void setVertexBuffer(std::vector<unsigned char>& vertices);
		void setVertexBuffer(unsigned long long buffer_length);

		void setIndexBuffer(unsigned long long buffer_length, unsigned int comp_count);
		//void addIndexBufferData(std::vector<unsigned char>& indices);

		const std::shared_ptr<IndexBuffer>& getIndexBuffer() const { return m_indexBuffer; };
		const std::shared_ptr<VertexBuffer>& getVertexBuffer() const { return m_vertexBuffer; };


	private:

		unsigned int m_bufferID;
		BufferLayout m_buffer_layout;
		std::shared_ptr<VertexBuffer> m_vertexBuffer;
		std::shared_ptr<IndexBuffer> m_indexBuffer;

	};




}