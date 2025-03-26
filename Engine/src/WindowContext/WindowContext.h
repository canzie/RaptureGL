#pragma once

//#include "../Events/Events.h"
#include "../Events/ApplicationEvents.h"

#include <functional>

namespace Rapture {

	// Buffer swap mode enumeration
	enum class SwapMode {
		Immediate,        // No VSync, uncapped framerate (double buffering)
		VSync,            // Traditional VSync with double buffering
		AdaptiveVSync,    // Adaptive VSync with triple buffering (if supported)
		TripleBuffering   // Triple buffering without VSync (uncapped framerate)
	};

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
		
		// Buffer swap control functions (implemented by derived classes)
		virtual void setSwapMode(SwapMode mode) {}
		virtual SwapMode getSwapMode() const { return static_cast<SwapMode>(0); } // Default implementation
		virtual bool isTripleBufferingSupported() const { return false; }

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