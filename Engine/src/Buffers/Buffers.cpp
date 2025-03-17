#include "Buffers.h"
#include "glad/glad.h"

namespace Rapture {
	VertexBuffer::VertexBuffer(std::vector<unsigned char>& vertices)
		: m_max_buffer_length(vertices.size()), m_idx_last_element(vertices.size())
	{
		glGenBuffers(1, &m_bufferID);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufferID);
		glBufferData(GL_ARRAY_BUFFER, vertices.size(), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

	}

	VertexBuffer::VertexBuffer(unsigned long long buffer_length)
		: m_max_buffer_length(buffer_length), m_idx_last_element(0)
	{
		glGenBuffers(1, &m_bufferID);
		glBindBuffer(GL_ARRAY_BUFFER, m_bufferID);
		glBufferData(GL_ARRAY_BUFFER, buffer_length, NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

	}

	VertexBuffer::~VertexBuffer()
	{
		glDeleteBuffers(1, &m_bufferID);
	}

	void Rapture::VertexBuffer::bind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_bufferID);
	}

	void VertexBuffer::unBind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void VertexBuffer::addSubData(std::vector<unsigned char>& binary_data)
	{
		if (binary_data.size() + m_idx_last_element > m_max_buffer_length)
		{
			GE_CORE_ERROR("Buffers/Buffer.cpp --- no space left in VertexBuffer({0})---", m_bufferID);
			GE_CORE_ERROR("given:{0}Bytes, max:{1}Bytes", binary_data.size() + m_idx_last_element, m_max_buffer_length);
			return;
		}

		m_idx_last_element += binary_data.size();
		m_premature_buffer_data.insert(m_premature_buffer_data.end(), binary_data.begin(), binary_data.end());
	}

	void VertexBuffer::pushData2Buffer(std::vector<std::vector<std::pair<size_t, size_t>>> premature_buffer_layout)
	{
		if (m_max_buffer_length != m_idx_last_element)
			GE_CORE_WARN("Data Pushed to Buffer has remaining free space: {0}Bytes", m_max_buffer_length-m_idx_last_element);


		std::vector<unsigned char> kak;
		kak.reserve(m_premature_buffer_data.size());

		// re-order the data in the corredt format
		// e.g VVVVNNNNTT_VVVVNNNNTT -> VVVVVVVVNNNNNNNNTTTT
		// based on the vecto
		for (int attrib = 0; attrib < premature_buffer_layout.size(); attrib++)
		{
			auto& attr_data = premature_buffer_layout[attrib];
			for (int i = 0; i < attr_data.size(); i++)
			{
				//GE_CORE_TRACE("{0}:({1}->{2})", attrib, attr_data[i].first, attr_data[i].second);
				kak.insert(kak.end(), 
					m_premature_buffer_data.begin() + attr_data[i].first,
					m_premature_buffer_data.begin() + attr_data[i].second);

			}

		}

		glNamedBufferSubData(m_bufferID, 0, kak.size(), kak.data());
		m_premature_buffer_data = std::vector<unsigned char>();
	}









	IndexBuffer::IndexBuffer(std::vector<unsigned char>& indices, unsigned int comp_type)
		: m_componentType(comp_type), m_idx_last_element(indices.size()), m_max_buffer_length(indices.size())
	{
		int t = 1;
		switch (comp_type)
		{
		case GL_BYTE: case GL_UNSIGNED_BYTE: t = 1; break;
		case GL_UNSIGNED_SHORT: case GL_SHORT: t = 2; break;
		case GL_FLOAT: case GL_UNSIGNED_INT: t = 4; break;
		}
		m_count = indices.size() / t;
		glGenBuffers(1, &m_bufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size(), indices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	}
	IndexBuffer::IndexBuffer(unsigned long long buffer_length, unsigned int comp_type)
		: m_componentType(comp_type), m_idx_last_element(0), m_max_buffer_length(buffer_length)
	{
		int t = 1;
		switch (comp_type)
		{
		case GL_BYTE: case GL_UNSIGNED_BYTE: t = 1; break;
		case GL_UNSIGNED_SHORT: case GL_SHORT: t = 2; break;
		case GL_FLOAT: case GL_UNSIGNED_INT: t = 4; break;
		}
		m_count = buffer_length / t;

		glGenBuffers(1, &m_bufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer_length, NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	}
	IndexBuffer::~IndexBuffer()
	{
		glDeleteBuffers(1, &m_bufferID);
	}

	void IndexBuffer::bind()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bufferID);
	}

	void IndexBuffer::unBind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	void IndexBuffer::addSubIndices(std::vector<unsigned char>& indices)
	{
		if (indices.size()+m_idx_last_element > m_max_buffer_length)
		{
			GE_CORE_ERROR("Buffers/Buffer.cpp --- no space left in IndexBuffer({0})---", m_bufferID);
			GE_CORE_ERROR("{0}, {1}", indices.size() + m_idx_last_element, m_max_buffer_length);
			return;
		}
		glNamedBufferSubData(m_bufferID, m_idx_last_element, indices.size(), indices.data());
		m_idx_last_element += indices.size();
	}
}