#include "OpenGLShader.h"
#include "glad/glad.h"
#include <vector>
#include <filesystem>
#include "../../Logger/Log.h"
#include "../OpenGLUniforms/UniformBindingPointIndices.h"

#include "../../Textures/Texture.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "../../Debug/TracyProfiler.h"

namespace Rapture {

// Forward declarations for conversion functions
GLenum ShaderTypeToGL(ShaderType type);
GLenum UniformTypeToGL(UniformType type);
UniformType GLToUniformType(GLenum type);

// Forward declaration for recursive include processing
static std::string processIncludesInString(
    const std::string& sourceCode, 
    const std::filesystem::path& currentFileCanonicalPath,
    std::set<std::filesystem::path>& visitedFiles
);

// Helper to read a single file's content
static std::string readFileContent(const std::filesystem::path& filepath, bool isTopLevelForLogging) {
    std::ifstream stream(filepath);
    if (!stream) {
        if (!isTopLevelForLogging) {
            GE_CORE_ERROR("OpenGLShader: Failed to open include file: {}", filepath.string());
            return "// Error: Failed to open include file " + filepath.string() + "\n";
        }
        GE_CORE_ERROR("OpenGLShader: Failed to open shader file: {}", filepath.string());
        return "";
    }
    
    std::stringstream ss;
    ss << stream.rdbuf();
    stream.close();
    
    std::string content = ss.str();

    // Normalize line endings: remove all carriage returns (\r)
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

    // Strip UTF-8 BOM if present
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
        // GE_CORE_TRACE("OpenGLShader: Stripped UTF-8 BOM from: {}", filepath.string()); // Optional: log BOM stripping
    }

    if (content.empty()) {
         const char* fileType = isTopLevelForLogging ? "Shader" : "Included shader";
         GE_CORE_WARN("OpenGLShader: {} file is empty: {}", fileType, filepath.string());
    }

    return content;
}

// Processes #include directives in a shader source string
static std::string processIncludesInString(
    const std::string& sourceCode, 
    const std::filesystem::path& currentFileCanonicalPath,
    std::set<std::filesystem::path>& visitedFiles
) {
    std::string processedOutput;
    std::regex includeRegex(R"~(^\s*#include\s+"([^"]*\.glsl)"\s*$)~");
    std::istringstream contentStream(sourceCode);
    std::string line;

    while (std::getline(contentStream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, includeRegex)) {
            std::string includedFilename = match[1].str();
            std::filesystem::path includedRelativePath = currentFileCanonicalPath.parent_path() / includedFilename;
            
            std::error_code ec;
            std::filesystem::path nextFileCanonicalPath = std::filesystem::canonical(includedRelativePath, ec);

            if (ec) {
                GE_CORE_ERROR("OpenGLShader: Failed to find or access include file {}: {}. Referenced from: {}", 
                              includedRelativePath.string(), ec.message(), currentFileCanonicalPath.string());
                processedOutput += "// Error: Could not resolve or access include file " + includedRelativePath.string() + "\n";
                continue;
            }

            if (visitedFiles.count(nextFileCanonicalPath)) {
                GE_CORE_WARN("OpenGLShader: Circular include detected: {} already being processed. Referenced from: {}", 
                             nextFileCanonicalPath.string(), currentFileCanonicalPath.string());
                processedOutput += "// Error: Circular include detected for " + nextFileCanonicalPath.string() + "\n";
                continue;
            }
            
            visitedFiles.insert(nextFileCanonicalPath);
            
            std::string includedRawContent = readFileContent(nextFileCanonicalPath, false /* isTopLevelForLogging */);
            std::string fullyProcessedIncludedContent = processIncludesInString(includedRawContent, nextFileCanonicalPath, visitedFiles);
            
            processedOutput += fullyProcessedIncludedContent;
            if (!fullyProcessedIncludedContent.empty() && fullyProcessedIncludedContent.back() != '\n') {
                processedOutput += '\n';
            }
            
            visitedFiles.erase(nextFileCanonicalPath);

        } else {
            processedOutput += line + '\n';
        }
    }

    if (!sourceCode.empty() && sourceCode.back() != '\n' && !processedOutput.empty() && processedOutput.back() == '\n') {
        processedOutput.pop_back();
    }
    
    return processedOutput;
}

// Helper function to read shader source from a file path, processing includes
std::string readShaderSource(const std::filesystem::path& filepath) {
    std::error_code ec;
    std::filesystem::path canonicalTopLevelPath = std::filesystem::canonical(filepath, ec);
    if (ec) {
        GE_CORE_ERROR("OpenGLShader: Failed to find or access shader file: {}. Error: {}", filepath.string(), ec.message());
        return "";
    }

    std::string initialContent = readFileContent(canonicalTopLevelPath, true /* isTopLevelForLogging */);
    
    // If initialContent is empty (read error or empty file), readFileContent already logged.
    // Proceed with processing, as an empty file might just wrap includes.
    // The constructor handles empty strings from readShaderSource appropriately.

    std::set<std::filesystem::path> visitedFiles;
    visitedFiles.insert(canonicalTopLevelPath); 
    std::string result = processIncludesInString(initialContent, canonicalTopLevelPath, visitedFiles);
    // The top-level file is effectively "unvisited" when its processing is complete.
    // No explicit erase here as visitedFiles is local to this call of readShaderSource for the top-level.
    // The recursive calls manage their entries in the passed-around visitedFiles set.
    
    return result;
}

OpenGLShader::OpenGLShader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath) 
    : Shader(vertexPath.stem().string())
{
    GE_CORE_TRACE("OpenGLShader: Creating shader from {} and {}", vertexPath.string(), fragmentPath.string());

    std::string vertexShaderSource = readShaderSource(vertexPath);
    if (vertexShaderSource.empty()) {
        GE_CORE_CRITICAL("OpenGLShader: Vertex shader source is empty or failed to read: {}", vertexPath.string());
        m_status = ShaderStatus::SHADER_ERROR;
        return;
    }
    m_sources[ShaderType::VERTEX] = vertexShaderSource;

    std::string fragmentShaderSource = readShaderSource(fragmentPath);
    if (fragmentShaderSource.empty()) {
        m_status = ShaderStatus::SHADER_ERROR;
        GE_CORE_CRITICAL("OpenGLShader: Fragment shader source is empty or failed to read: {}", fragmentPath.string());
        return;
    }
    m_sources[ShaderType::FRAGMENT] = fragmentShaderSource;

    if (!geometryPath.empty()) {
        std::string geometryShaderSource = readShaderSource(geometryPath);
        if (geometryShaderSource.empty()) {
            m_status = ShaderStatus::SHADER_ERROR;
            GE_CORE_CRITICAL("OpenGLShader: Geometry shader source failed to read: {}", geometryPath.string());
            return;
        }
        m_sources[ShaderType::GEOMETRY] = geometryShaderSource;
    }

     if (!compile()) {
        GE_CORE_ERROR("OpenGLShader: Failed to compile/link shader program derived from {} and {}", vertexPath.string(), fragmentPath.string());
        return;
     }


    if (m_status == ShaderStatus::SHADER_ERROR) {
        GE_CORE_ERROR("OpenGLShader: Failed to compile/link shader program derived from {} and {}", vertexPath.string(), fragmentPath.string());
        // Cleanup potentially created program ID if linking failed after compilation succeeded partially
        if (m_programID != 0) {
            glDeleteProgram(m_programID);
            m_programID = 0;
        }
        return; // Don't proceed to uniform block binding if compilation/linking failed
    }

    // Rest of the constructor (uniform block binding)
    GLint i;
    GLint count;

    const GLsizei bufSize = 32; // maximum name length
    GLchar name[bufSize]; // variable name in GLSL
    GLsizei length; // name length

    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORM_BLOCKS, &count);
   // GE_CORE_TRACE("OpenGLShader: Found {} active uniform blocks.", count);

    for (i = 0; i < count; i++)
    {
        GLint blockSize = 0;
        glGetActiveUniformBlockiv(m_programID, (GLuint)i, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
        glGetActiveUniformBlockiv(m_programID, (GLuint)i, GL_UNIFORM_BLOCK_NAME_LENGTH, &length);
        glGetActiveUniformBlockName(m_programID, (GLuint)i, bufSize, &length, name);

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
        else if (blockName == "Lights") {
            bindingPoint = LIGHTS_BINDING_POINT_IDX;
        }
        else if (blockName == "BoneMatrices") {
            bindingPoint = BONE_MATRICES_BINDING_POINT_IDX;
        }
        else if (blockName == "Camera") {
            bindingPoint = CAMERA_BINDING_POINT_IDX;
        } 
        else if (blockName == "ShadowMatrices") {
            bindingPoint = SHADOW_MATRICES_BINDING_POINT_IDX;
        }
        else {
            GE_CORE_WARN("OpenGLShader: Unknown uniform block '{}' found in shader '{}'", blockName, m_name);
            continue;
        }
        
        // Get the block index and current binding
        GLuint blockIndex = glGetUniformBlockIndex(m_programID, name);
        if (blockIndex == GL_INVALID_INDEX) {
            GE_CORE_WARN("OpenGLShader: Could not get index for uniform block '{}' in shader '{}'", blockName, m_name);
            continue;
        }

        GLint currentBinding = 0;
        glGetActiveUniformBlockiv(m_programID, blockIndex, GL_UNIFORM_BLOCK_BINDING, &currentBinding);
        
        // Set the binding point if it's different from current
        if (currentBinding != bindingPoint) {
            glUniformBlockBinding(m_programID, blockIndex, bindingPoint);
            //GE_CORE_TRACE("OpenGLShader: Binding uniform block '{}' to point {}", blockName, bindingPoint);

            // Validate that binding worked
            glGetActiveUniformBlockiv(m_programID, blockIndex, GL_UNIFORM_BLOCK_BINDING, &currentBinding);
            if (currentBinding != bindingPoint) {
                GE_CORE_ERROR("OpenGLShader: FAILED to bind block '{}' to point {}, still at {}", 
                    blockName, bindingPoint, currentBinding);
            }
        } else {
            //GE_CORE_TRACE("OpenGLShader: Uniform block '{}' already bound to point {}", blockName, bindingPoint);
        }
    }

    GE_CORE_INFO("OpenGLShader: Successfully created Shader: {} (Program ID: {})", m_name, m_programID);
}

OpenGLShader::OpenGLShader(const std::filesystem::path &computePath)
    : Shader(computePath.stem().string())
{


    if (!computePath.empty()) {
        std::string computeShaderSource = readShaderSource(computePath);
        if (computeShaderSource.empty()) {
            m_status = ShaderStatus::SHADER_ERROR;
            GE_CORE_CRITICAL("OpenGLShader: Compute shader source failed to read: {}", computePath.string());
            return;
        }
        m_sources[ShaderType::COMPUTE] = computeShaderSource;
    }

     if (!compile()) {
        GE_CORE_ERROR("OpenGLShader: Failed to compile/link shader program derived from {}", computePath.string());
        return;
     }


    if (m_status == ShaderStatus::SHADER_ERROR) {
        GE_CORE_ERROR("OpenGLShader: Failed to compile/link shader program derived from {}", computePath.string());
        // Cleanup potentially created program ID if linking failed after compilation succeeded partially
        if (m_programID != 0) {
            glDeleteProgram(m_programID);
            m_programID = 0;
        }
        return; 
    }



    GE_CORE_INFO("OpenGLShader: Compute Shader successfully created Shader: {} (Program ID: {})", m_name, m_programID);

}

OpenGLShader::~OpenGLShader()
	{
		GE_CORE_TRACE("OpenGLShader: Deleting Shader: {0}", m_programID);
		glDeleteProgram(m_programID);
	}

bool OpenGLShader::compile(const std::string& variantName) {
    RAPTURE_PROFILE_FUNCTION();

    // Clean up previous compilation artifacts if any
    if (m_programID) {
        glDeleteProgram(m_programID);
        m_programID = 0;
    }
    for (auto shaderID : m_shaderIDs) {
        glDeleteShader(shaderID);
    }
    m_shaderIDs.clear();
    m_uniformLocationCache.clear(); // Also clear uniform location cache
    m_uniforms.clear(); // Clear reflected uniforms
    m_samplers.clear(); // Clear reflected samplers

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
    RAPTURE_PROFILE_SCOPE("Shader Bind");
    glUseProgram(m_programID);
}

void OpenGLShader::unBind()
{
    RAPTURE_PROFILE_SCOPE("Shader Unbind");
    glUseProgram(0);
}

void OpenGLShader::dispatchCompute(uint32_t x, uint32_t y, uint32_t z)
{
    auto type = m_sources.find(ShaderType::COMPUTE);
    if (type == m_sources.end()) {
        GE_CORE_ERROR("OpenGLShader::dispatchCompute: Shader is not a compute shader");
        return;
    }

    glDispatchCompute(x, y, z);
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
        std::stringstream ss;
        size_t versionPos = source.find("#version");

        if (versionPos != std::string::npos) {
            size_t endOfVersionLine = source.find('\n', versionPos);
            if (endOfVersionLine != std::string::npos) {
                // #version line found, and it has a newline
                ss << source.substr(0, endOfVersionLine + 1); // Include the #version line and its newline
                for (const auto& define : variant.defines) {
                    ss << "#define " << define << std::endl;
                }
                ss << source.substr(endOfVersionLine + 1); // Add the rest of the source
            } else {
                // #version line found, but it's the last line (no newline after it)
                ss << source << std::endl; // Add the #version line and a newline
                for (const auto& define : variant.defines) {
                    ss << "#define " << define << std::endl;
                }
            }
        } else {
            // #version directive not found, prepend defines (fallback to old behavior)
            GE_CORE_WARN("OpenGLShader::processSource: '#version' directive not found in shader source for variant '{0}'. Prepending defines.", variant.name);
            for (const auto& define : variant.defines) {
                ss << "#define " << define << std::endl;
            }
            ss << source;
        }
        
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

void OpenGLShader::setUint(const std::string &name, uint32_t value)
{
    glUniform1ui(getUniformLocation(name), value);
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

void OpenGLShader::setUint64(const std::string& name, uint64_t handle)
{
    glUniformHandleui64ARB(getUniformLocation(name), handle);
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

std::shared_ptr<Shader> Shader::create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
{
    return std::make_shared<OpenGLShader>(vertexPath, fragmentPath);
}

Shader* Shader::createRaw(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
{
    return new OpenGLShader(vertexPath, fragmentPath);
}

std::shared_ptr<Shader> Shader::create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath)
{
    return std::make_shared<OpenGLShader>(vertexPath, fragmentPath, geometryPath);
}

Shader* Shader::createRaw(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath)
{
    return new OpenGLShader(vertexPath, fragmentPath, geometryPath);
}

std::shared_ptr<Shader> Shader::createCompute(const std::filesystem::path& computePath)
{
    return std::make_shared<OpenGLShader>(computePath);
}

Shader* Shader::createComputeRaw(const std::filesystem::path& computePath)
{
    return new OpenGLShader(computePath);
}




}


