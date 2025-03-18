//#include "../Events/Events.h"
//#include "../Events/ApplicationEvents.h"


#include "WindowContext.h"
#include "../Layers/LayerStack.h"
#include <memory>
#include <string>

namespace Rapture {

	class Application {
	public:

		Application();
		~Application();

		void Run(void);

		void onEvent(Event& e);

		bool onWindowContextClose(void);
		bool onWindowContextResize(WindowResizeEvent& e);

		void pushLayer(Layer* layer);
		void pushOverlay(Layer* overlay);

		static Application& getInstance() { return *s_instance; }
		WindowContext& getWindowContext() { return *m_window; }

		std::string getDebugName() { return m_debugName; }

		// Get the layer stack 
		const LayerStack& getLayerStack() const { return m_layerStack; }

	protected:
		std::string m_debugName;

	private:

		bool m_running = true;
		bool m_isMinimized = false;

		LayerStack m_layerStack;

		std::unique_ptr<WindowContext> m_window;

		static Application* s_instance;
	};

	Application* CreateWindow();

}