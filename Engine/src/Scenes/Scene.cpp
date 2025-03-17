#include "Scene.h"
#include "Entity.h"


namespace Rapture
{
	Scene::Scene()
	{
	}
	Scene::~Scene()
	{
	}

	Entity Scene::createEntity(const std::string& name)
	{
		Entity entity(m_Registry.create(), this);

		return entity;
	}
	void Scene::destroyEntity(Entity entity)
	{
		m_Registry.destroy(entity);
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