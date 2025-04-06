#pragma once

#include <string>

// scuffed for noew fix this shit
#include "../../vendor/entt/entt.hpp"
#include "../Textures/Texture.h"
#include "../Renderer/PrimitiveShapes.h"
#include "../Materials/Material.h"
#include "../AssetsManager/AssetManager.h"

namespace Rapture
{


    struct SceneSettings {
        std::string sceneName;

        bool frustumCullingEnabled = false;
        bool rayCastDebugEnabled = false;
        bool useAsyncRendering = true; // Enable by default for better performance

        //Entity mainCamera;


    };

    struct SkyBox {

        std::vector<std::filesystem::path> texturePaths;
        std::shared_ptr<Texture2D> texture;
        Cube skybox;

        SkyBox()
        {
            skybox = Cube(true);
        }

        SkyBox(std::vector<std::filesystem::path> texturePaths)
        {
            this->texturePaths = texturePaths;
            //texture = TextureLibrary::loadCubemap(texturePaths);
            auto [asset, handle] = AssetManager::importAsset<Texture2D>(texturePaths);
            skybox = Cube(true);
            skybox.getMaterial()->setTexture("skybox", asset, handle);
        }

        void setTexturePaths(std::vector<std::filesystem::path> texturePaths)
        {
            this->texturePaths = texturePaths;
            //texture = TextureLibrary::loadCubemap(texturePaths);
            auto [asset, handle] = AssetManager::importAsset<Texture2D>(texturePaths);
            skybox.getMaterial()->setTexture("skybox", asset, handle);
        }
        
    };
    
	class Entity;
	class Scene
	{
	public:
		Scene(std::string sceneName="Untitled Scene");
		~Scene();

		Entity createEntity(const std::string& name = "Untitled Entity");
		void destroyEntity(Entity entity);

		//void OnUpdateRuntime(Timestep ts);
        void onUpdate();

		//void OnViewportResize(unsigned int width, unsigned int height);

		entt::registry& getRegistry() { return m_Registry; }

        SceneSettings& getSettings() { return m_config; }
        SkyBox& getSkyBox() { return m_SkyBox; }

        std::string getSceneName() { return m_config.sceneName; }

	private:
		entt::registry m_Registry;
		//unsigned int m_ViewportWidth = 0, m_ViewportHeight = 0;

		friend class Entity;

        SceneSettings m_config;
        SkyBox m_SkyBox;


	};

}