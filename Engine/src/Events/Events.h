#pragma once
#include <string>
#include <format>
#include <utility>

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
}