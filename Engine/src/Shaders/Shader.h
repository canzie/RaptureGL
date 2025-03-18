#pragma once

#include <glm/glm.hpp>
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace Rapture {

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

		virtual void bind() = 0;
		virtual void unBind() = 0;

        // Shader compilation and management
        virtual bool compile() = 0;
        virtual bool reload() = 0;
        virtual void addVariant(const ShaderVariant& variant) = 0;
        virtual void removeVariant(const std::string& name) = 0;
        
        // Uniform management
        virtual void setUniform(const std::string& name, const void* data) = 0;
        virtual void setUniformArray(const std::string& name, const void* data, size_t count) = 0;
        
        // Shader introspection
        virtual const std::vector<UniformInfo>& getUniforms() const = 0;
        virtual const std::vector<UniformInfo>& getSamplers() const = 0;
        
        // Shader caching
        virtual std::shared_ptr<Shader> loadFromCache(const std::string& name) = 0;
        virtual void saveToCache() const = 0;


		virtual void setUniformMat4f(const std::string& name, glm::mat4& matrix)=0;
		virtual void setUniformVec3f(const std::string& name, glm::vec3& vector) = 0;
		virtual void setUniform1f(const std::string& name, float val) = 0;

		//virtual std::map<std::string, float> getShaderUniforms()=0;

	protected:
        std::string m_name;
        std::map<ShaderType, std::string> m_sources;
        std::vector<ShaderVariant> m_variants;
        std::vector<UniformInfo> m_uniforms;
        std::vector<UniformInfo> m_samplers;
        ShaderStatus m_status;
        uint32_t m_programID;

	};

}
