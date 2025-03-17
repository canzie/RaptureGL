#include "OpenGLShader.h"
#include "glad/glad.h"
#include <vector>
#include "../../Logger/Log.h"
#include "../OpenGLUniforms/UniformBindingPointIndices.h"

#include <fstream>

namespace Rapture {

	std::string parseShader(std::string filepath)
	{
		std::ifstream stream("C:/Users/vanma/source/repos/Rapture Game Engine/Rapture Game Engine/src/Shaders/GLSL/"+filepath);
		std::string ShaderString;

		if (!stream)
		{
			GE_CORE_ERROR("shader file parse error");
		}

		char c = stream.get();
		while (stream.good())
		{
			ShaderString.push_back(c);
			c = stream.get();
		};
		stream.close();

		return ShaderString;
	}

	OpenGLShader::OpenGLShader(std::string vertex_source, std::string fragment_source)
		: Shader(vertex_source, fragment_source)
	{
		m_vertex_source = parseShader(m_vertex_source);
		m_fragment_source = parseShader(m_fragment_source);
		initShader();

		GLint i;
		GLint count;

		const GLsizei bufSize = 32; // maximum name length
		GLchar name[bufSize]; // variable name in GLSL
		GLsizei length; // name length
		/*
		
		GLint size; // size of the variable
		GLenum type; // type of the variable (float, vec3 or mat4, etc)
		
		glGetProgramiv(m_shaderID, GL_ACTIVE_UNIFORMS, &count);

		for (i = 0; i < count; i++)
		{
			glGetActiveUniform(m_shaderID, (GLuint)i, bufSize, &length, &size, &type, name);
			GE_CORE_TRACE(name);
			if (name[1] == 'm') // only put material uniforms in the map (not 'r' render uniforms)
				m_uniforms[name] = NULL;
		}
		*/


		glGetProgramiv(m_shaderID, GL_ACTIVE_UNIFORM_BLOCKS, &count);
		for (i = 0; i < count; i++)
		{
			glGetActiveUniformBlockiv(m_shaderID, (GLuint)i, GL_UNIFORM_BLOCK_NAME_LENGTH, &length);
			glGetActiveUniformBlockName(m_shaderID, (GLuint)i, length, NULL, name);
			GE_CORE_TRACE("Uniform Block({0}) : {1}", i, name);

			std::string s(name);
			if (s == "BaseTransformMats")
				glUniformBlockBinding(m_shaderID, (GLuint)i, BASE_BINDING_POINT_IDX);
			else if (s == "Phong")
				glUniformBlockBinding(m_shaderID, (GLuint)i, PHONG_BINDING_POINT_IDX);
			else if (s == "PBR")
				glUniformBlockBinding(m_shaderID, (GLuint)i, PBR_BINDING_POINT_IDX);
			else if (s == "SOLID")
				glUniformBlockBinding(m_shaderID, (GLuint)i, SOLID_BINDING_POINT_IDX);
		}

		GE_CORE_TRACE("Created Shader: {0}", m_shaderID);

	}

	OpenGLShader::~OpenGLShader()
	{
		GE_CORE_TRACE("Deleting Shader: {0}", m_shaderID);
		glDeleteProgram(m_shaderID);
	}

	void OpenGLShader::bind()
	{
		glUseProgram(m_shaderID);

	}

	void OpenGLShader::unBind()
	{
		glUseProgram(0);
	}

	void OpenGLShader::setUniformMat4f(const std::string& name, glm::mat4& matrix)
	{
		
		int loc = glGetUniformLocation(m_shaderID, name.c_str());
		if (loc == -1)
		{
			GE_CORE_WARN("Uniform location not found, {0}", name);
			return;
		}

		glUniformMatrix4fv(loc, 1, GL_FALSE, &matrix[0][0]);
		
	}

	void OpenGLShader::setUniform1f(const std::string& name, float val)
	{
		int loc = glGetUniformLocation(m_shaderID, name.c_str());
		if (loc == -1)
		{
			GE_CORE_WARN("Uniform location not found, {0}", name);
			return;
		}

		glUniform1f(loc, val);
	}

	void OpenGLShader::setUniformVec3f(const std::string& name, glm::vec3& vector)
	{
		int loc = glGetUniformLocation(m_shaderID, name.c_str());
		if (loc == -1)
		{
			GE_CORE_WARN("Uniform location not found, {0}", name);
			return;
		}

		glUniform3fv(loc, 1, &vector[0]);
	}

	void OpenGLShader::initShader()
	{
		// Create an empty vertex shader handle
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const GLchar* source = (const GLchar*)m_vertex_source.c_str();
		glShaderSource(vertexShader, 1, &source, 0);

		// Compile the vertex shader
		glCompileShader(vertexShader);

		GLint isCompiled = 0;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(vertexShader);

			GE_CORE_CRITICAL("---Vertex Shader Error---");
			std::string s(infoLog.begin(), infoLog.end());
			GE_CORE_CRITICAL(s);

			return;
		}

		// Create an empty fragment shader handle
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

		// Send the fragment shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		source = (const GLchar*)m_fragment_source.c_str();
		glShaderSource(fragmentShader, 1, &source, 0);

		// Compile the fragment shader
		glCompileShader(fragmentShader);

		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(fragmentShader);
			// Either of them. Don't leak shaders.
			glDeleteShader(vertexShader);

			GE_CORE_CRITICAL("---Framgent Shader Error---");
			std::string s(infoLog.begin(), infoLog.end());
			GE_CORE_CRITICAL(s);

			return;
		}

		// Vertex and fragment shaders are successfully compiled.
		// Now time to link them together into a program.
		// Get a program object.
		m_shaderID = glCreateProgram();

		// Attach our shaders to our program
		glAttachShader(m_shaderID, vertexShader);
		glAttachShader(m_shaderID, fragmentShader);

		// Link our program
		glLinkProgram(m_shaderID);

		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint isLinked = 0;
		glGetProgramiv(m_shaderID, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(m_shaderID, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(m_shaderID, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(m_shaderID);
			// Don't leak shaders either.
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);

			GE_CORE_CRITICAL("---Shader Linking Error---");
			std::string s(infoLog.begin(), infoLog.end());
			GE_CORE_CRITICAL(s);

			return;
		}

		// Always detach shaders after a successful link.
		glDetachShader(m_shaderID, vertexShader);
		glDetachShader(m_shaderID, fragmentShader);
	}

}

