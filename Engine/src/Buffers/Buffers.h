#pragma once

#include <vector>
#include <glm/glm.hpp>

#include <utility>

#include "../logger/Log.h"


#include "../DataTypes.h"

namespace Rapture {



	class VertexBuffer
	{
	public:
		VertexBuffer(std::vector<unsigned char>& vertices);
		VertexBuffer(unsigned long long buffer_length);

		~VertexBuffer();

		void bind();
		void unBind();

		//void setLayout(const BufferLayout& layout) { m_layout = layout; }
		//const BufferLayout& getLayout() const { return m_layout; }

		//unsigned int getVertexCount() const { return m_count; }
		void addSubData(std::vector<unsigned char>& binary_data);
		void pushData2Buffer(std::vector < std::vector<std::pair<size_t, size_t>>> premature_buffer_layout);

		unsigned int getID__DEBUG() const { return m_bufferID; }

	private:
		// stores the contents that still need to be pushed to the buffer. (by calling pushData2Buffer())
		std::vector<unsigned char> m_premature_buffer_data;

		// 0: [0->x, y->z, u->v] | 1: [x->y], [...], ...


		//unsigned int m_count;
		unsigned int m_bufferID;
		unsigned long long m_max_buffer_length;
		unsigned int long long m_idx_last_element;
		//BufferLayout m_layout;

	};



	class IndexBuffer 
	{
	public:
		IndexBuffer(std::vector<unsigned char>& indices, unsigned int comp_count);
		IndexBuffer(unsigned long long buffer_length, unsigned int comp_count);

		~IndexBuffer();

		void bind();
		void unBind();

		unsigned int getIndexCount() const { return m_count; }
		unsigned int getIndexType() const { return m_componentType; }

		void addSubIndices(std::vector<unsigned char>& indices);

		unsigned int getID__DEBUG() const { return m_bufferID; }

	private:
		// total amoutn of elements in the buffer
		unsigned int m_count;
		// type of the contents stored in the buffer as GLenum
		unsigned int m_componentType;
		unsigned long long m_max_buffer_length;
		unsigned int long long m_idx_last_element;
		unsigned int m_bufferID;



	};

}