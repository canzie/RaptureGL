#pragma once
//#include "../Events/Events.h"
//#include "../Events/ApplicationEvents.h"


#include "WindowContext.h"
#include "../Layers/LayerStack.h"
#include "../Events/Event.h"
#include "../Events/ApplicationEvents.h"
#include "../Project/Project.h"
#include <memory>
#include <string>
#include <unordered_set>

namespace Rapture {

	class Application {
	public:

		Application();
		virtual ~Application();

		void Run(void);

		void onEvent(Event& e);

		bool onWindowContextClose(void);
		bool onWindowContextResize(WindowResizeEvent& e);

		void pushLayer(Layer* layer);
		void pushOverlay(Layer* overlay);

		WindowContext& getWindowContext() { return *m_window.get(); }

		static Application& getInstance() { return *s_instance; }
		LayerStack& getLayerStack() { return m_layerStack; }

		// Project management
		void loadProject(const std::string& projectPath);
		void saveProject(const std::string& projectPath);
		std::shared_ptr<Project> getProject() const;

		// World operations
		void transitionToWorld(const std::string& worldName);

		// Layer access
		Layer* getLayerByName(const std::string& name);

		std::string getDebugName() { return m_debugName; }

		std::shared_ptr<Project> getCurrentProject() const { return m_project; }

	protected:
		std::string m_debugName;
		std::shared_ptr<Project> m_project;

	private:

		bool m_running = true;
		bool m_isMinimized = false;

		LayerStack m_layerStack;

		std::unique_ptr<WindowContext> m_window;

		static Application* s_instance;
	};

	Application* CreateWindow();

}