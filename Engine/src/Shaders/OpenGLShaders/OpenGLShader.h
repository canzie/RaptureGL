#pragma once

#include "../Shader.h"
#include <unordered_map>

namespace Rapture {



	class OpenGLShader : public Shader{

	public:
		OpenGLShader(std::string vertex_source, std::string fragment_source);
		virtual ~OpenGLShader() override;

		virtual void bind() override;
		virtual void unBind() override;

		virtual void setUniformMat4f(const std::string& name, glm::mat4& matrix) override;
		virtual void setUniform1f(const std::string& name, float val) override;
		virtual void setUniformVec3f(const std::string& name, glm::vec3& vector) override;

        virtual void setFloat(const std::string& name, float value) override;
        virtual void setInt(const std::string& name, int value) override;
        virtual void setBool(const std::string& name, bool value) override;
        virtual void setVec2(const std::string& name, const glm::vec2& value) override;
        virtual void setVec3(const std::string& name, const glm::vec3& value) override;
        virtual void setVec4(const std::string& name, const glm::vec4& value) override;
        virtual void setMat3(const std::string& name, const glm::mat3& value) override;
        virtual void setMat4(const std::string& name, const glm::mat4& value) override;
        virtual void setTexture(const std::string& name, std::shared_ptr<Texture2D> texture, uint32_t slot = 0) override;


		virtual bool compile(const std::string& variantName = "") override;
		virtual bool reload() override;
		virtual void addVariant(const ShaderVariant& variant) override;
		virtual void removeVariant(const std::string& name) override;

        virtual const std::vector<UniformInfo>& getUniforms() const override;

		virtual const std::vector<UniformInfo>& getSamplers() const override;

		virtual std::shared_ptr<Shader> loadFromCache(const std::string& name) override;
		virtual void saveToCache() const override;

		virtual void validateShaderProgram() override;

		//virtual std::map<std::string, float> getShaderUniforms() override { return m_uniforms; };


	private:
		unsigned int m_programID;
		std::vector<unsigned int> m_shaderIDs;
		std::map<ShaderType, std::string> m_sources;
		std::vector<UniformInfo> m_uniforms;
		std::vector<UniformInfo> m_samplers;
		std::vector<ShaderVariant> m_variants;
		ShaderStatus m_status = ShaderStatus::UNCOMPILED;
		
        bool compileShader(ShaderType type, const std::string& processed_source);
        bool linkProgram();
        void reflectUniforms(); 
        
        // Variant processing and compilation
        std::string processSource(const std::string& source, const ShaderVariant& variant);


        // Cached uniform locations for faster lookup
        std::unordered_map<std::string, int> m_uniformLocationCache;
        
        // Texture slots
        int m_textureSlots[32] = {0};
        
        // Get uniform location with caching
        int getUniformLocation(const std::string& name);
	};

}