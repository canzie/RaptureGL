#pragma once
#include <string>
#include <format>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <typeindex>

namespace Rapture {

    // Forward declare Scene and World classes
    class Scene;
    class World;

/**
 * Unified Event System for Rapture Engine
 * This file contains two complementary event mechanisms:
 * 1. Traditional class-based events (Event, EventType, etc.)
 * 2. Observer pattern using EventDispatcher<...> for callbacks
 */

// ---- Traditional Event System ----

// Event types enumeration
enum class EventType {
	None = 0,
	
	// Window events
	WindowClose,
	WindowResize,
	WindowFocus,
	WindowLostFocus,
	WindowMoved,
	
	// Keyboard events
	KeyPressed,
	KeyReleased,
	KeyTyped,
	
	// Mouse events
	MouseButtonPressed,
	MouseButtonReleased,
	MouseMoved,
	MouseScrolled,
	
	// Application events
	AppTick,
	AppUpdate,
	AppRender,
	
	// Custom engine events
	Custom
};

// Event categories for filtering
enum EventCategory {
	None = 0,
	EventCategoryApplication = 1 << 0,
	EventCategoryInput       = 1 << 1,
	EventCategoryKeyboard    = 1 << 2,
	EventCategoryMouse       = 1 << 3,
	EventCategoryMouseButton = 1 << 4
};

// Macros to avoid repetitive code in event classes
#define EVENT_CLASS_TYPE(type) static EventType getStaticType() { return EventType::type; } \
							   virtual EventType getEventType() const override { return getStaticType(); } \
							   virtual const char* getName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int getCategoryFlags() const override { return category; }

/**
 * Base Event class
 * All event types derive from this abstract base class
 */
class Event {
	friend class EventDispatcher;
public:
	virtual ~Event() = default;
	
	// Properties all events must implement
	virtual EventType getEventType() const = 0;
	virtual const char* getName() const = 0;
	virtual int getCategoryFlags() const = 0;
	virtual std::string toString() const { return getName(); }
	
	// Category checking
	bool isInCategory(EventCategory category) const {
		return getCategoryFlags() & category;
	}
	
	// Flag indicating if the event has been handled
	bool handled = false;
};

/**
 * Event Dispatcher for the traditional event system
 * Used to dispatch events to handler functions based on type
 */
class EventDispatcher {
public:
	EventDispatcher(Event& event)
		: m_event(event) {}
	
	// Dispatch an event to a handler function
	template<typename T, typename F>
	bool dispatch(const F& func) {
		if (m_event.getEventType() == T::getStaticType()) {
			m_event.handled = func(static_cast<T&>(m_event));
			return true;
		}
		return false;
	}
	
private:
	Event& m_event;
};

// ---- Generic Event System ----

/**
 * EventBus - A templated event dispatcher for the observer pattern
 * Allows subscribing to and publishing events with arbitrary parameters
 */
template<typename... Args>
class EventBus {
public:
	using EventCallback = std::function<void(Args...)>;
	using ListenerID = size_t;
	
	// Add a listener to this event
	ListenerID addListener(const EventCallback& callback) {
		m_listeners[m_nextListenerID] = callback;
		return m_nextListenerID++;
	}
	
	// Remove a listener by ID
	void removeListener(ListenerID id) {
		m_listeners.erase(id);
	}
	
	// Publish an event to all listeners
	void publish(Args... args) const {
		for (const auto& [id, listener] : m_listeners) {
			listener(args...);
		}
	}
	
	// Alias for publish to maintain compatibility with existing code
	void invoke(Args... args) const {
		publish(args...);
	}
	
private:
	std::unordered_map<ListenerID, EventCallback> m_listeners;
	ListenerID m_nextListenerID = 0;
};

// Type-erased event handler base class for the event registry
class BaseEventHandler {
public:
	virtual ~BaseEventHandler() = default;
};

// Concrete event handler for specific event types
template<typename... Args>
class EventHandler : public BaseEventHandler {
public:
	EventHandler() = default;
	
	EventBus<Args...>& getEventBus() { return m_eventBus; }
	
private:
	EventBus<Args...> m_eventBus;
};

/**
 * EventRegistry - Global registry for event buses
 * Allows accessing event buses by name rather than by type
 */
class EventRegistry {
public:
	static EventRegistry& getInstance() {
		static EventRegistry instance;
		return instance;
	}
	
	// Get or create an event bus for the given name and types
	template<typename... Args>
	EventBus<Args...>& getEventBus(const std::string& name) {
		//std::type_index typeIdx = std::type_index(typeid(EventBus<Args...>));
		std::string typeId = typeid(EventBus<Args...>).name();

		// Create the type-event pair if it doesn't exist

        // Check if bus exists with correct type
        auto busIt = m_eventBuses.find(name);
        if (busIt == m_eventBuses.end() || 
            m_eventTypeNames[name] != typeId) {
            
            // Create new bus
            m_eventBuses[name] = std::make_shared<EventHandler<Args...>>();
            m_eventTypeNames[name] = typeId;
        }
		
		// Return the event bus
		return static_cast<EventHandler<Args...>*>(m_eventBuses[name].get())->getEventBus();
	}
	
private:
	EventRegistry() = default;
	
	std::unordered_map<std::string, std::shared_ptr<BaseEventHandler>> m_eventBuses;
	std::unordered_map<std::string, std::string> m_eventTypeNames;
};

// Namespace for predefined global events
namespace GameEvents {
	// Scene events
	using SceneLoadRequestedEvent = EventBus<std::string>;
	using SceneActivatedEvent = EventBus<std::shared_ptr<Scene>>;
	using SceneDeactivatedEvent = EventBus<std::shared_ptr<Scene>>;
	
	// World events
	using WorldTransitionRequestedEvent = EventBus<std::string>;
	using WorldActivatedEvent = EventBus<std::shared_ptr<World>>;
	
	// Layer communication events
	using LayerCommunicationEvent = EventBus<std::string, std::string>;
	
	// Project events
	using ProjectLoadRequestedEvent = EventBus<std::string>;
	using ProjectLoadedEvent = EventBus<std::string>;
	
	// Global event accessors
	inline SceneLoadRequestedEvent& onSceneLoadRequested() {
		return EventRegistry::getInstance().getEventBus<std::string>("SceneLoadRequested");
	}
	
	inline SceneActivatedEvent& onSceneActivated() {
		return EventRegistry::getInstance().getEventBus<std::shared_ptr<Scene>>("SceneActivated");
	}
	
	inline SceneDeactivatedEvent& onSceneDeactivated() {
		return EventRegistry::getInstance().getEventBus<std::shared_ptr<Scene>>("SceneDeactivated");
	}
	
	inline WorldTransitionRequestedEvent& onWorldTransitionRequested() {
		return EventRegistry::getInstance().getEventBus<std::string>("WorldTransitionRequested");
	}
	
	inline WorldActivatedEvent& onWorldActivated() {
		return EventRegistry::getInstance().getEventBus<std::shared_ptr<World>>("WorldActivated");
	}
	
	inline LayerCommunicationEvent& onLayerCommunication() {
		return EventRegistry::getInstance().getEventBus<std::string, std::string>("LayerCommunication");
	}
	
	inline ProjectLoadRequestedEvent& onProjectLoadRequested() {
		return EventRegistry::getInstance().getEventBus<std::string>("ProjectLoadRequested");
	}
	
	inline ProjectLoadedEvent& onProjectLoaded() {
		return EventRegistry::getInstance().getEventBus<std::string>("ProjectLoaded");
	}
}

} // namespace Rapture