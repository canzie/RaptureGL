#include "VertexArray.h"
#include "glad/glad.h"
#include "BufferLayout.h"

#include "../logger/Log.h"

namespace Rapture {

	/*
	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Float:  return GL_FLOAT;
		case ShaderDataType::Float2: return GL_FLOAT;
		case ShaderDataType::Float3: return GL_FLOAT;
		case ShaderDataType::Float4: return GL_FLOAT;
		case ShaderDataType::Mat3:   return GL_FLOAT;
		case ShaderDataType::Mat4:   return GL_FLOAT;
		case ShaderDataType::Int:    return GL_INT;
		case ShaderDataType::Int2:   return GL_INT;
		case ShaderDataType::Int3:   return GL_INT;
		case ShaderDataType::Int4:   return GL_INT;
		case ShaderDataType::Bool:   return GL_BOOL;
		}
		return 1;
	}


	VertexArray::VertexArray()
	{
		glGenVertexArrays(1, &m_bufferID);

	}
	VertexArray::~VertexArray()
	{
		glDeleteVertexArrays(1, &m_bufferID);
	}
	void VertexArray::bind() const
	{
		//GE_CORE_CRITICAL("Binding VAO:{0}, VBO:{1}, IBO:{2}", m_bufferID, m_indexBuffer->getID__DEBUG(), m_vertexBuffer->getID__DEBUG());
		glBindVertexArray(m_bufferID);
		m_indexBuffer->bind();
		m_vertexBuffer->bind();
	}

	void VertexArray::unbind() const
	{
		glBindVertexArray(0);
	}

	void VertexArray::addVertexBuffer(std::shared_ptr<VertexBuffer>& vertexBuffer)
	{
		glBindVertexArray(m_bufferID);
		vertexBuffer->bind();

		unsigned int index = 0;
		const auto& layout = vertexBuffer->getLayout();
		for (const auto& element : layout)
		{
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type),
				element.isNormalized ? GL_TRUE : GL_FALSE,
				layout.GetStride(),
				(const void*)element.Offset);

			index++;
		}

		m_vertexBuffer = vertexBuffer;

		glBindVertexArray(0);

	}
	void VertexArray::setIndexBuffer(std::shared_ptr<IndexBuffer>& indexBuffer)
	{
		m_indexBuffer = indexBuffer;
	}
	*/



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
		glGenVertexArrays(1, &m_bufferID);
	}

	VertexArray::~VertexArray()
	{
		glDeleteVertexArrays(1, &m_bufferID);
	}
	void VertexArray::bind() const
	{
		glBindVertexArray(m_bufferID);
		if (m_indexBuffer != nullptr)
			m_indexBuffer->bind();
		if (m_vertexBuffer != nullptr)
			m_vertexBuffer->bind();
	}

	void VertexArray::unbind() const
	{
		glBindVertexArray(0);
	}

	void VertexArray::setAttribLayout(BufferAttribute& el)
	{

		GLuint size = 1;
		GLuint stride = 1;

		if (el.type == "SCALAR") size = 1;
		else if (el.type == "VEC2") size = 2;
		else if (el.type == "VEC3") size = 3;
		else if (el.type == "VEC4") size = 4;
		else if (el.type == "MAT4") size = 16;
		else GE_CORE_ERROR("Invalid Type: '{0}'", el.type);


		switch (el.componentType)
		{
		case GL_BYTE: case GL_UNSIGNED_BYTE: stride = 1; break;
		case GL_UNSIGNED_SHORT: case GL_SHORT: stride = 2; break;
		case GL_FLOAT: case GL_UNSIGNED_INT: stride = 4; break;
		}

		stride *= size;

		glBindVertexArray(m_bufferID);
		m_vertexBuffer->bind();

		if (el.name == "POSITION") {
			
			glEnableVertexAttribArray(POSITION_ATTRIB_PTR);
			glVertexAttribPointer(POSITION_ATTRIB_PTR,
				size,
				(GLenum)el.componentType, // componenttype
				GL_FALSE,
				stride,
				(const void*)el.offset);

		}else if (el.name == "NORMAL") {

			glEnableVertexAttribArray(NORMAL_ATTRIB_PTR);
			glVertexAttribPointer(NORMAL_ATTRIB_PTR,
				size, 
				(GLenum)el.componentType, // componenttype
				GL_FALSE,
				stride,
				(const void*)el.offset);

		}else if (el.name == "TEXCOORD_0") {

		glEnableVertexAttribArray(TEXCOORD_0_ATTRIB_PTR);
		glVertexAttribPointer(TEXCOORD_0_ATTRIB_PTR,
		size,
		(GLenum)el.componentType, // componenttype
		GL_FALSE,
		stride,
		(const void*)el.offset);

		}

		else if (el.name == "TRANSFORM_MAT") {
			glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR);
			glVertexAttribPointer(TRANSFORM_ATTRIB_PTR,
				4, 
				GL_FLOAT, // componenttype
				GL_FALSE,
				sizeof(glm::mat4),
				(const void*)el.offset);

			glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR+1);
			glVertexAttribPointer(TRANSFORM_ATTRIB_PTR+1,
				4, 
				GL_FLOAT, // componenttype
				GL_FALSE,
				sizeof(glm::mat4),
				(const void*)(el.offset + sizeof(glm::vec4)));

			glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR + 2);
			glVertexAttribPointer(TRANSFORM_ATTRIB_PTR + 2,
				4, 
				GL_FLOAT, // componenttype
				GL_FALSE,
				sizeof(glm::mat4),
				(const void*)(el.offset + (2 * sizeof(glm::vec4))));

			glEnableVertexAttribArray(TRANSFORM_ATTRIB_PTR + 3);
			glVertexAttribPointer(TRANSFORM_ATTRIB_PTR + 3,
				4, 
				GL_FLOAT, // componenttype
				GL_FALSE,
				sizeof(glm::mat4),
				(const void*)(el.offset + (3 * sizeof(glm::vec4))));
			glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR, 1);
			glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR+1, 1);
			glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR+2, 1);
			glVertexAttribDivisor(TRANSFORM_ATTRIB_PTR+3, 1);

		}

		glBindVertexArray(0);

	}

	void VertexArray::setBufferLayout(BufferLayout el)
	{
		m_buffer_layout = el;

		for (int i = 0; i < el.buffer_attribs.size(); i++)
		{
			setAttribLayout(el.buffer_attribs[i]);
		}

	}

	void VertexArray::setVertexBuffer(std::vector<unsigned char>& vertices)
	{
		m_vertexBuffer.reset(new VertexBuffer(vertices));
	}	
	
	void VertexArray::setVertexBuffer(unsigned long long buffer_length)
	{
		m_vertexBuffer.reset(new VertexBuffer(buffer_length));
	}

	void VertexArray::setIndexBuffer(unsigned long long buffer_length, unsigned int comp_count)
	{
		if (buffer_length == 0) return;
		m_indexBuffer.reset(new IndexBuffer(buffer_length, comp_count));
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