#pragma once

#include <string>

// scuffed for noew fix this shit
#include "../../vendor/entt/entt.hpp"

namespace Rapture
{
	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity createEntity(const std::string& name = "Untitled Entity");
		void destroyEntity(Entity entity);

		//void OnUpdateRuntime(Timestep ts);
		//void OnViewportResize(unsigned int width, unsigned int height);

		entt::registry& getRegistry() { return m_Registry; }

	private:
		entt::registry m_Registry;
		//unsigned int m_ViewportWidth = 0, m_ViewportHeight = 0;

		friend class Entity;
		//friend class SceneHierarchy;

	};

}