# Profiling System

This directory contains the profiling system for the Rapture Engine. The profiling system helps identify performance bottlenecks and optimize application performance.

## Tracy Profiler

The engine now uses [Tracy](https://github.com/wolfpld/tracy), a real-time, frame profiler for games and other applications. Tracy provides detailed visualizations of performance data, including:

- Per-frame timing information
- Multi-threading analysis
- Lock contention visualization
- GPU profiling
- Memory allocation tracking
- Value plotting over time

### Installation

To use Tracy, you need to:

1. Include the Tracy library in your project dependencies
2. Build Tracy server from the Tracy repository or download a pre-built binary
3. Enable Tracy in your builds via the `RAPTURE_TRACY_PROFILING_ENABLED` macro

Tracy is automatically enabled in debug builds, but can be manually enabled in release builds as well.

### Usage

Tracy provides a set of macros for instrumenting your code:

```cpp
// Profile function duration
void myFunction() {
    RAPTURE_PROFILE_FUNCTION();  // Auto-captures function name
    
    // Function code...
}

// Profile specific code blocks
void complexFunction() {
    {
        RAPTURE_PROFILE_SCOPE("Initialization Phase");
        // Initialization code...
    }
    
    {
        RAPTURE_PROFILE_SCOPE("Processing Phase");
        // Processing code...
    }
}

// Name threads for better visibility
void workerThreadFunction() {
    RAPTURE_PROFILE_THREAD("Worker Thread");
    // Thread code...
}

// Profile GPU operations
void renderFunction() {
    RAPTURE_PROFILE_GPU_SCOPE("Shadow Pass");
    // GPU rendering code...
}

// Track lock contention
RAPTURE_PROFILE_LOCKABLE(std::mutex, m_mutex);  // Declare a profiled mutex

void threadSafeFunction() {
    std::lock_guard<RAPTURE_PROFILE_LOCKABLE(std::mutex, m_mutex)> lock(m_mutex);
    // Thread-safe code...
}

// Plot values over time
void updatePhysics() {
    RAPTURE_PROFILE_PLOT("Active Physics Objects", m_activeObjectCount);
    // Physics code...
}
```

### Viewing Profile Data

To view the profile data:

1. Run the Tracy server application
2. Run your application with Tracy enabled
3. The Tracy server will automatically connect to your application
4. Use the Tracy UI to analyze performance data

Alternatively, you can use the built-in Tracy tab in the Engine Statistics panel, which provides a simplified view of the profiling data directly in the application.

## Legacy Profilers

The original CPU and GPU profilers (`Profiler` and `GPUProfiler`) are still available for backward compatibility but will eventually be deprecated in favor of Tracy.

## Integration

To integrate profiling in your code:

1. Include the appropriate header files:
   ```cpp
   #include "Debug/TracyProfiler.h"  // For Tracy profiling
   ```

2. Add profiling macros to the code sections you want to profile

3. View the results in the Engine Statistics panel or Tracy server

## Usage

### Basic Profiling

The profiler is automatically enabled in debug builds (`RAPTURE_DEBUG`). The profiling macros will compile to nothing in release builds.

Use these macros in your code to add profiling:

```cpp
// Profile a function (automatically captures function name)
void myFunction() {
    RAPTURE_PROFILE_FUNCTION();
    
    // Function code here
}

// Profile a specific scope with custom name
void complexFunction() {
    // This whole function is profiled
    RAPTURE_PROFILE_FUNCTION();
    
    {
        // This inner block has its own profile section
        RAPTURE_PROFILE_SCOPE("Expensive Calculation");
        
        // Complex calculation code here
    }
    
    // Back to function level profiling
}
```

### Thread Naming

```cpp
// Name your threads for better readability in the profiler
std::thread myThread([]() {
    // Set thread name at the start
    RAPTURE_PROFILE_THREAD("Physics Thread");
    
    // Thread code here
});
```

### Frame Markers

The main application loop automatically adds frame markers, but if you want to add them in custom loops:

```cpp
while (running) {
    RAPTURE_PROFILE_FRAME();
    
    // Frame code
}
```

### GPU Profiling

For GPU profiling zones:

```cpp
void renderFunction() {
    RAPTURE_PROFILE_FUNCTION(); // CPU profiling
    
    {
        RAPTURE_PROFILE_GPU_SCOPE("Shadow Pass"); // GPU profiling
        
        // GPU-intensive shadow rendering code
    }
}
```

## Viewing Profiling Results

There are two ways to view profiling data:

### 1. In-Game ImGui Stats Panel

Basic profiling statistics are displayed in the Engine's Stats Panel in ImGui, showing:
- Current/Min/Max/Average frame times
- FPS counter
- Frame time history graph

### 2. Tracy Profiler Application (Detailed Analysis)

For detailed profiling with full timeline, callstacks, and GPU profiling:

1. Run the application in debug mode
2. Launch the Tracy profiler application (download from https://github.com/wolfpld/tracy/releases)
3. Connect to your running application through Tracy's UI

## Tips for Effective Profiling

1. Start with high-level profiling (entire functions) and narrow down to specific scopes
2. Look for:
   - Long running functions
   - Functions called frequently 
   - Functions with inconsistent timing
3. Profile both CPU and GPU operations to find potential bottlenecks
4. Compare performance between debug and release builds
5. Use profiling data to guide optimization efforts

## Implementation Details 

If you need to access profiler data from code, use these methods:

```cpp
// Get performance metrics
float lastFrameMs = Profiler::getLastFrameTime();
float avgFrameMs = Profiler::getAverageFrameTime();
float minFrameMs = Profiler::getMinFrameTime();
float maxFrameMs = Profiler::getMaxFrameTime();
int fps = Profiler::getFramesPerSecond();

// Get frame time history for plotting
const auto& frameHistory = Profiler::getFrameTimeHistory();
``` 