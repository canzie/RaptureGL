#include "Application.h"
#include <functional>
#include "../Logger/Log.h"

// Include Windows headers for memory tracking
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <psapi.h>
#endif

#include "../Timestep/Timestep.h"
#include "../Renderer/Renderer.h"
#include "../Debug/TracyProfiler.h"
//#include "../Debug/Profiler.h"
//#include "../Debug/GPUProfiler.h"
#include "../Textures/Texture.h"

#include "../Materials/MaterialLibrary.h"
#include "../Buffers/BufferPools.h"

#include "../AssetsManager/AssetManager.h"
#include "../Scenes/SceneManager.h"
#include "../Renderer/Deferred Shading/DeferredRenderer.h"

namespace Rapture {

	Application* Application::s_instance = nullptr;

	Application::Application()
	{
		// creates openGL windows context, change it so its dynamic
		m_window = std::unique_ptr<WindowContext>(WindowContext::createWindow());
		m_window->setWindowEventCallback(std::bind(&Application::onEvent, this, std::placeholders::_1));
		s_instance = this;

		// Initialize profilers before any other operations
		TracyProfiler::init();
		//Profiler::init();
		//GPUProfiler::init();
			// Now that OpenGL is fully initialized, we can initialize Tracy GPU context
		TracyProfiler::initGPUContext();
		// Initialize systems
		{
			RAPTURE_PROFILE_SCOPE("Systems Initialization");
            

			// Initialize project - this will setup default world and scene
			m_project = std::make_shared<Project>();
			auto working_dir = std::filesystem::current_path();
			auto project_dir = working_dir;
			
			// Try to find the project root by looking for Engine folder
			const int max_steps = 4;
			int steps = 0;
			while (steps < max_steps) {
				// Check if Engine directory exists in current path
				if (std::filesystem::exists(project_dir / "Engine") && std::filesystem::exists(project_dir / "build")) {
					break;
				}
				// Go up one directory
				auto parent = project_dir.parent_path();
				if (parent == project_dir) {  // We've hit the root
					break;
				}
				project_dir = parent;
				steps++;
			}

            m_project->setProjectDirectory(project_dir);

            
            AssetManager::init();

			TextureLibrary::init(4);
			MaterialLibrary::init();
			BufferPoolManager::init();
			Renderer::init();
            DeferredRenderer::init();

			



		}
	}

	Application::~Application()
	{
		TracyProfiler::shutdown();
        AssetManager::shutdown();
        TextureLibrary::shutdown();
        MaterialLibrary::shutdown();
		BufferPoolManager::shutdown();
        Renderer::shutdown();
        DeferredRenderer::shutdown();
        
		// closes twice...
		//onWindowContextClose();
	}

	void Application::Run(void)
	{
		while (m_running)
		{
			// Begin frame profiling
			RAPTURE_PROFILE_FUNCTION();
            
            // Start of frame
            {
                RAPTURE_PROFILE_SCOPE("Frame Start");
                RAPTURE_PROFILE_GPU_SCOPE("Frame Start");
                
                TracyProfiler::beginFrame();
            }
            
            
            // Update game state
            {
                RAPTURE_PROFILE_SCOPE("Game State Update");
                
                // Process texture loading queue
                {
                    RAPTURE_PROFILE_SCOPE("Texture Loading");
                    RAPTURE_PROFILE_GPU_SCOPE("Texture Loading");
                    TextureLibrary::processLoadingQueue();
                }
                
                // Update all layers
                for (auto layer : m_layerStack)
                {
                    RAPTURE_PROFILE_SCOPE("Layer Update");
                    layer->onUpdate(Timestep::deltaTime());
                }
                
                // Update active world
                auto activeWorld = SceneManager::getInstance().getActiveWorld();
                if (activeWorld && activeWorld->isActive()) {
                    RAPTURE_PROFILE_SCOPE("World Update");
                    activeWorld->update(Timestep::deltaTime());
                }
                
                // Update timestep
                {
                    RAPTURE_PROFILE_SCOPE("Timestep Update");
                    Timestep::onUpdate();
                }
            }
            
            // Rendering
            {
                RAPTURE_PROFILE_SCOPE("Rendering");
                RAPTURE_PROFILE_GPU_SCOPE("Rendering");
                
                m_window->onUpdate();
            }
            
            // End of frame
            {
                RAPTURE_PROFILE_SCOPE("Frame End");
                RAPTURE_PROFILE_GPU_SCOPE("Frame End");
                
                // Plot memory usage
                #ifdef _WIN32
                    PROCESS_MEMORY_COUNTERS pmc;
                    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                        // Plot working set size in KB
                        RAPTURE_PROFILE_PLOT("WorkingSet (KB)", (int64_t)(pmc.WorkingSetSize / 1024));
                    }
                #endif
                
                // Very important - collect GPU data
                TracyProfiler::collectGPUData();
                
                TracyProfiler::endFrame();
            }
		}
	}

	void Application::onEvent(Event& e)
	{
		RAPTURE_PROFILE_FUNCTION();
		
		// Use an EventDispatcher to handle different event types
		EventDispatcher dispatcher(e);
		
		// Dispatch window close event
		dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
			return onWindowContextClose();
		});
		
		// Dispatch window resize event
		dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
			return onWindowContextResize(e);
		});
		
		// Forward the event to layers in reverse order (top to bottom)
		for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
			if (e.handled) break;
			(*it)->onEvent(e);
		}
	}

	bool Application::onWindowContextClose(void)
	{
		RAPTURE_PROFILE_FUNCTION();
		m_running = false;
		return true;
	}

	
	bool Application::onWindowContextResize(WindowResizeEvent& e)
	{
		RAPTURE_PROFILE_FUNCTION();
        
		GE_CORE_INFO("{0}, {1}", e.getResolution().first, e.getResolution().second);

		if (e.getResolution().first == 0)
		{
			m_isMinimized = true;
			return false;
		}
		// call the framebuffer updater procedure thingy
		m_isMinimized = false;
		return true;
	}

	void Application::pushLayer(Layer* layer)
	{
		RAPTURE_PROFILE_FUNCTION();
        
		m_layerStack.pushLayer(layer);
		layer->onAttach();
	}

	void Application::pushOverlay(Layer* overlay)
	{
		RAPTURE_PROFILE_FUNCTION();
        
		m_layerStack.pushOverlay(overlay);
		overlay->onAttach();
	}
	
	// Project management
	void Application::loadProject(const std::string& projectPath) {
		// Destroy old project and all its children automatically
		m_project = Project::loadProject(projectPath);
	}
	
	void Application::saveProject(const std::string& projectPath) {
		Project::saveProject(projectPath);
	}
	
	std::shared_ptr<Project> Application::getProject() const {
		return m_project;
	}
	
	// World operations
	void Application::transitionToWorld(const std::string& worldName) {
		// Make sure the world exists
		auto world = m_project->getWorld(worldName);
		if (!world) {
			// Create the world if it doesn't exist
			world = m_project->createWorld(worldName);
		}
		
		// Set as active
		m_project->setActiveWorld(worldName);
	}
	
	// Layer access
	Layer* Application::getLayerByName(const std::string& name) {
		for (auto layer : m_layerStack) {
			if (layer->getLayerName() == name) {
				return layer;
			}
		}
		return nullptr;
	}

}

