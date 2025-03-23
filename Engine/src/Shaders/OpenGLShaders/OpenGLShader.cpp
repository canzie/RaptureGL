#include "OpenGLShader.h"
#include "glad/glad.h"
#include <vector>
#include "../../Logger/Log.h"
#include "../OpenGLUniforms/UniformBindingPointIndices.h"

#include "../../Textures/Texture.h"
#include <fstream>
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

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
			GE_CORE_ERROR("ShaderParser: Shader file parse error: {0}", fullPath);
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
        GE_CORE_CRITICAL("OpenGLShader: Vertex shader source is empty after parsing: {0}", vertex_source);
        m_status = ShaderStatus::SHADER_ERROR;
        return;
    }
    m_sources[ShaderType::VERTEX] = vertexShaderSource;

        std::string fragmentShaderSource = parseShader(fragment_source);
        if (fragmentShaderSource.empty()) {
            m_status = ShaderStatus::SHADER_ERROR;
            GE_CORE_CRITICAL("OpenGLShader: Fragment shader source is empty after parsing: {0}", fragment_source);
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
	GE_CORE_INFO("OpenGLShader: '{0}' has {1} uniform blocks", m_name, count);
	
	for (i = 0; i < count; i++)
	{
		GLint blockSize = 0;
		glGetActiveUniformBlockiv(m_programID, (GLuint)i, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
		glGetActiveUniformBlockiv(m_programID, (GLuint)i, GL_UNIFORM_BLOCK_NAME_LENGTH, &length);
		glGetActiveUniformBlockName(m_programID, (GLuint)i, length, NULL, name);
		GE_CORE_INFO("OpenGLShader: Uniform Block({0}): '{1}' (size: {2} bytes)", i, name, blockSize);

		std::string blockName(name);
		GLuint bindingPoint = 0;
		
		// Map block names to binding points
		if (blockName == "BaseTransformMats") {
			bindingPoint = BASE_BINDING_POINT_IDX;
		}
		else if (blockName == "PBR") {
			bindingPoint = PBR_BINDING_POINT_IDX;
		}
		else if (blockName == "Phong") {
			bindingPoint = PHONG_BINDING_POINT_IDX;
		}
		else if (blockName == "SOLID") {
			bindingPoint = SOLID_BINDING_POINT_IDX;
		}
		else if (blockName == "SpecularGlossiness") {
			bindingPoint = SPECULAR_GLOSSINESS_BINDING_POINT_IDX;
		}
		else {
			GE_CORE_WARN("OpenGLShader: Unknown uniform block '{0}'", blockName);
            continue;
		}
		
		// Get the block index and current binding
		GLuint blockIndex = glGetUniformBlockIndex(m_programID, name);
		GLint currentBinding = 0;
		glGetActiveUniformBlockiv(m_programID, blockIndex, GL_UNIFORM_BLOCK_BINDING, &currentBinding);
		
		// Set the binding point if it's different from current
		if (currentBinding != bindingPoint) {
			glUniformBlockBinding(m_programID, blockIndex, bindingPoint);
			GE_CORE_INFO("OpenGLShader: Bound block '{0}' to binding point {1} (was {2})", 
				blockName, bindingPoint, currentBinding);
		}
		else {
			GE_CORE_INFO("OpenGLShader: Block '{0}' already bound to point {1}", blockName, bindingPoint);
		}
		
		// Validate that binding worked
		glGetActiveUniformBlockiv(m_programID, blockIndex, GL_UNIFORM_BLOCK_BINDING, &currentBinding);
		if (currentBinding != bindingPoint) {
			GE_CORE_ERROR("OpenGLShader: FAILED to bind block '{0}' to point {1}, still at {2}", 
				blockName, bindingPoint, currentBinding);
		}
	}

	GE_CORE_INFO("OpenGLShader: Created Shader: {0}", m_programID);

	}

OpenGLShader::~OpenGLShader()
	{
		GE_CORE_TRACE("OpenGLShader: Deleting Shader: {0}", m_programID);
		glDeleteProgram(m_programID);
	}

bool OpenGLShader::compile(const std::string& variantName) {

    const ShaderVariant* variant = nullptr;
    
    if (!variantName.empty()) {

        // Find the requested variant
        for (const auto& v : m_variants) {
            if (v.name == variantName) {
                variant = &v;
                break;
            }
        }
        
        if (!variant) {
            GE_CORE_ERROR("OpenGLShader::compileVariant: Variant '{0}' not found, compiling default variant", variantName);
        }
        
        GE_CORE_INFO("OpenGLShader::compile: Compiling shader variant '{0}' for shader '{1}'", variantName, m_name);
    }

        for (auto& [type, source] : m_sources) {
            std::string processed = source;
            
            if (variant) {
                processed = processSource(source, *variant);
            }
    
            //std::string processed = source;

            // 2. Compile individual shaders
            if (!compileShader(type, processed)) {
                GE_CORE_CRITICAL("OpenGLShader::compile: Shader compilation failed");
                return false;
            }
        }

        if (!linkProgram()) {
            GE_CORE_CRITICAL("OpenGLShader::compile: Shader linking failed");
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

// deprecated
void OpenGLShader::setUniformMat4f(const std::string& name, glm::mat4& matrix)
{
    setMat4(name, matrix);
}

// deprecated
void OpenGLShader::setUniform1f(const std::string& name, float val)
	{
        setFloat(name, val);
	}

// deprecated
void OpenGLShader::setUniformVec3f(const std::string& name, glm::vec3& vector)
{
    setVec3(name, vector);
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
        // Check if variant with this name already exists
        for (const auto& existingVariant : m_variants) {
            if (existingVariant.name == variant.name) {
                GE_CORE_WARN("OpenGLShader::addVariant: Variant '{0}' already exists. Overwriting.", variant.name);
                return;
            }
        }
        
        GE_CORE_INFO("OpenGLShader::addVariant: Added shader variant '{0}' to shader '{1}'", variant.name, m_name);
        m_variants.push_back(variant);
    }

    void OpenGLShader::removeVariant(const std::string& name)
    {
        for (auto it = m_variants.begin(); it != m_variants.end(); ++it) {
            if (it->name == name) {
                GE_CORE_INFO("OpenGLShader::removeVariant: Removed shader variant '{0}' from shader '{1}'", name, m_name);
                m_variants.erase(it);
                return;
            }
        }
        
        GE_CORE_WARN("OpenGLShader::removeVariant: Variant '{0}' not found", name);
    }

    std::string OpenGLShader::processSource(const std::string& source, const ShaderVariant& variant)
    {
        // Simple preprocessor for shader variants
        std::stringstream ss;
        
        // Add defines for this variant
        for (const auto& define : variant.defines) {
            ss << "#define " << define << std::endl;
        }
        
        // Add the original source
        ss << source;
        
        return ss.str();
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
		GE_CORE_INFO("OpenGLShader::loadFromCache: Loading shader '{0}' from cache (not implemented)", name);
		return nullptr;
	}

	void OpenGLShader::saveToCache() const
	{
		// Simple implementation that does nothing
		// In a real implementation, we would save shader to cache file
		GE_CORE_INFO("OpenGLShader::saveToCache: Saving shader to cache (not implemented)");
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

			GE_CORE_CRITICAL("OpenGLShader::compileShader: ---Shader Compilation Error---");
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

			GE_CORE_CRITICAL("OpenGLShader::linkProgram: ---Shader Linking Error---");
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

void OpenGLShader::validateShaderProgram()
{
    glValidateProgram(m_programID);
    GLint status;
    glGetProgramiv(m_programID, GL_VALIDATE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(m_programID, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetProgramInfoLog(m_programID, length, &length, log.data());
        GE_CORE_ERROR("OpenGLShader::validateShaderProgram: Program validation failed: {0}", log.data());
    }
}


void OpenGLShader::setFloat(const std::string& name, float value)
{
    glUniform1f(getUniformLocation(name), value);
}

void OpenGLShader::setInt(const std::string& name, int value)
{
    glUniform1i(getUniformLocation(name), value);
}

void OpenGLShader::setBool(const std::string& name, bool value)
{
    glUniform1i(getUniformLocation(name), static_cast<int>(value));
}

void OpenGLShader::setVec2(const std::string& name, const glm::vec2& value)
{
    glUniform2f(getUniformLocation(name), value.x, value.y);
}

void OpenGLShader::setVec3(const std::string& name, const glm::vec3& value)
{
    glUniform3f(getUniformLocation(name), value.x, value.y, value.z);
}


void OpenGLShader::setVec4(const std::string& name, const glm::vec4& value)
{
    glUniform4f(getUniformLocation(name), value.x, value.y, value.z, value.w);
}

void OpenGLShader::setMat3(const std::string& name, const glm::mat3& value)
{
    glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, &value[0][0]);
}

void OpenGLShader::setMat4(const std::string& name, const glm::mat4& value)
{
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &value[0][0]);
    
}


void OpenGLShader::setTexture(const std::string& name, std::shared_ptr<Texture2D> texture, uint32_t slot)
{
    if (!texture) {
        GE_CORE_WARN("OpenGLShader::setTexture: Texture is null for uniform '{0}'", name);
        return;
    }
    
    if (slot >= 32) {
        GE_CORE_ERROR("OpenGLShader::setTexture: Texture slot {0} out of range (max 31)", slot);
        return;
    }
    
    glActiveTexture(GL_TEXTURE0 + slot);
    texture->bind();
    setInt(name, slot);
    m_textureSlots[slot] = texture->getRendererID();
}

int OpenGLShader::getUniformLocation(const std::string& name)
{
    // Check if the uniform location is already cached
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end()) {
        return it->second;
    }
    
    // If not cached, query OpenGL for the location
    int location = glGetUniformLocation(m_programID, name.c_str());
    if (location == -1) {
        GE_CORE_WARN("OpenGLShader::getUniformLocation: Uniform '{0}' not found in shader '{1}'", name, m_name);
    }
    
    // Cache the location for future use
    m_uniformLocationCache[name] = location;
    return location;
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


