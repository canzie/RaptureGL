#pragma once
#include "Event.h"

// This file is kept for backward compatibility.
// It includes the unified event system from Event.h

/*
namespace Rapture {
	enum class EventType {
		None = 0,
		WINDOW_CLOSE, WINDOW_RESIZE, WINDOW_FOCUS, WindowMoved,
		KeyPressed, KeyReleased,
		MouseBtnPressed, MouseBtbReleased, MouseMoved, MouseScrolled
	};
	
	
	class Event {

	public:
		virtual std::string toString(void) = 0;
		EventType getEventType() { return m_event_type; }

	public:
		bool isHandled = false;

	protected:
		EventType m_event_type;
	};

	// Any additional event-related helpers could go here
}
*/