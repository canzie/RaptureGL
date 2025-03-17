#pragma once

//#include "../Events/Events.h"
#include "../Events/ApplicationEvents.h"

#include <functional>

namespace Rapture {


	class WindowContext {

	public:
		~WindowContext() = default;

		// create context and set the callbacks
		virtual void initWindow(void) = 0;
		virtual void closeWindow(void) = 0;

		virtual void onUpdate(void) = 0;

		void setWindowEventCallback(const std::function<void(Event&)> callback) {
			m_context_data.eventFnCallback = callback;
		}

		virtual void* getNativeWindowContext() = 0;

		static WindowContext* createWindow();


	protected:

		struct ContextData {
			unsigned int height;
			unsigned int width;

			// function to be called when an event happens
			std::function<void(Event&)> eventFnCallback;
		} m_context_data;


	};


}