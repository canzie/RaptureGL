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


		//virtual std::map<std::string, float> getShaderUniforms() override { return m_uniforms; };


	private:
		void initShader();

	private:
		unsigned int m_shaderID;

	};

}