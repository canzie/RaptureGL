#include "VertexArray.h"
#include "glad/glad.h"
#include "../logger/Log.h"
#include "../Debug/TracyProfiler.h"

namespace Rapture {

	// Constants for attribute locations (kept from original code for compatibility)
	#define POSITION_ATTRIB_PTR 0
	#define NORMAL_ATTRIB_PTR 1
	#define TEXCOORD_0_ATTRIB_PTR 2
	#define TEXCOORD_1_ATTRIB_PTR 3
	#define JOINTS_0_ATTRIB_PTR 4
	#define WEIGTHS_0_ATTRIB_PTR 5
	#define TANGENT_ATTRIB_PTR 6
	#define TRANSFORM_ATTRIB_PTR 7 // mat4

	VertexArray::VertexArray()
		: m_vertexBuffer(nullptr), m_indexBuffer(nullptr)
	{
		if (GLCapabilities::hasDSA()) {
			glCreateVertexArrays(1, &m_rendererId);
		} else {
			glGenVertexArrays(1, &m_rendererId);
		}
	}

	VertexArray::~VertexArray()
	{
		glDeleteVertexArrays(1, &m_rendererId);
	}
	
	void VertexArray::bind() const
	{
		RAPTURE_PROFILE_SCOPE("VAO Bind");
		glBindVertexArray(m_rendererId);
		// When using DSA, we don't need to explicitly bind buffers here
		if (!GLCapabilities::hasDSA()) {
			if (m_indexBuffer != nullptr)
				m_indexBuffer->bind();
			if (m_vertexBuffer != nullptr)
				m_vertexBuffer->bind();
		}
	}

	void VertexArray::unbind() const
	{
		RAPTURE_PROFILE_SCOPE("VAO Unbind");
		glBindVertexArray(0);
	}

	void VertexArray::setDebugLabel(const std::string& label)
	{
		if (GLCapabilities::hasDebugMarkers()) {
			glObjectLabel(GL_VERTEX_ARRAY, m_rendererId, -1, label.c_str());
		}
	}

	void VertexArray::setAttribLayout(BufferAttribute& el)
	{
		GLuint size = 1;
		GLuint componentStride = 1;

		if (el.type == "SCALAR") size = 1;
		else if (el.type == "VEC2") size = 2;
		else if (el.type == "VEC3") size = 3;
		else if (el.type == "VEC4") size = 4;
		else if (el.type == "MAT4") size = 16;
		else GE_CORE_ERROR("VertexArray: Invalid Buffer Attribute Type: '{0}'", el.type);

		switch (el.componentType)
		{
		case GL_BYTE: case GL_UNSIGNED_BYTE: componentStride = 1; break;
		case GL_UNSIGNED_SHORT: case GL_SHORT: componentStride = 2; break;
		case GL_FLOAT: case GL_UNSIGNED_INT: componentStride = 4; break;
		}

		// Calculate stride based on layout type
		GLsizei stride;
		size_t attributeOffset = el.offset;
		
		if (m_buffer_layout.isInterleaved) {
			// For interleaved format (PNTPNTPNT...), stride is the size of a complete vertex
			stride = (GLsizei)m_buffer_layout.vertexSize;
		} else {
			// For non-interleaved format (PPP...NNN...TTT...), stride is just the size of this component
			stride = componentStride * size;
		}

		if (m_vertexBuffer == nullptr) {
			GE_CORE_ERROR("VertexArray: Cannot set attribute layout without a vertex buffer");
			return;
		}

		if (GLCapabilities::hasDSA()) {
			// Use DSA for setting up attributes
			if (el.name == "POSITION") {
				glEnableVertexArrayAttrib(m_rendererId, POSITION_ATTRIB_PTR);
				glVertexArrayAttribBinding(m_rendererId, POSITION_ATTRIB_PTR, POSITION_ATTRIB_PTR);
				glVertexArrayAttribFormat(m_rendererId, POSITION_ATTRIB_PTR, size,
					(GLenum)el.componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(m_rendererId, POSITION_ATTRIB_PTR, m_vertexBuffer->getID(), 
                     attributeOffset, stride);
				
			} else if (el.name == "NORMAL") {
				glEnableVertexArrayAttrib(m_rendererId, NORMAL_ATTRIB_PTR);
				glVertexArrayAttribBinding(m_rendererId, NORMAL_ATTRIB_PTR, NORMAL_ATTRIB_PTR);
				glVertexArrayAttribFormat(m_rendererId, NORMAL_ATTRIB_PTR, size, 
					(GLenum)el.componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(m_rendererId, NORMAL_ATTRIB_PTR, m_vertexBuffer->getID(), 
                     attributeOffset, stride);

			} else if (el.name == "TEXCOORD_0") {
				glEnableVertexArrayAttrib(m_rendererId, TEXCOORD_0_ATTRIB_PTR);
				glVertexArrayAttribBinding(m_rendererId, TEXCOORD_0_ATTRIB_PTR, TEXCOORD_0_ATTRIB_PTR);
				glVertexArrayAttribFormat(m_rendererId, TEXCOORD_0_ATTRIB_PTR, size, 
					(GLenum)el.componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(m_rendererId, TEXCOORD_0_ATTRIB_PTR, m_vertexBuffer->getID(), 
                     attributeOffset, stride);

			} else if (el.name == "TRANSFORM_MAT") {
				// Handle matrix attribute (instanced)
				for (int i = 0; i < 4; i++) {
					glEnableVertexArrayAttrib(m_rendererId, TRANSFORM_ATTRIB_PTR + i);
					glVertexArrayAttribBinding(m_rendererId, TRANSFORM_ATTRIB_PTR + i, TRANSFORM_ATTRIB_PTR);
					glVertexArrayAttribFormat(m_rendererId, TRANSFORM_ATTRIB_PTR + i, 4, 
						GL_FLOAT, GL_FALSE, i * sizeof(glm::vec4));
				}
				glVertexArrayBindingDivisor(m_rendererId, TRANSFORM_ATTRIB_PTR, 1);
				glVertexArrayVertexBuffer(m_rendererId, TRANSFORM_ATTRIB_PTR, m_vertexBuffer->getID(), 
                     attributeOffset, stride);

			} else if (el.name == "TANGENT") {
				glEnableVertexArrayAttrib(m_rendererId, TANGENT_ATTRIB_PTR);
				glVertexArrayAttribBinding(m_rendererId, TANGENT_ATTRIB_PTR, TANGENT_ATTRIB_PTR);
				glVertexArrayAttribFormat(m_rendererId, TANGENT_ATTRIB_PTR, size, 
					(GLenum)el.componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(m_rendererId, TANGENT_ATTRIB_PTR, m_vertexBuffer->getID(), 
                     attributeOffset, stride);

			} else if (el.name == "JOINTS_0") {
				glEnableVertexArrayAttrib(m_rendererId, JOINTS_0_ATTRIB_PTR);
				glVertexArrayAttribBinding(m_rendererId, JOINTS_0_ATTRIB_PTR, JOINTS_0_ATTRIB_PTR);
				glVertexArrayAttribFormat(m_rendererId, JOINTS_0_ATTRIB_PTR, size, 
					(GLenum)el.componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(m_rendererId, JOINTS_0_ATTRIB_PTR, m_vertexBuffer->getID(), 
                     attributeOffset, stride);

			} else if (el.name == "WEIGHTS_0") {
				glEnableVertexArrayAttrib(m_rendererId, WEIGTHS_0_ATTRIB_PTR);
				glVertexArrayAttribBinding(m_rendererId, WEIGTHS_0_ATTRIB_PTR, WEIGTHS_0_ATTRIB_PTR);
				glVertexArrayAttribFormat(m_rendererId, WEIGTHS_0_ATTRIB_PTR, size, 
					(GLenum)el.componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(m_rendererId, WEIGTHS_0_ATTRIB_PTR, m_vertexBuffer->getID(), 
                    attributeOffset, stride);
			}
		} else {
			// For non-DSA fallback
			glBindVertexArray(m_rendererId);
			m_vertexBuffer->bind();

			if (el.name == "POSITION") {
				glEnableVertexAttribArray(POSITION_ATTRIB_PTR);
				glVertexAttribPointer(POSITION_ATTRIB_PTR,
					size,
					(GLenum)el.componentType,
					GL_FALSE,
					stride,
					(const void*)(attributeOffset));
			} else if (el.name == "NORMAL") {
				glEnableVertexAttribArray(NORMAL_ATTRIB_PTR);
				glVertexAttribPointer(NORMAL_ATTRIB_PTR,
					size,
					(GLenum)el.componentType,
					GL_FALSE,
					stride,
					(const void*)(attributeOffset));
			} else if (el.name == "TEXCOORD_0") {
				glEnableVertexAttribArray(TEXCOORD_0_ATTRIB_PTR);
				glVertexAttribPointer(TEXCOORD_0_ATTRIB_PTR,
					size,
					(GLenum)el.componentType,
					GL_FALSE,
					stride,
					(const void*)(attributeOffset));
			} else if (el.name == "TRANSFORM_MAT") {
				// Handle matrix attribute (instanced)
				glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR);
				glVertexAttribPointer(TRANSFORM_ATTRIB_PTR,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(glm::mat4),
					(const void*)(attributeOffset));

				glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR+1);
				glVertexAttribPointer(TRANSFORM_ATTRIB_PTR+1,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(glm::mat4),
					(const void*)(attributeOffset + sizeof(glm::vec4)));

				glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR + 2);
				glVertexAttribPointer(TRANSFORM_ATTRIB_PTR + 2,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(glm::mat4),
					(const void*)(attributeOffset + (2 * sizeof(glm::vec4))));

				glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR + 3);
				glVertexAttribPointer(TRANSFORM_ATTRIB_PTR + 3,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(glm::mat4),
					(const void*)(attributeOffset + (3 * sizeof(glm::vec4))));
					
				glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR, 1);
				glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR+1, 1);
				glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR+2, 1);
				glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR+3, 1);
			} else if (el.name == "TANGENT") {
				glEnableVertexAttribArray(TANGENT_ATTRIB_PTR);
				glVertexAttribPointer(TANGENT_ATTRIB_PTR,
					size,
					(GLenum)el.componentType,
					GL_FALSE,
					stride,
					(const void*)(attributeOffset));
			} else if (el.name == "JOINTS_0") {
				glEnableVertexAttribArray(JOINTS_0_ATTRIB_PTR);
				glVertexAttribPointer(JOINTS_0_ATTRIB_PTR,
					size,
					(GLenum)el.componentType,
					GL_FALSE,
					stride,
					(const void*)(attributeOffset));
			} else if (el.name == "WEIGHTS_0") {
				glEnableVertexAttribArray(WEIGTHS_0_ATTRIB_PTR);
				glVertexAttribPointer(WEIGTHS_0_ATTRIB_PTR,
					size,
					(GLenum)el.componentType,
					GL_FALSE,
					stride,
					(const void*)(attributeOffset));
			}
			
			glBindVertexArray(0);
		}
	}

	void VertexArray::setBufferLayout(const BufferLayout& el)
	{
		m_buffer_layout = el;
		
		// If interleaved, ensure the vertex size is calculated
		if (m_buffer_layout.isInterleaved && m_buffer_layout.vertexSize == 0) {
			m_buffer_layout.calculateVertexSize();
		}
		
		// For each attribute, set up the layout
		for (int i = 0; i < el.buffer_attribs.size(); i++)
		{
			setAttribLayout(const_cast<BufferAttribute&>(el.buffer_attribs[i]));
		}
		
	}

	void VertexArray::setVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
	{
		m_vertexBuffer = vertexBuffer;
		
		// For DSA, we'll set up the binding in setAttribLayout since each attribute may have its own binding point
		if (GLCapabilities::hasDSA()) {
			// Initially bind with stride 0
			glVertexArrayVertexBuffer(m_rendererId, 0, vertexBuffer->getID(), 0, 0);
		}
	}

	void VertexArray::setIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
	{

		m_indexBuffer = indexBuffer;
		
		if (GLCapabilities::hasDSA()) {
			glVertexArrayElementBuffer(m_rendererId, indexBuffer->getID());
		} else {
			// For non-DSA, binding happens in the bind() method
			glBindVertexArray(m_rendererId);
			indexBuffer->bind();
			glBindVertexArray(0);
		}
	}

	// Legacy methods for compatibility
	void VertexArray::setVertexBuffer(std::vector<unsigned char>& vertices)
	{
        setVertexBuffer(std::make_shared<VertexBuffer>(vertices, BufferUsage::Static));
	}
	
	void VertexArray::setVertexBuffer(unsigned long long buffer_length)
	{
        setVertexBuffer(std::make_shared<VertexBuffer>(buffer_length, BufferUsage::Static));
	}

	void VertexArray::setIndexBuffer(unsigned long long buffer_length, unsigned int comp_count)
	{
		if (buffer_length == 0) return;
		std::shared_ptr<IndexBuffer> indexBuffer = std::make_shared<IndexBuffer>(buffer_length, comp_count, BufferUsage::Static);
        setIndexBuffer(indexBuffer);
	}
}

/*
* get all attribute buffer sizes
* 
* create buffers
* go over all primitives and store the data in the buffers
* 
* create vertex attrribute pointers
* 
* 
* 
*/