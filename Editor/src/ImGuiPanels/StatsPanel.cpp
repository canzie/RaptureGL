#include "StatsPanel.h"
#include "Debug/Profiler.h"
#include "Debug/GPUProfiler.h"
#include <array>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>

// Static variables for cached display values
static float s_cachedLastFrameTime = 0.0f;
static float s_cachedAverageFrameTime = 0.0f;
static float s_cachedMinFrameTime = 0.0f;
static float s_cachedMaxFrameTime = 0.0f;
static int s_cachedFPS = 0;
static float s_cachedGPUTime = 0.0f;
static std::vector<std::pair<std::string, Rapture::ProfileTimingData>> s_cachedProfileData;
static std::array<float, 100> s_cachedCPUHistory = {};
static std::array<float, 100> s_cachedGPUHistory = {};

// Timer for updating display values
static float s_updateTimer = 0.0f;
static const float UPDATE_INTERVAL = 1.0f; // Update once per second

// Map to store component timing history (component name -> circular buffer of timings)
static std::unordered_map<std::string, std::array<float, 30>> s_componentHistories;

void StatsPanel::render(float timestep) {
    // Update the timer
    s_updateTimer += timestep;
    
    // Update cached values if it's time
    if (s_updateTimer >= UPDATE_INTERVAL) {
        s_updateTimer = 0.0f;
        
        // Update CPU profiler stats
        s_cachedLastFrameTime = Rapture::Profiler::getLastFrameTime();
        s_cachedAverageFrameTime = Rapture::Profiler::getAverageFrameTime();
        s_cachedMinFrameTime = Rapture::Profiler::getMinFrameTime();
        s_cachedMaxFrameTime = Rapture::Profiler::getMaxFrameTime();
        s_cachedFPS = Rapture::Profiler::getFramesPerSecond();
        
        // Update GPU profiler stats
        s_cachedGPUTime = Rapture::GPUProfiler::getLastGPUTime();
        
        // Copy frame time histories
        s_cachedCPUHistory = Rapture::Profiler::getFrameTimeHistory();
        s_cachedGPUHistory = Rapture::GPUProfiler::getGPUTimeHistory();
        
        // Update component profiling data
        const auto& profilingData = Rapture::Profiler::getProfilingData();
        s_cachedProfileData.clear();
        for (const auto& [name, data] : profilingData) {
            s_cachedProfileData.push_back(std::make_pair(name, data));
            
            // Update component history
            auto& history = s_componentHistories[name];
            
            // Shift values to make room for the new one
            for (size_t i = history.size() - 1; i > 0; --i) {
                history[i] = history[i-1];
            }
            history[0] = static_cast<float>(data.duration);
        }
        
        // Sort by duration (descending)
        std::sort(s_cachedProfileData.begin(), s_cachedProfileData.end(), 
            [](const auto& a, const auto& b) {
                return a.second.duration > b.second.duration;
            });
    }
    
    // Start ImGui panel
    ImGui::Begin("Statistics");
    
    // Use cached values for all stats (including basic stats)
    ImGui::Text("FPS: %d", s_cachedFPS);
    ImGui::Text("Frame Time: %.3f ms", s_cachedLastFrameTime);
    
    ImGui::Separator();
    
    // CPU Profiler stats (using cached values)
    ImGui::Text("CPU Profiler Statistics");
    ImGui::Text("Last Frame Time: %.3f ms", s_cachedLastFrameTime);
    ImGui::Text("Average Frame Time: %.3f ms", s_cachedAverageFrameTime);
    ImGui::Text("Min Frame Time: %.3f ms", s_cachedMinFrameTime);
    ImGui::Text("Max Frame Time: %.3f ms", s_cachedMaxFrameTime);
    ImGui::Text("FPS: %d", s_cachedFPS);
    
    // Frame time history graph
    ImGui::Separator();
    ImGui::Text("CPU Frame Time History");
    
    // Find the maximum value for scaling
    float maxValue = 0.0f;
    for (const auto& frametime : s_cachedCPUHistory) {
        maxValue = std::max(maxValue, frametime);
    }
    
    // Add a little headroom to the max value
    maxValue = maxValue * 1.2f;
    
    // Plot the frame times
    ImGui::PlotLines("##cpuframetimes", 
                    s_cachedCPUHistory.data(), 
                    s_cachedCPUHistory.size(), 
                    0,                      // values offset
                    "ms",                  // overlay text
                    0.0f,                  // scale min
                    maxValue,              // scale max
                    ImVec2(0, 80));        // graph size
    
    // GPU Profiler stats
    ImGui::Separator();
    ImGui::Text("GPU Profiler Statistics");
    ImGui::Text("Last GPU Frame Time: %.3f ms", s_cachedGPUTime);
    
    // GPU frame time history graph
    ImGui::Text("GPU Frame Time History");
    
    // Find the maximum value for scaling
    float maxGpuValue = 0.0f;
    for (const auto& gpuTime : s_cachedGPUHistory) {
        maxGpuValue = std::max(maxGpuValue, gpuTime);
    }
    
    // Add a little headroom to the max value
    maxGpuValue = maxGpuValue * 1.2f;
    
    // Plot the GPU frame times
    ImGui::PlotLines("##gpuframetimes", 
                    s_cachedGPUHistory.data(), 
                    s_cachedGPUHistory.size(), 
                    0,                     // values offset
                    "ms",                 // overlay text
                    0.0f,                 // scale min
                    maxGpuValue,          // scale max
                    ImVec2(0, 80));       // graph size
    
    // Component-specific profiling data
    ImGui::Separator();
    ImGui::Text("Component Profiling");
    
    if (s_cachedProfileData.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No profiling data available");
    } else {
        // Create a simple table to display the profiling data
        if (ImGui::BeginTable("ProfileTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Component/Operation");
            ImGui::TableSetupColumn("Duration (ms)");
            ImGui::TableHeadersRow();
            
            for (const auto& [name, data] : s_cachedProfileData) {
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());
                
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", data.duration);
            }
            
            ImGui::EndTable();
        }
        
        // Add component graphs for the top components
        ImGui::Separator();
        ImGui::Text("Top Component Graphs");
        
        // Take only top 5 or fewer to avoid cluttering the display
        const size_t numToShow = std::min(s_cachedProfileData.size(), size_t(5));
        
        // Determine the maximum value across all top components for consistent scaling
        float maxProfileValue = 0.0f;
        for (size_t i = 0; i < numToShow; i++) {
            const auto& componentName = s_cachedProfileData[i].first;
            const auto& history = s_componentHistories[componentName];
            
            for (const auto& value : history) {
                maxProfileValue = std::max(maxProfileValue, value);
            }
        }
        
        // Add headroom to max value
        maxProfileValue *= 1.2f;
        
        // Display component graphs
        for (size_t i = 0; i < numToShow; i++) {
            const auto& componentName = s_cachedProfileData[i].first;
            float currentValue = static_cast<float>(s_cachedProfileData[i].second.duration);
            
            // Create a label with the current value
            std::string label = componentName + " (" + std::to_string(currentValue) + " ms)";
            
            // Get history for this component
            const auto& history = s_componentHistories[componentName];
            
            // Create a temporary array of floats for the plot
            std::array<float, 30> plotValues;
            for (size_t j = 0; j < plotValues.size(); j++) {
                plotValues[j] = history[j];
            }
            
            // Display the graph
            ImGui::PlotLines(("##" + componentName).c_str(), 
                            plotValues.data(), 
                            plotValues.size(), 
                            0,                     // values offset
                            label.c_str(),         // overlay text
                            0.0f,                  // scale min
                            maxProfileValue,       // scale max
                            ImVec2(0, 50));        // graph size
        }
    }
    
    // Add update rate indicator
    ImGui::Separator();
    ImGui::Text("Stats update every %.1f seconds", UPDATE_INTERVAL);
    ImGui::ProgressBar(s_updateTimer / UPDATE_INTERVAL, ImVec2(-1, 0), "Next update");
    
    ImGui::End();
}

