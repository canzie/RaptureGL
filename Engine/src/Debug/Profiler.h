#pragma once

#include "TracyProfiler.h"

/*
#pragma once

#include <string>
#include <array>
#include <chrono>
#include <unordered_map>
#include "GPUProfiler.h"

// Profiling toggle flag
#define RAPTURE_PROFILING_ENABLED 1

// Common profiler definitions that work in both debug and release builds
#if RAPTURE_PROFILING_ENABLED
    #define RAPTURE_PROFILE_FUNCTION() Rapture::ProfilerTimer timer##__LINE__(__FUNCTION__)
    #define RAPTURE_PROFILE_SCOPE(name) Rapture::ProfilerTimer timer##__LINE__(name)
    #define RAPTURE_PROFILE_FRAME()
    #define RAPTURE_PROFILE_THREAD(name)
    // GPU profiling macros
    #define RAPTURE_PROFILE_GPU_SCOPE(name) Rapture::GPUProfilerTimer gpu_timer##__LINE__(name)
    #define RAPTURE_PROFILE_GPU_COLLECT() Rapture::GPUProfiler::collectResults()
#else
    #define RAPTURE_PROFILE_FUNCTION() 
    #define RAPTURE_PROFILE_SCOPE(name) 
    #define RAPTURE_PROFILE_FRAME()
    #define RAPTURE_PROFILE_THREAD(name)
    #define RAPTURE_PROFILE_GPU_SCOPE(name) 
    #define RAPTURE_PROFILE_GPU_COLLECT() 
#endif

namespace Rapture {

// Forward declarations for profiling data
struct ProfileTimingData {
    std::string name;
    double startTime;
    double endTime;
    double duration;      // Duration of the last call
    double totalTime;     // Total time across all calls in this frame
    uint32_t callCount;   // Number of times this scope was called in this frame
    double averageTime;   // Average time per call
};

class Profiler {
public:
    static void init();
    static void shutdown();
    
    // ImGui stats integration
    static void beginFrame();
    static void endFrame();
    
    // Get performance stats for ImGui display
    static float getLastFrameTime();
    static float getAverageFrameTime();
    static float getMinFrameTime();
    static float getMaxFrameTime();
    static int getFramesPerSecond();
    
    // Store recent frame times for a graph
    static const std::array<float, 100>& getFrameTimeHistory();
    
    // Debug mode check that can be called from code
    static bool isDebugBuild() {
    #if defined(RAPTURE_DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
        return true;
    #else
        return false;
    #endif
    }
    
    // Profiling API
    static void beginTimedSection(const std::string& name);
    static void endTimedSection(const std::string& name);
    
    // Get profiling data for each component
    static const std::unordered_map<std::string, ProfileTimingData>& getProfilingData();

private:
    static void updateStats(float frameTime);
    
    static constexpr size_t s_FrameTimeHistoryCount = 100;
    static std::array<float, s_FrameTimeHistoryCount> s_FrameTimeHistory;
    static size_t s_FrameTimeHistoryIndex;
    
    static float s_LastFrameTime;
    static float s_MinFrameTime;
    static float s_MaxFrameTime;
    static float s_AccumulatedFrameTime;
    static int s_FrameCount;
    static float s_AverageFrameTime;
    static float s_LastStatsUpdateTime;
    static int s_FramesPerSecond;
    
    // Component-specific profiling data
    static std::unordered_map<std::string, ProfileTimingData> s_ProfilingData;
};

// RAII Timer class for automatic profiling
class ProfilerTimer {
public:
    ProfilerTimer(const std::string& name) : m_Name(name) {
        m_StartTime = std::chrono::high_resolution_clock::now();
        Profiler::beginTimedSection(m_Name);
    }
    
    ~ProfilerTimer() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_StartTime).count() / 1000.0; // Convert to ms
        Profiler::endTimedSection(m_Name);
    }
    
private:
    std::string m_Name;
    std::chrono::high_resolution_clock::time_point m_StartTime;
};

} // namespace Rapture 

*/