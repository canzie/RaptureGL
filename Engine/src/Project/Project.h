#pragma once
#include "../Scenes/Scene.h"
#include "../Scenes/World.h"
#include "../Scenes/SceneManager.h"
#include <memory>
#include <string>
#include <filesystem>
#include <unordered_map>

namespace Rapture
{
    struct ProjectConfig
    {
        std::string name;
        std::string directory;
        std::string initialWorldName;
    };

    class Project
    {
    public:
        Project()
            : m_config{"New Project", ".", "DefaultWorld"}
        {

            GE_CORE_INFO("Creating Project: {0}", m_config.name);

            // Create a default world with a default scene
            auto defaultWorld = std::make_shared<World>("DefaultWorld");
            auto defaultScene = std::make_shared<Scene>();

            
            // Register with SceneManager
            SceneManager::getInstance().createScene("DefaultScene");
            SceneManager::getInstance().setActiveScene("DefaultScene");
            
            // Add scene to world and set as main
            defaultWorld->addScene("DefaultScene", defaultScene);
            defaultWorld->setMainScene("DefaultScene");
            
            // Register world with SceneManager
            SceneManager::getInstance().registerWorld(defaultWorld);
            
            // Set as active
            SceneManager::getInstance().setActiveWorld("DefaultWorld");


        }

        ~Project()
        {
            // Clear all worlds and scenes
            m_worlds.clear();
        }

        // Get the active scene from the SceneManager
        std::shared_ptr<Scene> getActiveScene() const 
        { 
            return SceneManager::getInstance().getActiveScene(); 
        }
        
        // Set the active scene via SceneManager
        void setActiveScene(std::shared_ptr<Scene> scene) 
        { 
            SceneManager::getInstance().setActiveScene(scene); 
        }
        
        // World management
        std::shared_ptr<World> createWorld(const std::string& name)
        {
            auto world = std::make_shared<World>(name);
            m_worlds[name] = world;
            SceneManager::getInstance().registerWorld(world);
            return world;
        }
        
        std::shared_ptr<World> getWorld(const std::string& name) const
        {
            auto it = m_worlds.find(name);
            if (it != m_worlds.end()) {
                return it->second;
            }
            return nullptr;
        }
        
        void setActiveWorld(const std::string& name)
        {
            SceneManager::getInstance().setActiveWorld(name);
        }
        
        std::shared_ptr<World> getActiveWorld() const
        {
            return SceneManager::getInstance().getActiveWorld();
        }

        // Project file operations
        static void saveProject(std::filesystem::path path) 
        {
            // Implementation for serializing project
        }
        
        static std::shared_ptr<Project> loadProject(std::filesystem::path path)
        {
            auto project = std::make_shared<Project>();
            
            // Implementation for deserializing project
            
            return project;
        }

        // Project config access
        std::string getProjectDirectory() const { return m_config.directory; }
        std::string getProjectName() const { return m_config.name; }
        std::string getInitialWorldName() const { return m_config.initialWorldName; }
        
        void setProjectDirectory(const std::string& dir) { m_config.directory = dir; }
        void setProjectName(const std::string& name) { m_config.name = name; }
        void setInitialWorldName(const std::string& name) { m_config.initialWorldName = name; }

    private:
        ProjectConfig m_config;
        std::unordered_map<std::string, std::shared_ptr<World>> m_worlds;
    };
}
