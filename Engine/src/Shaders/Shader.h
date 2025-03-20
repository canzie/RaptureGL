#pragma once

#include <glm/glm.hpp>
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace Rapture {

    class Texture2D;

    enum class ShaderType {
        VERTEX,
        FRAGMENT,
        GEOMETRY,
        COMPUTE,
        TESS_CONTROL,
        TESS_EVAL
    };

    enum class ShaderStatus {
        UNCOMPILED,
        COMPILING,
        COMPILED,
        SHADER_ERROR
    };

    // Shader variant definition
    struct ShaderVariant {
        std::string name;
        std::vector<std::string> defines;
        std::vector<std::string> keywords;
    };

    enum class UniformType {
        FLOAT,
        VEC2,
        VEC3,
        VEC4,
        MAT3,
        MAT4,
        INT,
        UINT,
        BOOL,
        SAMPLER_2D,
        SAMPLER_CUBE,
    };

    // Shader uniform information
    struct UniformInfo {
        std::string name;
        UniformType type;
        uint32_t location;
        uint32_t size;
        uint32_t offset;
    };


	class Shader {
	
	public:
		Shader(std::string name)
			:m_name(name) { }

        virtual ~Shader() = default;

		virtual void bind() = 0;
        virtual void unBind() = 0;

        // Shader compilation and management
        virtual bool compile(const std::string& variantName = "") = 0;
        virtual bool reload() = 0;
        virtual void addVariant(const ShaderVariant& variant) = 0;
        virtual void removeVariant(const std::string& name) = 0;
        

        // Shader introspection
        virtual const std::vector<UniformInfo>& getUniforms() const = 0;
        virtual const std::vector<UniformInfo>& getSamplers() const = 0;
        
        // Shader caching
        virtual std::shared_ptr<Shader> loadFromCache(const std::string& name) = 0;
        virtual void saveToCache() const = 0;

        // Enhanced uniform setters for materials
        virtual void setFloat(const std::string& name, float value) = 0;
        virtual void setInt(const std::string& name, int value) = 0;
        virtual void setBool(const std::string& name, bool value) = 0;
        virtual void setVec2(const std::string& name, const glm::vec2& value) = 0;
        virtual void setVec3(const std::string& name, const glm::vec3& value) = 0;
        virtual void setVec4(const std::string& name, const glm::vec4& value) = 0;
        virtual void setMat3(const std::string& name, const glm::mat3& value) = 0;
        virtual void setMat4(const std::string& name, const glm::mat4& value) = 0;
        virtual void setTexture(const std::string& name, std::shared_ptr<Texture2D> texture, uint32_t slot = 0) = 0;

        // Legacy setters (kept for compatibility)
		virtual void setUniformMat4f(const std::string& name, glm::mat4& matrix) = 0;
		virtual void setUniformVec3f(const std::string& name, glm::vec3& vector) = 0;
		virtual void setUniform1f(const std::string& name, float val) = 0;

        virtual void validateShaderProgram() = 0;

        // Get the name of this shader
        const std::string& getName() const { return m_name; }

        // Get the shader program ID
        virtual unsigned int getProgramID() const = 0;
        
        // Get the shader status
        ShaderStatus getStatus() const { return m_status; }
        
        // Check if the shader is valid
        bool isValid() const { return m_status == ShaderStatus::COMPILED; }

	protected:
        std::string m_name;
        std::map<ShaderType, std::string> m_sources;
        std::vector<ShaderVariant> m_variants;
        std::vector<UniformInfo> m_uniforms;
        std::vector<UniformInfo> m_samplers;
        ShaderStatus m_status = ShaderStatus::UNCOMPILED;
        uint32_t m_programID = 0;
	};

}
