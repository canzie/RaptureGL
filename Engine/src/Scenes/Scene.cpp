#include "Scene.h"
#include "Entity.h"
#include "Components/Components.h"
#include "../Renderer/Renderer.h"

namespace Rapture
{
	Scene::Scene(std::string sceneName)
	{
		m_config.sceneName = sceneName;
        m_SkyBox = nullptr;
	}
	Scene::~Scene()
	{
	}

	Entity Scene::createEntity(const std::string& name)
	{
		Entity entity(m_Registry.create(), this);
		entity.addComponent<TagComponent>(name);

		return entity;
	}
	void Scene::destroyEntity(Entity entity)
	{
		m_Registry.destroy(entity);
	}

    void Scene::onUpdate()
    {
        if (m_SkyBox)
        {
            Renderer::drawCube(m_SkyBox->skybox);
        } else {
            m_SkyBox = std::make_unique<SkyBox>();
            Renderer::drawCube(m_SkyBox->skybox);
        }
    }

    SkyBox &Scene::getSkyBox()
    {
        if (!m_SkyBox)
        {
            m_SkyBox = std::make_unique<SkyBox>();
            return *m_SkyBox;
        }
        
        return *m_SkyBox;
    }

    /*
	void Scene::OnUpdateRuntime(Timestep ts)
	{

	}

	void Scene::OnViewportResize(unsigned int width, unsigned int height)
	{
	}
	*/


}