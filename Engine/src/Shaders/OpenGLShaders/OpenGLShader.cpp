#include "OpenGLShader.h"
#include "glad/glad.h"
#include <vector>
#include "../../Logger/Log.h"
#include "../OpenGLUniforms/UniformBindingPointIndices.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <fstream>

namespace Rapture {

// Shader directory path - externally accessible
std::string s_ShaderDirectory = "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL/";


// Forward declarations for conversion functions
GLenum ShaderTypeToGL(ShaderType type);
GLenum UniformTypeToGL(UniformType type);
UniformType GLToUniformType(GLenum type);

std::string parseShader(std::string filepath)
{

	std::string fullPath = s_ShaderDirectory + filepath;
	std::ifstream stream(fullPath);
	std::string ShaderString;

		if (!stream)
		{
			GE_CORE_ERROR("Shader file parse error: {0}", fullPath);
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
    : Shader(vertex_source + "_" + fragment_source), m_status(ShaderStatus::UNCOMPILED)
{
    std::string vertexShaderSource = parseShader(vertex_source);
    if (vertexShaderSource.empty()) {
        GE_CORE_CRITICAL("Vertex shader source is empty after parsing: {0}", vertex_source);
        m_status = ShaderStatus::SHADER_ERROR;
        return;
    }
    m_sources[ShaderType::VERTEX] = vertexShaderSource;

        std::string fragmentShaderSource = parseShader(fragment_source);
        if (fragmentShaderSource.empty()) {
            m_status = ShaderStatus::SHADER_ERROR;
            GE_CORE_CRITICAL("Fragment shader source is empty after parsing: {0}", fragment_source);
            return;
        }
        m_sources[ShaderType::FRAGMENT] = fragmentShaderSource;

	compile();

		GLint i;
		GLint count;

		const GLsizei bufSize = 32; // maximum name length
		GLchar name[bufSize]; // variable name in GLSL
		GLsizei length; // name length

	glGetProgramiv(m_programID, GL_ACTIVE_UNIFORM_BLOCKS, &count);
	for (i = 0; i < count; i++)
	{
		glGetActiveUniformBlockiv(m_programID, (GLuint)i, GL_UNIFORM_BLOCK_NAME_LENGTH, &length);
		glGetActiveUniformBlockName(m_programID, (GLuint)i, length, NULL, name);
		GE_CORE_TRACE("Uniform Block({0}) : {1}", i, name);

			std::string s(name);
			if (s == "BaseTransformMats")
				glUniformBlockBinding(m_programID, (GLuint)i, BASE_BINDING_POINT_IDX);
			else if (s == "Phong")
				glUniformBlockBinding(m_programID, (GLuint)i, PHONG_BINDING_POINT_IDX);
			else if (s == "PBR")
				glUniformBlockBinding(m_programID, (GLuint)i, PBR_BINDING_POINT_IDX);
			else if (s == "SOLID")
				glUniformBlockBinding(m_programID, (GLuint)i, SOLID_BINDING_POINT_IDX);
		}

		GE_CORE_TRACE("Created Shader: {0}", m_programID);

	}

	OpenGLShader::~OpenGLShader()
	{
		GE_CORE_TRACE("Deleting Shader: {0}", m_programID);
		glDeleteProgram(m_programID);
	}

    bool OpenGLShader::compile() {
        for (auto& [type, source] : m_sources) {
            //std::string processed = OpenGLShaderCompiler::preprocessShader(source);
            std::string processed = source;

            // 2. Compile individual shaders
            if (!compileShader(type, processed)) {
                GE_CORE_CRITICAL("Shader compilation failed");
                return false;
            }
        }

        if (!linkProgram()) {
            GE_CORE_CRITICAL("Shader linking failed");
            return false;
        }

        reflectUniforms();
        m_status = ShaderStatus::COMPILED;
        return true;
    }

void OpenGLShader::bind()
{
	glUseProgram(m_programID);
}

	void OpenGLShader::unBind()
	{
		glUseProgram(0);
	}

    
	void OpenGLShader::setUniformMat4f(const std::string& name, glm::mat4& matrix)
	{
		
		int loc = glGetUniformLocation(m_programID, name.c_str());
		if (loc == -1)
		{
			GE_CORE_WARN("Uniform location not found, {0}", name);
			return;
		}

		glUniformMatrix4fv(loc, 1, GL_FALSE, &matrix[0][0]);
		
	}

	void OpenGLShader::setUniform1f(const std::string& name, float val)
	{
		int loc = glGetUniformLocation(m_programID, name.c_str());
		if (loc == -1)
		{
			GE_CORE_WARN("Uniform location not found, {0}", name);
			return;
		}

		glUniform1f(loc, val);
	}

	void OpenGLShader::setUniformVec3f(const std::string& name, glm::vec3& vector)
	{
		int loc = glGetUniformLocation(m_programID, name.c_str());
		if (loc == -1)
		{
			GE_CORE_WARN("Uniform location not found, {0}", name);
			return;
		}

		glUniform3fv(loc, 1, &vector[0]);
	}


	void OpenGLShader::setUniform(const std::string& name, const void* data)
	{
		if (!data) return;
		
		// Get uniform location
		GLint location = glGetUniformLocation(m_programID, name.c_str());
		if (location == -1) {
			GE_CORE_WARN("Uniform '{0}' not found in shader", name);
			return;
		}
		
		// Get uniform type from reflection data or assume based on parameter
		// For simplicity, we'll use RTTI or parameter type hints in a real implementation
		// Here we'll just demonstrate for float, vec3, and mat4 as examples
		
		// Determine type from uniform name convention (simple approach)
		if (name.find("mat4") != std::string::npos) {
			glUniformMatrix4fv(location, 1, GL_FALSE, static_cast<const float*>(data));
		}
		else if (name.find("vec3") != std::string::npos) {
			glUniform3fv(location, 1, static_cast<const float*>(data));
		}
		else {
			// Default to float
			glUniform1f(location, *static_cast<const float*>(data));
		}
	}

	void OpenGLShader::setUniformArray(const std::string& name, const void* data, size_t count)
	{
		if (!data || count == 0) return;
		
		// Get uniform location
		GLint location = glGetUniformLocation(m_programID, name.c_str());
		if (location == -1) {
			GE_CORE_WARN("Uniform array '{0}' not found in shader", name);
			return;
		}
		
		// Determine type from uniform name convention (simple approach)
		if (name.find("mat4") != std::string::npos) {
			glUniformMatrix4fv(location, count, GL_FALSE, static_cast<const float*>(data));
		}
		else if (name.find("vec3") != std::string::npos) {
			glUniform3fv(location, count, static_cast<const float*>(data));
		}
		else {
			// Default to float
			glUniform1fv(location, count, static_cast<const float*>(data));
		}
	}

	bool OpenGLShader::reload()
	{
		// Get shader sources from files again
		// This assumes shader sources are stored in files and parsed via parseShader
		
		// Delete existing program
		if (m_programID) {
			glDeleteProgram(m_programID);
		}
		
		// Delete existing shaders
		for (auto shaderID : m_shaderIDs) {
			glDeleteShader(shaderID);
		}
		m_shaderIDs.clear();
		
		// Recompile shaders
		return compile();
	}

	void OpenGLShader::addVariant(const ShaderVariant& variant)
	{
		// Store the variant for future use
		// This is a simplified implementation that doesn't modify the shader
		// In a full implementation, we would apply the variant's defines to shader source
		GE_CORE_INFO("Added shader variant '{0}'", variant.name);
	}

	void OpenGLShader::removeVariant(const std::string& name)
	{
		// Remove the variant from storage
		// This is a simplified implementation
		GE_CORE_INFO("Removed shader variant '{0}'", name);
	}

	const std::vector<UniformInfo>& OpenGLShader::getUniforms() const
	{
		return m_uniforms;
	}

	const std::vector<UniformInfo>& OpenGLShader::getSamplers() const
	{
		return m_samplers;
	}

	std::shared_ptr<Shader> OpenGLShader::loadFromCache(const std::string& name)
	{
		// Simple implementation that returns null
		// In a real implementation, we would load shader from cache file
		GE_CORE_INFO("Loading shader '{0}' from cache (not implemented)", name);
		return nullptr;
	}

	void OpenGLShader::saveToCache() const
	{
		// Simple implementation that does nothing
		// In a real implementation, we would save shader to cache file
		GE_CORE_INFO("Saving shader to cache (not implemented)");
	}

    bool OpenGLShader::compileShader(ShaderType type, const std::string& processed_source) {
        // Create an empty vertex shader handle
		GLuint shaderID = glCreateShader(ShaderTypeToGL(type));

		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const GLchar* source = (const GLchar*)processed_source.c_str();
		glShaderSource(shaderID, 1, &source, 0);

		// Compile the vertex shader
		glCompileShader(shaderID);

		GLint isCompiled = 0;
		glGetShaderiv(shaderID, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(shaderID, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(shaderID);

			GE_CORE_CRITICAL("---Shader Compilation Error---");
			std::string s(infoLog.begin(), infoLog.end());
			GE_CORE_CRITICAL(s);

			return false;
		}

    m_shaderIDs.push_back(shaderID);
    return true;
}

bool OpenGLShader::linkProgram() {
	// Get a program object.
	m_programID = glCreateProgram();

		// Attach our shaders to our program
        for (auto& shaderID : m_shaderIDs) {
            glAttachShader(m_programID, shaderID);
        }

		// Link our program
		glLinkProgram(m_programID);

		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint isLinked = 0;
		glGetProgramiv(m_programID, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(m_programID, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(m_programID, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(m_programID);
			// Don't leak shaders either.
			for (auto& shaderID : m_shaderIDs) {
				glDeleteShader(shaderID);
			}

			GE_CORE_CRITICAL("---Shader Linking Error---");
			std::string s(infoLog.begin(), infoLog.end());
			GE_CORE_CRITICAL(s);

			return false;
		}

		// Always detach shaders after a successful link.
		for (auto& shaderID : m_shaderIDs) {
			glDetachShader(m_programID, shaderID);
		}

		return true;
    }

    void OpenGLShader::reflectUniforms() {
        // Get number of uniforms
        GLint numUniforms;
        glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &numUniforms);
        
        // Get uniform information
        for (GLint i = 0; i < numUniforms; i++) {
            GLchar name[256];
            GLsizei length;
            GLint size;
            GLenum type;
            
            glGetActiveUniform(m_programID, i, sizeof(name), &length, &size, &type, name);
            
            UniformInfo info;
            info.name = std::string(name);
            info.type = GLToUniformType(type);
            info.location = glGetUniformLocation(m_programID, name);
            info.size = size;
            
            m_uniforms.push_back(info);
        }
    }

GLenum ShaderTypeToGL(ShaderType type)
{
	switch (type)
	{
		case ShaderType::VERTEX: return GL_VERTEX_SHADER;
		case ShaderType::FRAGMENT: return GL_FRAGMENT_SHADER;
		case ShaderType::GEOMETRY: return GL_GEOMETRY_SHADER;
		case ShaderType::COMPUTE: return GL_COMPUTE_SHADER;
		case ShaderType::TESS_CONTROL: return GL_TESS_CONTROL_SHADER;
		case ShaderType::TESS_EVAL: return GL_TESS_EVALUATION_SHADER;
		default: return 0;
	}
};

    GLenum UniformTypeToGL(UniformType type)
    {
        switch (type)
        {
            case UniformType::FLOAT: return GL_FLOAT;
            case UniformType::INT: return GL_INT;
            case UniformType::UINT: return GL_UNSIGNED_INT;
            case UniformType::BOOL: return GL_BOOL;
            case UniformType::VEC2: return GL_FLOAT_VEC2;
            case UniformType::VEC3: return GL_FLOAT_VEC3;
            case UniformType::VEC4: return GL_FLOAT_VEC4;
            case UniformType::MAT3: return GL_FLOAT_MAT3;
            case UniformType::MAT4: return GL_FLOAT_MAT4;
            case UniformType::SAMPLER_2D: return GL_SAMPLER_2D;
            case UniformType::SAMPLER_CUBE: return GL_SAMPLER_CUBE;
            default: return 0;
        }
    }

UniformType GLToUniformType(GLenum type)
{
	switch (type)
	{
		case GL_FLOAT: return UniformType::FLOAT;
		case GL_INT: return UniformType::INT;
		case GL_UNSIGNED_INT: return UniformType::UINT;
		case GL_BOOL: return UniformType::BOOL;
		case GL_FLOAT_VEC2: return UniformType::VEC2;
		case GL_FLOAT_VEC3: return UniformType::VEC3;
		case GL_FLOAT_VEC4: return UniformType::VEC4;
		case GL_FLOAT_MAT3: return UniformType::MAT3;
		case GL_FLOAT_MAT4: return UniformType::MAT4;
		case GL_SAMPLER_2D: return UniformType::SAMPLER_2D;
		case GL_SAMPLER_CUBE: return UniformType::SAMPLER_CUBE;
		default: return UniformType::FLOAT;
	}
}

}


