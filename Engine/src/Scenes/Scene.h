#pragma once

#include <string>

// scuffed for noew fix this shit
#include "../../vendor/entt/entt.hpp"
#include "../Textures/Texture.h"
#include "../Renderer/PrimitiveShapes.h"
#include "../Materials/Material.h"

namespace Rapture
{

    struct SceneSettings {

        bool frustumCullingEnabled = false;
        bool rayCastDebugEnabled = false;
    };

    struct SkyBox {

        std::vector<std::string> texturePaths;
        std::shared_ptr<Texture2D> texture;
        Cube skybox;

        SkyBox()
        {
            skybox = Cube(true);
        }

        SkyBox(std::vector<std::string> texturePaths)
        {
            this->texturePaths = texturePaths;
            texture = TextureLibrary::loadCubemap(texturePaths);
            skybox = Cube(true);
            skybox.getMaterial()->setTexture("skybox", texture);
        }

        void setTexturePaths(std::vector<std::string> texturePaths)
        {
            this->texturePaths = texturePaths;
            texture = TextureLibrary::loadCubemap(texturePaths);
            skybox.getMaterial()->setTexture("skybox", texture);
        }
        
    };

	class Entity;
	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity createEntity(const std::string& name = "Untitled Entity");
		void destroyEntity(Entity entity);

		//void OnUpdateRuntime(Timestep ts);
        void onUpdate();

		//void OnViewportResize(unsigned int width, unsigned int height);

		entt::registry& getRegistry() { return m_Registry; }

        SceneSettings& getSettings() { return m_Settings; }
        SkyBox& getSkyBox() { return m_SkyBox; }

	private:
		entt::registry m_Registry;
		//unsigned int m_ViewportWidth = 0, m_ViewportHeight = 0;

		friend class Entity;

        SceneSettings m_Settings;
        SkyBox m_SkyBox;


	};

}