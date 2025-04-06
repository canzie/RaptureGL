#pragma once
#include "Event.h"

namespace Rapture {

    class MouseButtonEvent : public Event
    {
    public:
        int getMouseButton() const { return m_button; }
        
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryMouseButton)

    protected:
        MouseButtonEvent(int button)
            : m_button(button) {}

        int m_button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(int button)
            : MouseButtonEvent(button) {}

        EVENT_CLASS_TYPE(MouseButtonPressed)
        
        std::string toString() const override
        {
            return "MouseButtonPressedEvent: " + std::to_string(m_button);
        }
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(int button)
            : MouseButtonEvent(button) {}

        EVENT_CLASS_TYPE(MouseButtonReleased)
        
        std::string toString() const override
        {
            return "MouseButtonReleasedEvent: " + std::to_string(m_button);
        }
    };

    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(float x, float y)
            : m_mouseX(x), m_mouseY(y) {}

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse)
        
        float getX() const { return m_mouseX; }
        float getY() const { return m_mouseY; }

        std::string toString() const override
        {
            return "MouseMovedEvent: " + std::to_string(m_mouseX) + ", " + std::to_string(m_mouseY);
        }

    private:
        float m_mouseX, m_mouseY;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(float xOffset, float yOffset)
            : m_xOffset(xOffset), m_yOffset(yOffset) {}

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse)
        
        float getXOffset() const { return m_xOffset; }
        float getYOffset() const { return m_yOffset; }

        std::string toString() const override
        {
            return "MouseScrolledEvent: " + std::to_string(m_xOffset) + ", " + std::to_string(m_yOffset);
        }

    private:
        float m_xOffset, m_yOffset;
    };
} 