# Rapture Game Engine Documentation

(last update : 06/04/2025)

## Overview
This document provides comprehensive documentation for the Rapture Game Engine and Editor. The engine is built with modern C++ and utilizes various external libraries for different functionalities.

## Engine Systems

### Core Systems

#### Application Layer
The engine is built around a core application layer that manages the window context, event system, and layer stack. The main components are:

- **Application Class**: Central manager for the engine that:
  - Initializes core systems (OpenGL context, profilers, texture system, etc.)
  - Manages the main game loop
  - Handles window events
  - Manages the layer stack
  - Provides singleton access to engine systems

- **Window Context System**:
  - Abstracts window creation and management using GLFW
  - Supports multiple buffer swap modes:
    - Immediate (No VSync, uncapped framerate)
    - VSync (Traditional double buffering)
    - AdaptiveVSync (Triple buffering with tear control)
    - TripleBuffering (Uncapped framerate with triple buffering)
  - Handles window events and callbacks

#### Layer System
The engine uses a layer-based architecture for organizing and managing different parts of the application:

- **Layer Stack**: 
  - Manages a stack of layers and overlays
  - Layers are processed in order for updates and events
  - Supports both regular layers and overlays (rendered on top)
  - Provides iteration and management functions

- **Layer Interface**:
  - Virtual interface for creating new layers
  - Lifecycle methods: onAttach(), onDetach()
  - Update and event handling: onUpdate(), onEvent()

#### Event System
A robust event system for handling various types of events:

- **Event Types**:
  - Window events (close, resize, focus, move)
  - Input events (keyboard, mouse)
  - Application events
  
- **Event Dispatcher**:
  - Type-safe event dispatch system
  - Callback-based event handling
  - Category-based event filtering

#### Input System
Abstracts input handling across the engine:

- Mouse input management (position, buttons, cursor modes)
- Keyboard input handling
- Integration with GLFW for platform-specific input

### Rendering System

The engine features a modern, flexible rendering system with support for both forward and deferred rendering pipelines.

#### Core Rendering Architecture

- **Renderer Class**:
  - Central rendering manager
  - Scene submission and processing
  - Camera and light uniform management
  - Frustum culling support
  - Bounding box visualization
  - Performance profiling integration

- **Command-Based Rendering**:
  - Command queue architecture for efficient rendering
  - Support for multiple command types:
    - RenderCommand: Basic mesh rendering
    - GeometryPassCommand: Deferred rendering geometry pass
    - LightingPassCommand: Deferred rendering lighting pass
    - PostProcessCommand: Post-processing effects
    - AnimationSetupCommand: Skeletal animation setup

- **Render Queue System**:
  - Thread-safe command queue implementation
  - Asynchronous queue building support
  - Command sorting and batching
  - Multiple queue types (Geometry, PostProcess, ShadowMap)

#### Deferred Rendering Pipeline

- **G-Buffer Implementation**:
  - Multiple render targets for deferred shading
  - Geometry pass for material properties
  - Lighting pass for final illumination
  - Support for multiple light types

- **Render Pipeline Stages**:
1. Scene Processing
   - Entity component extraction
   - Frustum culling
   - Command generation
2. Queue Building
   - Async command queue construction
   - Command sorting and optimization
3. Rendering Execution
   - Geometry pass execution
   - Lighting calculations
   - Post-processing effects

#### Performance Features

- **Optimization Techniques**:
  - Frustum culling for visibility determination
  - Command sorting for state changes minimization
  - Asynchronous command queue building
  - Batch processing of similar commands

- **Multi-threading Support**:
  - Parallel command queue construction
  - Thread pool for async operations
  - Thread-safe queue management

#### Rendering Utilities

- **Debug Visualization**:
  - Bounding box rendering
  - Debug line drawing
  - Primitive shape rendering (cubes, lines, quads)

- **Buffer Management**:
  - Uniform buffer optimization
  - Persistent buffer mapping
  - Cache-friendly data organization

#### Integration with External Systems

- **Tracy Profiler Integration**:
  - GPU and CPU performance markers
  - Frame timing analysis
  - Detailed rendering statistics

- **Material System Integration**:
  - Material library management
  - Shader program handling
  - Uniform state management

### Asset Management

The engine features a robust asset management system that handles various types of game assets with support for asynchronous loading and resource tracking.

#### Core Components

- **AssetManager**:
  - Central manager for all engine assets
  - Handles asset loading, caching, and lifecycle management
  - Provides type-safe asset access through templates
  - Maintains asset registry and loaded asset tracking

- **Asset Types**:
  - Mesh assets
  - Texture2D assets
  - Cubemap textures
  - Materials
  - Skeletons
  - Animations
  - Audio files
  - Scripts
  - Scenes
  - Fonts
  - Shaders

#### Asset System Features

- **Asset Identification**:
  - Unique UUID-based asset handles
  - Metadata tracking for each asset
  - File path and sub-asset indexing support
  - Asset type classification

- **Asset Loading**:
  - Asynchronous asset loading support
  - Type-specific asset importers
  - Variant-based asset storage
  - Error handling and validation

- **Asset Registry**:
  - Tracks all known assets
  - Maintains asset metadata
  - Provides asset lookup and querying
  - Handles asset dependencies

#### Specialized Asset Handlers

- **Texture Management**:
  - Asynchronous texture loading
  - Support for 2D textures and cubemaps
  - Texture library for resource sharing

- **Material System**:
  - PBR material support
  - Material library management
  - Shader program integration

- **Mesh Loading**:
  - Support for multiple mesh formats
  - Sub-mesh handling
  - Primitive batch loading

#### Asset Editor Integration

- **Editor Support**:
  - Asset browser functionality
  - Asset preview capabilities
  - Asset import/export tools
  - Asset modification tracking

#### Implementation Details

- **Resource Management**:
  - Reference counting through shared pointers
  - Automatic resource cleanup
  - Memory optimization
  - Asset pooling and caching

- **Error Handling**:
  - Robust error checking
  - Detailed error logging
  - Fallback mechanisms
  - Asset validation

### Scene Management

The engine uses a modern Entity-Component-System (ECS) architecture powered by EnTT for scene management and game object organization.

#### Core Components

- **Scene Class**:
  - Central manager for game world
  - Entity creation and destruction
  - Registry management (EnTT)
  - Scene settings and configuration
  - Skybox management

- **Entity System**:
  - Unique entity identification
  - Component management
  - Entity hierarchy support
  - Type-safe component access
  - Error handling and validation

#### Component System

- **Transform Components**:
  - Position, rotation, and scale
  - Matrix transformation
  - Quaternion support
  - Parent-child relationships

- **Rendering Components**:
  - Mesh components
  - Material components
  - Camera components
  - Light components
  - Sprite components

- **Animation Components**:
  - Skeletal animation support
  - Animation playback control
  - Multiple animation states
  - Skeleton management

- **Physics Components**:
  - Bounding box components
  - Collision detection
  - Physics properties

#### Entity Hierarchy

- **EntityNode System**:
  - Tree-based entity relationships
  - Parent-child transformations
  - Component inheritance
  - Dynamic hierarchy management

#### Scene Systems

- **Animation System**:
  - Animation component updates
  - Skeleton transformations
  - Animation state management
  - Time-based animation control

- **Bounding Box System**:
  - Automatic bounds calculation
  - Bounds updating
  - Intersection testing
  - Debug visualization

#### Scene Features

- **Scene Settings**:
  - Frustum culling configuration
  - Raycast debugging
  - Async rendering options
  - Performance settings

- **Scene Environment**:
  - Skybox management
  - Environmental lighting
  - Scene-wide effects
  - Background settings

#### Implementation Details

- **Component Management**:
  - Type-safe component access
  - Component addition/removal
  - Component queries and filters
  - Batch component operations

- **Performance Optimization**:
  - Efficient component storage
  - Fast entity queries
  - Minimal memory overhead
  - Cache-friendly data layout

### Input and Events

The engine features a comprehensive input and event system that handles user interaction and system events.

#### Unified Event System

The Rapture Engine provides a unified event system that combines two complementary approaches:

1. **Traditional Class-Based Events**: A hierarchy of event classes with type information for window, input, and application events.
2. **Generic Observer Pattern**: A flexible EventBus implementation for communication between engine subsystems.

##### Traditional Event System

The traditional event system uses class inheritance and virtual methods for type-safe event dispatching:

- **Event Base Class**:
  ```cpp
  class Event {
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
  ```

- **Event Types and Categories**:
  ```cpp
  enum class EventType {
      None = 0,
      
      // Window events
      WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
      
      // Keyboard events
      KeyPressed, KeyReleased, KeyTyped,
      
      // Mouse events
      MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,
      
      // Application events
      AppTick, AppUpdate, AppRender,
      
      // Custom engine events
      Custom
  };

  enum EventCategory {
      None = 0,
      EventCategoryApplication = 1 << 0,
      EventCategoryInput       = 1 << 1,
      EventCategoryKeyboard    = 1 << 2,
      EventCategoryMouse       = 1 << 3,
      EventCategoryMouseButton = 1 << 4
  };
  ```

- **Event Dispatcher**:
  ```cpp
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
  ```

- **Helper Macros**:
  ```cpp
  #define EVENT_CLASS_TYPE(type) static EventType getStaticType() { return EventType::type; } \
                                virtual EventType getEventType() const override { return getStaticType(); } \
                                virtual const char* getName() const override { return #type; }

  #define EVENT_CLASS_CATEGORY(category) virtual int getCategoryFlags() const override { return category; }
  ```

##### Generic Event System (EventBus)

The EventBus system implements the observer pattern with type-safe callbacks:

- **EventBus Template Class**:
  ```cpp
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
  ```

- **Event Registry**:
  ```cpp
  class EventRegistry {
  public:
      static EventRegistry& getInstance() {
          static EventRegistry instance;
          return instance;
      }
      
      // Get or create an event bus for the given name and types
      template<typename... Args>
      EventBus<Args...>& getEventBus(const std::string& name) {
          std::string typeId = typeid(EventBus<Args...>).name();

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
  };
  ```

##### Predefined Game Events

The engine provides predefined global events through the `GameEvents` namespace:

```cpp
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
    
    // Accessor functions
    inline SceneActivatedEvent& onSceneActivated() {
        return EventRegistry::getInstance().getEventBus<std::shared_ptr<Scene>>("SceneActivated");
    }
    
    inline WorldActivatedEvent& onWorldActivated() {
        return EventRegistry::getInstance().getEventBus<std::shared_ptr<World>>("WorldActivated");
    }
    
    // Additional accessors...
}
```

##### Using the Event System

**Traditional Event System Usage**:
```cpp
// Define a window resize event
class WindowResizeEvent : public Event {
public:
    WindowResizeEvent(unsigned int width, unsigned int height)
        : m_Width(width), m_Height(height) {}
        
    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
    
    unsigned int getWidth() const { return m_Width; }
    unsigned int getHeight() const { return m_Height; }
    
private:
    unsigned int m_Width, m_Height;
};

// Handling events
void onEvent(Event& event) {
    EventDispatcher dispatcher(event);
    
    // Dispatch event to handler function
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        // Handle window resize
        return true; // Mark as handled
    });
}
```

**EventBus System Usage**:
```cpp
// Register a listener for scene activation
size_t listenerId = GameEvents::onSceneActivated().addListener(
    [](std::shared_ptr<Scene> scene) {
        // Handle scene activation
        GE_INFO("Scene activated: {0}", scene->getSceneName());
    }
);

// Later, trigger the event
GameEvents::onSceneActivated().invoke(myScene);

// Clean up when done (important!)
GameEvents::onSceneActivated().removeListener(listenerId);
```

**Custom Event Bus**:
```cpp
// Define a custom event
struct PlayerDamageEvent {
    using Event = EventBus<Entity, float>;
    
    static Event& get() {
        return EventRegistry::getInstance().getEventBus<Entity, float>("PlayerDamage");
    }
};

// Usage
size_t damageListenerId = PlayerDamageEvent::get().addListener(
    [](Entity entity, float damage) {
        // Handle player damage
    }
);

// Trigger the event
PlayerDamageEvent::get().invoke(playerEntity, 25.0f);
```

##### Best Practices

1. **Always clean up listeners**: Store listener IDs and remove them when objects are destroyed.

2. **Prefer the EventBus system** for communication between subsystems.

3. **Use appropriate event granularity**: Don't create too many specialized events or too few general events.

4. **Keep handlers lightweight**: Event handlers should be fast to avoid blocking the event system.

5. **Avoid circular dependencies**: Be careful not to create circular event chains.

#### Core Event Architecture

- **Event Types**:
  - Window events (close, resize, focus, move)
  - Input events (keyboard, mouse)
  - Application events
  - Custom event support

- **Event Categories**:
  - Application events
  - Input events
  - Keyboard events
  - Mouse events
  - Mouse button events

- **Event Dispatcher**:
  - Type-based event routing
  - Event handling callbacks
  - Event propagation control
  - Event consumption tracking

#### Input System

- **Keyboard Input**:
  - Key state tracking
  - Key press detection
  - Key release detection
  - Modifier key support

- **Mouse Input**:
  - Mouse button states
  - Mouse position tracking
  - Mouse movement events
  - Scroll wheel support
  - Cursor mode control (normal/disabled)

#### Event Classes

- **Mouse Events**:
  - MouseButtonEvent (base)
  - MouseButtonPressedEvent
  - MouseButtonReleasedEvent
  - MouseMovedEvent
  - MouseScrolledEvent

- **Window Events**:
  - WindowCloseEvent
  - WindowResizeEvent
  - WindowFocusEvent
  - WindowMovedEvent

#### Implementation Details

- **Event Handling**:
  - Event propagation through layers
  - Event filtering by category
  - Event consumption tracking
  - Event debugging support

- **Input State Management**:
  - Real-time input state tracking
  - Input device abstraction
  - Platform-independent interface
  - GLFW integration

### Debug and Utilities

The engine provides comprehensive debugging and utility systems for development and optimization.

#### Profiling System

- **Tracy Integration**:
  - CPU and GPU performance profiling
  - Frame timing analysis
  - Memory allocation tracking
  - Thread profiling
  - Lock tracking
  - Plot visualization
  - Message logging

- **CPU Profiling**:
  - Function-level profiling
  - Scope-based timing
  - Thread naming
  - Memory allocation tracking
  - Performance statistics

- **GPU Profiling**:
  - OpenGL performance markers
  - GPU timing queries
  - Frame time tracking
  - GPU command profiling
  - Render pass analysis

#### Profiling Features

- **Performance Metrics**:
  - Frame time tracking
  - Average frame time
  - Min/Max frame times
  - FPS counter
  - Frame time history

- **Debug Visualization**:
  - ImGui integration
  - Performance graphs
  - Timing data display
  - Memory usage tracking
  - Thread activity visualization

#### Utility Systems

- **UUID Generation**:
  - High-precision timestamp-based UUIDs
  - Thread-safe generation
  - 64-bit unique identifiers
  - Timestamp (42 bits) + Random (22 bits)
  - ~139 years of unique timestamps
  - 4M+ values per nanosecond

- **Debug Macros**:
  - Conditional compilation
  - Debug/Release mode detection
  - Profiling enable/disable
  - Debug visualization toggles

#### Implementation Details

- **Profiling Implementation**:
  - RAII-based timing
  - Automatic scope profiling
  - Low-overhead measurements
  - Conditional compilation
  - Debug build detection

- **Performance Optimization**:
  - Minimal overhead in release builds
  - Efficient data collection
  - Thread-safe operations
  - Memory-efficient storage

## Editor
The editor is built on top of the engine and provides a comprehensive set of tools for game development:

### Core Editor Components

#### Test Layer
The main editor layer that handles:
- Scene management and editing
- Entity selection and manipulation
- Camera control and viewport rendering
- Debug visualization tools
- Integration with ImGui panels

#### ImGui Integration
The editor uses Dear ImGui for its user interface, with several specialized panels:

- **Entity Browser Panel**: Browse and manage scene entities
- **Properties Panel**: Edit entity properties
- **Viewport Panel**: Main scene view and interaction
- **Stats Panel**: Performance and debug statistics
- **Log Panel**: Engine and editor logging
- **Assets Panel**: Asset management interface
- **Settings Panel**: Editor and engine settings

The ImGui integration includes:
- Custom styling and theming
- ImGuizmo integration for scene manipulation
- Dockable panels and windows
- Real-time property editing

## External Dependencies
The engine utilizes several external libraries:

- **GLFW**: Window management and OpenGL context creation
- **Glad**: OpenGL function loading
- **Dear ImGui**: User interface
- **ImGuizmo**: 3D manipulation gizmos
- **spdlog**: Logging system
- **Tracy**: Profiling and performance analysis
- [Additional dependencies will be listed as discovered]
