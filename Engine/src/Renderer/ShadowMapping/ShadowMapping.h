#pragma once

#include "../Framebuffer.h"
#include "../../Shaders/Shader.h"
#include "../../Logger/Log.h"
#include "../../AssetsManager/AssetManager.h"

#include <glm/glm.hpp>

#include <memory>

namespace Rapture {

    class ShadowMap {

    public:

        ShadowMap(uint32_t width, uint32_t height);

        void bind();
        void bindForReading();

        void unbind();

        void setShader(AssetHandle shaderHandle);
        std::shared_ptr<Shader> getShader();

        void setWVPMatrix(const glm::mat4& gWVP);


    private:

        std::shared_ptr<Framebuffer> m_ShadowMap;
        std::weak_ptr<Shader> m_Shader;
        AssetHandle m_SMShaderHandle;

        glm::mat4 m_gWVP;


    };


}