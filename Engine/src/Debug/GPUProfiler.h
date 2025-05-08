/*
#pragma once

// Include OpenGL headers
#include <glad/glad.h>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Rapture {

// Structure to hold GPU timing data
struct GPUTimingData {
    std::string name;
    float duration = 0.0f;
    GLuint queryIDs[2] = {0, 0}; // Start and end query objects
    bool active = false;
};

class GPUProfiler {
public:
    static void init();
    static void shutdown();
    
    static void beginFrame();
    static void endFrame();
    
    static void beginGPUTimer(const std::string& name);
    static void endGPUTimer(const std::string& name);
    static void collectResults();
    
    static float getLastGPUTime();
    static const std::array<float, 100>& getGPUTimeHistory();
    static const std::unordered_map<std::string, GPUTimingData>& getTimingData();

private:
    // Helper to get or create timing data for a section
    static GPUTimingData& getOrCreateTimingData(const std::string& name);
    
    // Static variables for GPU timing
    static bool s_Initialized;
    
    static constexpr size_t s_GPUTimeHistoryCount = 100;
    static std::array<float, s_GPUTimeHistoryCount> s_GPUTimeHistory;
    static size_t s_GPUTimeHistoryIndex;
    
    static float s_LastGPUTime;
    
    static std::unordered_map<std::string, GPUTimingData> s_TimingData;
    static std::vector<std::string> s_PendingResults;
};

// RAII Timer for automatic GPU profiling
class GPUProfilerTimer {
public:
    GPUProfilerTimer(const std::string& name);
    ~GPUProfilerTimer();
    
private:
    std::string m_Name;
};

} // namespace Rapture 

*/