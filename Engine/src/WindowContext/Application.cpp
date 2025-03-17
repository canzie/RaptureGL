#include "Application.h"
#include <functional>
#include "../Logger/Log.h"

#include "../Timestep/Timestep.h"
#include "../Renderer/Renderer.h"

namespace Rapture {

	Application* Application::s_instance = nullptr;

	Application::Application()
	{
		// creates openGL windows context, change it so its dynamic
		m_window = std::unique_ptr<WindowContext>(WindowContext::createWindow());
		m_window->setWindowEventCallback(std::bind(&Application::onEvent, this, std::placeholders::_1));
		s_instance = this;
		Renderer::init();
	}

	Application::~Application()
	{
		// closes twice...
		//onWindowContextClose();
	}

	void Application::Run(void)
	{
		while (m_running)
		{
			//GE_CORE_TRACE("DT: {0}", Timestep::deltaTimeMs());

			for (auto layer : m_layerStack)
			{
				layer->onUpdate((float)Timestep::deltaTimeMs().count());
			}

			Timestep::onUpdate();
			m_window->onUpdate();
		}
	}

	void Application::onEvent(Event& e)
	{
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

}

