/*
#include "GPUProfiler.h"
#include "../Logger/Log.h"
#include <GLFW/glfw3.h>

namespace Rapture {

// Initialize static variables
bool GPUProfiler::s_Initialized = false;
std::array<float, GPUProfiler::s_GPUTimeHistoryCount> GPUProfiler::s_GPUTimeHistory = {};
size_t GPUProfiler::s_GPUTimeHistoryIndex = 0;
float GPUProfiler::s_LastGPUTime = 0.0f;
std::unordered_map<std::string, GPUTimingData> GPUProfiler::s_TimingData;
std::vector<std::string> GPUProfiler::s_PendingResults;

void GPUProfiler::init() {
    // Skip initialization if GL functions might not be available
    GLenum err = glGetError(); // This will work if GL is initialized
    
    s_GPUTimeHistory.fill(0.0f);
    s_LastGPUTime = 0.0f;
    s_GPUTimeHistoryIndex = 0;
    
    // Create test query to verify GL timer queries are supported
    GLuint testQuery = 0;
    glGenQueries(1, &testQuery);
    
    if (testQuery == 0) {
        GE_CORE_ERROR("GPU Profiler: Timer queries not supported, GPU profiling disabled");
        return;
    }
    
    glDeleteQueries(1, &testQuery);
    s_Initialized = true;
    
    GE_CORE_INFO("GPU Profiler: Initialized successfully");
}

void GPUProfiler::shutdown() {
    if (!s_Initialized) return;
    
    // Clean up any outstanding queries
    for (auto& [name, timing] : s_TimingData) {
        if (timing.queryIDs[0] != 0) {
            glDeleteQueries(2, timing.queryIDs);
            timing.queryIDs[0] = 0;
            timing.queryIDs[1] = 0;
        }
    }
    s_TimingData.clear();
    s_PendingResults.clear();
    
    s_Initialized = false;
}

void GPUProfiler::beginFrame() {
    if (!s_Initialized) return;
    
    // Set up the frame timer
    beginGPUTimer("Frame");
}

void GPUProfiler::endFrame() {
    if (!s_Initialized) return;
    
    // End the frame timer
    endGPUTimer("Frame");
    
    // Process results from the last frame
    collectResults();
}

void GPUProfiler::beginGPUTimer(const std::string& name) {
    if (!s_Initialized) return;
    
    // Get or create timing data for this section
    auto& timing = getOrCreateTimingData(name);
    
    // Skip if already active
    if (timing.active) return;
    
    // Create query objects if needed
    if (timing.queryIDs[0] == 0) {
        glGenQueries(2, timing.queryIDs);
    }
    
    // Start timing
    glQueryCounter(timing.queryIDs[0], GL_TIMESTAMP);
    timing.active = true;
}

void GPUProfiler::endGPUTimer(const std::string& name) {
    if (!s_Initialized) return;
    
    // Find the timing data
    auto it = s_TimingData.find(name);
    if (it == s_TimingData.end() || !it->second.active) return;
    
    // End timing
    glQueryCounter(it->second.queryIDs[1], GL_TIMESTAMP);
    it->second.active = false;
    
    // Add to list of pending results
    s_PendingResults.push_back(name);
}

void GPUProfiler::collectResults() {
    if (!s_Initialized) return;
    
    // Process all pending results
    for (auto it = s_PendingResults.begin(); it != s_PendingResults.end();) {
        auto& timing = s_TimingData[*it];
        
        // Check if results are available
        GLint startReady = 0, endReady = 0;
        glGetQueryObjectiv(timing.queryIDs[0], GL_QUERY_RESULT_AVAILABLE, &startReady);
        glGetQueryObjectiv(timing.queryIDs[1], GL_QUERY_RESULT_AVAILABLE, &endReady);
        
        if (startReady && endReady) {
            // Get query results
            GLuint64 startTime, endTime;
            glGetQueryObjectui64v(timing.queryIDs[0], GL_QUERY_RESULT, &startTime);
            glGetQueryObjectui64v(timing.queryIDs[1], GL_QUERY_RESULT, &endTime);
            
            // Calculate duration in milliseconds
            timing.duration = static_cast<float>(endTime - startTime) / 1000000.0f;
            
            // Update frame time if this is the frame timer
            if (*it == "Frame") {
                s_LastGPUTime = timing.duration;
                s_GPUTimeHistory[s_GPUTimeHistoryIndex] = s_LastGPUTime;
                s_GPUTimeHistoryIndex = (s_GPUTimeHistoryIndex + 1) % s_GPUTimeHistoryCount;
            }
            
            // Remove from pending list
            it = s_PendingResults.erase(it);
        } else {
            ++it;
        }
    }
}

float GPUProfiler::getLastGPUTime() {
    return s_LastGPUTime;
}

const std::array<float, 100>& GPUProfiler::getGPUTimeHistory() {
    return s_GPUTimeHistory;
}

const std::unordered_map<std::string, GPUTimingData>& GPUProfiler::getTimingData() {
    return s_TimingData;
}

GPUTimingData& GPUProfiler::getOrCreateTimingData(const std::string& name) {
    auto it = s_TimingData.find(name);
    if (it == s_TimingData.end()) {
        GPUTimingData data;
        data.name = name;
        auto result = s_TimingData.emplace(name, data);
        return result.first->second;
    }
    return it->second;
}

// GPUProfilerTimer implementation
GPUProfilerTimer::GPUProfilerTimer(const std::string& name) : m_Name(name) {
    GPUProfiler::beginGPUTimer(m_Name);
}

GPUProfilerTimer::~GPUProfilerTimer() {
    GPUProfiler::endGPUTimer(m_Name);
}

} // namespace Rapture 

*/