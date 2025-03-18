#pragma once
#include "Events.h"

namespace Rapture {

    class MouseButtonEvent : public Event
    {
    public:
        int getMouseButton() const { return m_button; }

    protected:
        MouseButtonEvent(int button)
            : m_button(button) {}

        int m_button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(int button)
            : MouseButtonEvent(button) 
        {
            m_event_type = EventType::MouseBtnPressed;
        }

        std::string toString() override
        {
            return std::format("MouseButtonPressedEvent: {}", m_button);
        }
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(int button)
            : MouseButtonEvent(button) 
        {
            m_event_type = EventType::MouseBtbReleased;
        }

        std::string toString() override
        {
            return std::format("MouseButtonReleasedEvent: {}", m_button);
        }
    };

    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(float x, float y)
            : m_mouseX(x), m_mouseY(y) 
        {
            m_event_type = EventType::MouseMoved;
        }

        float getX() const { return m_mouseX; }
        float getY() const { return m_mouseY; }

        std::string toString() override
        {
            return std::format("MouseMovedEvent: {}, {}", m_mouseX, m_mouseY);
        }

    private:
        float m_mouseX, m_mouseY;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(float xOffset, float yOffset)
            : m_xOffset(xOffset), m_yOffset(yOffset) 
        {
            m_event_type = EventType::MouseScrolled;
        }

        float getXOffset() const { return m_xOffset; }
        float getYOffset() const { return m_yOffset; }

        std::string toString() override
        {
            return std::format("MouseScrolledEvent: {}, {}", m_xOffset, m_yOffset);
        }

    private:
        float m_xOffset, m_yOffset;
    };

} 