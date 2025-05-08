#include "TracyProfiler.h"
#include "../Logger/Log.h"
#include <glad/glad.h>  // Add direct include of glad

namespace Rapture {

bool TracyProfiler::s_initialized = false;
bool TracyProfiler::s_gpuInitialized = false;

void TracyProfiler::init() {
    if (s_initialized) {
        return;
    }
    
    #if RAPTURE_TRACY_PROFILING_ENABLED
        // Set the main thread name
        tracy::SetThreadName("Main Thread");
        
        // Log initialization
        GE_CORE_INFO("Tracy Profiler initialized");
        
        // Try to initialize GPU context - will work only if OpenGL is already initialized
        initGPUContext();
    #else
        GE_CORE_WARN("Tracy Profiler is disabled. Build with RAPTURE_TRACY_PROFILING_ENABLED=1 to enable.");
    #endif
    
    s_initialized = true;
}

void TracyProfiler::initGPUContext() {
    #if RAPTURE_TRACY_PROFILING_ENABLED
        if (s_gpuInitialized) {
            return;
        }
        
        // Verify OpenGL is initialized
        if (gladLoadGL == nullptr || glGenQueries == nullptr) {
            GE_CORE_WARN("Tracy GPU profiling: OpenGL not initialized yet! Call initGPUContext after OpenGL is initialized.");
            return;
        }
        
        // Initialize the GPU context for Tracy
        try {
            TracyGpuContext;
            s_gpuInitialized = true;
            GE_CORE_INFO("Tracy GPU profiling initialized successfully");
        } catch (const std::exception& e) {
            GE_CORE_ERROR("Tracy GPU profiling initialization failed: {0}", e.what());
        }
    #endif
}

void TracyProfiler::shutdown() {
    if (!s_initialized) {
        return;
    }
    
    #if RAPTURE_TRACY_PROFILING_ENABLED
        // No explicit shutdown needed for Tracy
        GE_CORE_INFO("Tracy Profiler shutdown");
    #endif
    
    s_initialized = false;
}

void TracyProfiler::beginFrame() {
    #if RAPTURE_TRACY_PROFILING_ENABLED
        // Mark the beginning of a new frame
        FrameMark;
    #endif
}

void TracyProfiler::endFrame() {
    // Tracy automatically handles frame boundaries with FrameMark
    // No additional work needed here
}

} // namespace Rapture 