#include "../Shader.h"

namespace Rapture {



	class OpenGLShader : public Shader{

	public:
		OpenGLShader(std::string vertex_source, std::string fragment_source);
		~OpenGLShader();

		virtual void bind() override;
		virtual void unBind() override;

		virtual void setUniformMat4f(const std::string& name, glm::mat4& matrix) override;
		virtual void setUniform1f(const std::string& name, float val) override;
		virtual void setUniformVec3f(const std::string& name, glm::vec3& vector) override;

        virtual void setUniform(const std::string& name, const void* data) override;
        virtual void setUniformArray(const std::string& name, const void* data, size_t count) override;

		virtual bool compile() override;
		virtual bool reload() override;
		virtual void addVariant(const ShaderVariant& variant) override;
		virtual void removeVariant(const std::string& name) override;

        virtual const std::vector<UniformInfo>& getUniforms() const override;

		virtual const std::vector<UniformInfo>& getSamplers() const override;

		virtual std::shared_ptr<Shader> loadFromCache(const std::string& name) override;
		virtual void saveToCache() const override;


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
	};

}