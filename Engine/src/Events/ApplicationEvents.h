#pragma once
#include "Event.h"
#include <format>

namespace Rapture {

	class WindowCloseEvent : public Event {
	
	public:

		WindowCloseEvent() {
			m_eventType = EventType::WindowClose;
		}

		EVENT_CLASS_TYPE(WindowClose)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return "WindowCloseEvent";
		}

	private:
		EventType m_eventType;

	};

	class WindowResizeEvent : public Event {

	public:

		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_width(width), m_height(height) {
			m_eventType = EventType::WindowResize;
		}

		EVENT_CLASS_TYPE(WindowResize)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return std::format("WindowResizeEvent: ({}, {})", m_width, m_height);
		}

		unsigned int getWidth() const { return m_width; }
		unsigned int getHeight() const { return m_height; }
		
		std::pair<unsigned int, unsigned int> getResolution() const {
			return { m_width, m_height };
		}

	private:
		unsigned int m_width, m_height;
		EventType m_eventType;

	};

	class WindowFocusEvent : public Event {
	public:
		WindowFocusEvent() {
			m_eventType = EventType::WindowFocus;
		}
		
		EVENT_CLASS_TYPE(WindowFocus)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return "WindowFocusEvent";
		}
		
	private:
		EventType m_eventType;
	};
	
	class WindowLostFocusEvent : public Event {
	public:
		WindowLostFocusEvent() {
			m_eventType = EventType::WindowLostFocus;
		}
		
		EVENT_CLASS_TYPE(WindowLostFocus)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return "WindowLostFocusEvent";
		}
		
	private:
		EventType m_eventType;
	};
	
	class WindowMovedEvent : public Event {
	public:
		WindowMovedEvent(unsigned int x, unsigned int y)
			: m_xPos(x), m_yPos(y) {
			m_eventType = EventType::WindowMoved;
		}
		
		EVENT_CLASS_TYPE(WindowMoved)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return std::format("WindowMovedEvent: ({}, {})", m_xPos, m_yPos);
		}
		
		unsigned int getX() const { return m_xPos; }
		unsigned int getY() const { return m_yPos; }
		
	private:
		unsigned int m_xPos, m_yPos;
		EventType m_eventType;
	};

	class AppTickEvent : public Event {
	public:
		AppTickEvent() {
			m_eventType = EventType::AppTick;
		}
		
		EVENT_CLASS_TYPE(AppTick)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return "AppTickEvent";
		}
		
	private:
		EventType m_eventType;
	};
	
	class AppUpdateEvent : public Event {
	public:
		AppUpdateEvent() {
			m_eventType = EventType::AppUpdate;
		}
		
		EVENT_CLASS_TYPE(AppUpdate)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return "AppUpdateEvent";
		}
		
	private:
		EventType m_eventType;
	};
	
	class AppRenderEvent : public Event {
	public:
		AppRenderEvent() {
			m_eventType = EventType::AppRender;
		}
		
		EVENT_CLASS_TYPE(AppRender)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
		std::string toString() const override {
			return "AppRenderEvent";
		}
		
	private:
		EventType m_eventType;
	};

}