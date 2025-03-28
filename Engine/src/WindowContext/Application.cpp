#include "Application.h"
#include <functional>
#include "../Logger/Log.h"

#include "../Timestep/Timestep.h"
#include "../Renderer/Renderer.h"
#include "../Debug/TracyProfiler.h"
//#include "../Debug/Profiler.h"
//#include "../Debug/GPUProfiler.h"
#include "../Textures/Texture.h"

#include "../Materials/MaterialLibrary.h"
#include "../Buffers/BufferPools.h"

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
			TextureLibrary::init(4);
			Rapture::MaterialLibrary::init();
			BufferPoolManager::init();
			Renderer::init();
			

		}
	}

	Application::~Application()
	{
		// Shutdown profilers
		//GPUProfiler::shutdown();
		//Profiler::shutdown();
		TracyProfiler::shutdown();
        TextureLibrary::shutdown();
        MaterialLibrary::shutdown();
		BufferPoolManager::shutdown();

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
                    layer->onUpdate((float)Timestep::deltaTimeMs().count());
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
                
                // Very important - collect GPU data
                TracyProfiler::collectGPUData();
                
                TracyProfiler::endFrame();
            }
		}
	}

	void Application::onEvent(Event& e)
	{
		RAPTURE_PROFILE_FUNCTION();
		
		switch (e.getEventType())
		{
		case EventType::WINDOW_CLOSE:
			onWindowContextClose();
			break;
		case EventType::WINDOW_RESIZE:
			// TODO: this is hella scuffed, fix this in some way
			onWindowContextResize(*(WindowResizeEvent*)&e);
			break;
		default:
			break;

			for (auto layer : m_layerStack)
			{
				layer->onEvent(e);
			}
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

}

