
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <map>



namespace Rapture {


	class Shader {
	
	public:

		Shader(std::string vertex_source, std::string fragment_source)
			:m_vertex_source(vertex_source), m_fragment_source(fragment_source) {}

		virtual void bind() = 0;
		virtual void unBind() = 0;

		virtual void setUniformMat4f(const std::string& name, glm::mat4& matrix)=0;
		virtual void setUniformVec3f(const std::string& name, glm::vec3& vector) = 0;
		virtual void setUniform1f(const std::string& name, float val) = 0;

		//virtual std::map<std::string, float> getShaderUniforms()=0;

	protected:
		std::string m_vertex_source;
		std::string m_fragment_source;
		//std::map<std::string, float> m_uniforms;

	};

}
