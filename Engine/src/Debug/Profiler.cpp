/*
#include "Profiler.h"
#include "../Logger/Log.h"
#include "../Timestep/Timestep.h"
#include <algorithm>
#include <numeric>
#include <limits>
#include <chrono>

namespace Rapture {

// Initialize static members
std::array<float, Profiler::s_FrameTimeHistoryCount> Profiler::s_FrameTimeHistory = {};
size_t Profiler::s_FrameTimeHistoryIndex = 0;
float Profiler::s_LastFrameTime = 0.0f;
float Profiler::s_MinFrameTime = std::numeric_limits<float>::max();
float Profiler::s_MaxFrameTime = 0.0f;
float Profiler::s_AccumulatedFrameTime = 0.0f;
int Profiler::s_FrameCount = 0;
float Profiler::s_AverageFrameTime = 0.0f;
float Profiler::s_LastStatsUpdateTime = 0.0f;
int Profiler::s_FramesPerSecond = 0;
std::unordered_map<std::string, ProfileTimingData> Profiler::s_ProfilingData = {};

void Profiler::init()
{
    GE_CORE_INFO("Initializing Profiler");
    
    // Initialize frame time history with zeros
    s_FrameTimeHistory.fill(0.0f);
    
    // Clear profiling data
    s_ProfilingData.clear();
    
    GE_CORE_INFO("Profiler enabled in {} mode", isDebugBuild() ? "debug" : "release");
}

void Profiler::shutdown()
{
    GE_CORE_INFO("Shutting down Profiler");
    s_ProfilingData.clear();
}

void Profiler::beginFrame()
{
    // Clear per-frame profiling data at the start of each frame
    s_ProfilingData.clear();
}

void Profiler::endFrame()
{
    // Calculate frame time (in milliseconds)
    float frameTime = (float)Timestep::deltaTimeMs().count();
    
    // Update statistics
    updateStats(frameTime);
}

void Profiler::updateStats(float frameTime)
{
    // Store current frame time
    s_LastFrameTime = frameTime;
    
    // Update min/max frame times
    s_MinFrameTime = std::min(s_MinFrameTime, frameTime);
    s_MaxFrameTime = std::max(s_MaxFrameTime, frameTime);
    
    // Add to frame history
    s_FrameTimeHistory[s_FrameTimeHistoryIndex] = frameTime;
    s_FrameTimeHistoryIndex = (s_FrameTimeHistoryIndex + 1) % s_FrameTimeHistoryCount;
    
    // Update accumulated stats
    s_AccumulatedFrameTime += frameTime;
    s_FrameCount++;
    
    // Calculate FPS once per second
    float currentTime = (float)Timestep::getMilliSeconds().count() / 1000.0f; // Convert to seconds
    if (currentTime - s_LastStatsUpdateTime >= 1.0f) {
        s_AverageFrameTime = s_AccumulatedFrameTime / s_FrameCount;
        s_FramesPerSecond = s_FrameCount;
        
        // Reset accumulators
        s_AccumulatedFrameTime = 0.0f;
        s_FrameCount = 0;
        s_LastStatsUpdateTime = currentTime;
    }
}

void Profiler::beginTimedSection(const std::string& name)
{
    // Create or update timing data
    auto& data = s_ProfilingData[name];
    data.name = name;
    data.startTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count() / 1000.0; // Convert to milliseconds
    
    // Initialize or increment call count
    if (data.callCount == 0) {
        data.totalTime = 0.0;
        data.averageTime = 0.0;
    }
    data.callCount++;
}

void Profiler::endTimedSection(const std::string& name)
{
    auto it = s_ProfilingData.find(name);
    if (it != s_ProfilingData.end()) {
        auto& data = it->second;
        data.endTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count() / 1000.0; // Convert to milliseconds
        
        // Calculate duration of this call
        data.duration = data.endTime - data.startTime;
        
        // Update total time and average
        data.totalTime += data.duration;
        data.averageTime = data.totalTime / data.callCount;
    }
}

const std::unordered_map<std::string, ProfileTimingData>& Profiler::getProfilingData()
{
    return s_ProfilingData;
}

float Profiler::getLastFrameTime()
{
    return s_LastFrameTime;
}

float Profiler::getAverageFrameTime()
{
    return s_AverageFrameTime;
}

float Profiler::getMinFrameTime()
{
    return s_MinFrameTime;
}

float Profiler::getMaxFrameTime()
{
    return s_MaxFrameTime;
}

int Profiler::getFramesPerSecond()
{
    return s_FramesPerSecond;
}

const std::array<float, Profiler::s_FrameTimeHistoryCount>& Profiler::getFrameTimeHistory()
{
    return s_FrameTimeHistory;
}

} // namespace Rapture 

*/