#pragma once
#include "Events.h"

namespace Rapture {

	class WindowCloseEvent : public Event{
	
	public:

		WindowCloseEvent(){
			m_event_type = EventType::WINDOW_CLOSE;
		}

		virtual std::string toString() override {
			return std::string("Close Event");
		}

	};

	class WindowResizeEvent : public Event {

	public:

		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_width(width), m_height(height) {
			m_event_type = EventType::WINDOW_RESIZE;
		}

		virtual std::string toString() override {
			return std::format("({}, {})", std::to_string(m_width), std::to_string(m_height));
		}

		std::pair<unsigned int, unsigned int> getResolution() {
			return { m_width, m_height };
		}

	private:
		unsigned int m_width, m_height;

	};


}