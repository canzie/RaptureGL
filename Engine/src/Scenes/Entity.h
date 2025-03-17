#pragma once


#include "Scene.h"


namespace Rapture
{

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_EntityHandle(handle), m_Scene(scene) {}

		Entity(const Entity& other) = default;

		template<typename T, typename... Args>
		void addComponent(Args&&... args)
		{
			m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& getComponent()
		{
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		bool hasComponent()
		{
			return m_Scene->m_Registry.valid<T>(m_EntityHandle);
		}

		template<typename T>
		void removeComponent()
		{
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		operator entt::entity() const { return m_EntityHandle; }

		operator bool() const { return m_EntityHandle != entt::null; }

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}
		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}
		entt::entity m_EntityHandle{ entt::null };

	private:
		Scene* m_Scene = nullptr;
	};

}