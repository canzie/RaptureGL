#pragma once
#include "Event.h"
#include <string>

namespace Rapture {

// Example of a custom event using the traditional event system
class SceneChangedEvent : public Event {
public:
    SceneChangedEvent(const std::string& oldSceneName, const std::string& newSceneName)
        : m_oldSceneName(oldSceneName), m_newSceneName(newSceneName) {
    }
    
    // Traditional event system interface
    EVENT_CLASS_TYPE(Custom) // Uses the Custom event type
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
    
    std::string toString() const override {
        return "SceneChangedEvent: " + m_oldSceneName + " -> " + m_newSceneName;
    }
    
    const std::string& getOldSceneName() const { return m_oldSceneName; }
    const std::string& getNewSceneName() const { return m_newSceneName; }
    
private:
    std::string m_oldSceneName;
    std::string m_newSceneName;
};

// Namespace for custom game-specific events using the EventBus system
namespace GameCustomEvents {
    // Define event types
    using PlayerDamagedEvent = EventBus<int, float>; // playerID, damageAmount
    using ItemCollectedEvent = EventBus<int, std::string>; // playerID, itemName
    using LevelCompletedEvent = EventBus<int, float>; // levelID, completionTime
    
    // Global event accessors
    inline PlayerDamagedEvent& onPlayerDamaged() {
        return EventRegistry::getInstance().getEventBus<int, float>("PlayerDamaged");
    }
    
    inline ItemCollectedEvent& onItemCollected() {
        return EventRegistry::getInstance().getEventBus<int, std::string>("ItemCollected");
    }
    
    inline LevelCompletedEvent& onLevelCompleted() {
        return EventRegistry::getInstance().getEventBus<int, float>("LevelCompleted");
    }
}

} 