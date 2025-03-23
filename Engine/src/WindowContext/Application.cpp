#include "Application.h"
#include <functional>
#include "../Logger/Log.h"

#include "../Timestep/Timestep.h"
#include "../Renderer/Renderer.h"
#include "../Debug/Profiler.h"
#include "../Debug/GPUProfiler.h"

namespace Rapture {

	Application* Application::s_instance = nullptr;

	Application::Application()
	{
		// Initialize profiler before any other operations
		Profiler::init();
		GPUProfiler::init();
		
		// creates openGL windows context, change it so its dynamic
		m_window = std::unique_ptr<WindowContext>(WindowContext::createWindow());
		m_window->setWindowEventCallback(std::bind(&Application::onEvent, this, std::placeholders::_1));
		s_instance = this;
		Renderer::init();
	}

	Application::~Application()
	{
		// Shutdown profiler
		GPUProfiler::shutdown();
		Profiler::shutdown();
		
		// closes twice...
		//onWindowContextClose();
	}

	void Application::Run(void)
	{
		while (m_running)
		{
			RAPTURE_PROFILE_FUNCTION();
			
			// Begin frame profiling
			Profiler::beginFrame();
			GPUProfiler::beginFrame();
			
			//GE_CORE_TRACE("DT: {0}", Timestep::deltaTimeMs());

			for (auto layer : m_layerStack)
			{
				RAPTURE_PROFILE_SCOPE(layer->getName().c_str());
				layer->onUpdate((float)Timestep::deltaTimeMs().count());
			}

			Timestep::onUpdate();
			m_window->onUpdate();
			
			// End frame profiling
			GPUProfiler::endFrame();
			Profiler::endFrame();
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
		GE_CORE_INFO("---Window Context Closed---");
		m_running = false;
		return true;
	}

	
	bool Application::onWindowContextResize(WindowResizeEvent& e)
	{
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
		m_layerStack.pushLayer(layer);
		layer->onAttach();
	}

	void Application::pushOverlay(Layer* overlay)
	{
		m_layerStack.pushOverlay(overlay);
		overlay->onAttach();
	}

}

