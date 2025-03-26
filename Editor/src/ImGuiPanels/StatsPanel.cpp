#include "StatsPanel.h"
#include "Debug/Profiler.h"
#include "Debug/GPUProfiler.h"
#include <imgui.h>
#include <imgui_internal.h> // For advanced ImGui functions
#include <array>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <numeric> // For std::accumulate
#include <cmath> // For sin/cos

// Forward declare helper functions when needed
ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
    return ImVec2(a.x + b.x, a.y + b.y);
}

// Helper function to check if a string contains another string (case insensitive)
static bool containsIgnoreCase(const std::string& str, const std::string& substr) {
    if (substr.empty()) return true;
    
    std::string lowerStr = str;
    std::string lowerSubstr = substr;
    
    // Convert both strings to lowercase
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(lowerSubstr.begin(), lowerSubstr.end(), lowerSubstr.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    return lowerStr.find(lowerSubstr) != std::string::npos;
}

// Format bytes to human-readable string (KB, MB, GB)
static std::string formatMemory(size_t bytes) {
    std::stringstream ss;
    
    if (bytes < 1024) {
        ss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (bytes / 1024.0f) << " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0f * 1024.0f)) << " MB";
    } else {
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0f * 1024.0f * 1024.0f)) << " GB";
    }
    
    return ss.str();
}

void StatsPanel::render(float timestep) {
    // Update the timer
    m_updateTimer += timestep;
    
    // Update cached values if it's time
    if (m_updateTimer >= UPDATE_INTERVAL) {
        updateCachedData();
        m_updateTimer = 0.0f;
    }
    
    // Begin main panel
    ImGui::Begin("Engine Statistics", nullptr, ImGuiWindowFlags_MenuBar);
    
    // Optional menu bar for saving/loading profiles, settings, etc.
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Auto Scroll", nullptr, &ImGui::GetIO().ConfigFlags);
            ImGui::SliderFloat("Update Interval", &const_cast<float&>(UPDATE_INTERVAL), 0.1f, 2.0f, "%.1f s");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Create tabs at the top
    if (ImGui::BeginTabBar("StatsTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Overview")) {
            m_activeTab = TabType::Overview;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CPU")) {
            m_activeTab = TabType::CPU;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GPU")) {
            m_activeTab = TabType::GPU;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory")) {
            m_activeTab = TabType::Memory;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Rendering")) {
            m_activeTab = TabType::Rendering;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    // Render active tab content
    switch (m_activeTab) {
        case TabType::Overview: renderOverviewTab(); break;
        case TabType::CPU:      renderCPUTab();      break;
        case TabType::GPU:      renderGPUTab();      break;
        case TabType::Memory:   renderMemoryTab();   break;
        case TabType::Rendering: renderRenderingTab(); break;
    }
    
    // Add update rate indicator at the bottom
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing());
    ImGui::Text("Stats update every %.1f seconds", UPDATE_INTERVAL);
    ImGui::SameLine();
    ImGui::ProgressBar(m_updateTimer / UPDATE_INTERVAL, ImVec2(100, 0), "");
    
    ImGui::End();
}

void StatsPanel::renderOverviewTab() {
    // Performance overview section with most important metrics
    if (ImGui::CollapsingHeader("Performance Overview", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Use a 2-column layout for compact display
        ImGui::Columns(2, "overview_columns", false);
        
        // Frame time and FPS
        ImGui::Text("Current FPS:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedFPS, 60, 30, "%d", false);
        ImGui::NextColumn();
        
        ImGui::Text("CPU Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedCPUFrameTime, StatsPanel::FRAMETIME_WARNING_MS, StatsPanel::FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("GPU Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedGPUFrameTime, StatsPanel::FRAMETIME_WARNING_MS, StatsPanel::FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        // Rendering stats
        ImGui::Text("Draw Calls:");
        ImGui::NextColumn();
        renderColoredValue(static_cast<float>(m_drawCalls), DRAWCALL_WARNING, DRAWCALL_ERROR, "%d");
        ImGui::NextColumn();
        
        ImGui::Text("Triangle Count:");
        ImGui::NextColumn();
        ImGui::Text("%d", m_triangleCount);
        ImGui::NextColumn();
        
        // Memory usage
        ImGui::Text("Total Memory:");
        ImGui::NextColumn();
        float memoryMB = m_totalMemoryUsage / (1024.0f * 1024.0f);
        size_t otherMemory = m_totalMemoryUsage - m_textureMemoryUsage - m_meshMemoryUsage;
        renderColoredValue(memoryMB, MEMORY_WARNING_MB, MEMORY_ERROR_MB, "%s", true, formatMemory(m_totalMemoryUsage).c_str());
        ImGui::NextColumn();
        
        ImGui::Columns(1);
    }
    
    // Combined frame time graph
    if (ImGui::CollapsingHeader("Frame Time History", ImGuiTreeNodeFlags_DefaultOpen)) {
        float maxValue = 0.0f;
        
        // Find the maximum value in both histories for consistent scaling
        for (const auto& cpuTime : m_cachedCPUHistory) {
            maxValue = std::max(maxValue, cpuTime);
        }
        for (const auto& gpuTime : m_cachedGPUHistory) {
            maxValue = std::max(maxValue, gpuTime);
        }
        
        // Add headroom
        maxValue *= 1.2f;
        
        // Draw CPU and GPU frame time graphs
        ImGui::Text("CPU & GPU Frame Times");
        
        // Plot both CPU and GPU frame times
        const float GRAPH_HEIGHT = 120.0f;
        
        // Save the cursor position for the legend
        ImVec2 plotStartPos = ImGui::GetCursorScreenPos();
        
        // Plot CPU frame times
        ImGui::PlotLines("##cpuframetimes", 
                        m_cachedCPUHistory.data(), 
                        m_cachedCPUHistory.size(), 
                        0,          // values offset
                        nullptr,    // overlay text
                        0.0f,       // scale min
                        maxValue,   // scale max
                        ImVec2(-1, GRAPH_HEIGHT));
        
        // Save the plot rect for the legend
        ImVec2 plotSize = ImGui::GetItemRectSize();
        
        // Plot the GPU frame times on the same area
        ImGui::SetCursorScreenPos(plotStartPos);
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red for GPU
        ImGui::PlotLines("##gpuframetimes", 
                        m_cachedGPUHistory.data(), 
                        m_cachedGPUHistory.size(), 
                        0,          // values offset
                        nullptr,    // overlay text
                        0.0f,       // scale min
                        maxValue,   // scale max
                        ImVec2(-1, GRAPH_HEIGHT));
        ImGui::PopStyleColor();
        
        // Add a legend
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("CPU: %.2f ms", m_cachedCPUFrameTime);
        ImGui::SameLine(150);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("GPU: %.2f ms", m_cachedGPUFrameTime);
        ImGui::PopStyleColor();
        
        // Draw frame time breakdown
        renderFrameTimeBreakdown();
    }
    
    // Top performance issues - simple table with most expensive operations
    if (ImGui::CollapsingHeader("Performance Hotspots", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Show the top 5 most expensive operations
        if (ImGui::BeginTable("HotspotsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Operation");
            ImGui::TableSetupColumn("Time (ms)");
            ImGui::TableHeadersRow();
            
            const size_t showCount = std::min(m_cachedProfileData.size(), size_t(5));
            for (size_t i = 0; i < showCount; i++) {
                const auto& [name, data] = m_cachedProfileData[i];
                
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());
                
                ImGui::TableNextColumn();
                float duration = static_cast<float>(data.duration);
                if (duration > FRAMETIME_ERROR_MS / 2) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.1f, 0.1f, 1.0f)); // Red
                } else if (duration > FRAMETIME_WARNING_MS / 2) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.1f, 1.0f)); // Yellow
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.9f, 0.1f, 1.0f)); // Green
                }
                ImGui::Text("%.2f", duration);
                ImGui::PopStyleColor();
            }
            
            ImGui::EndTable();
        }
    }
}

void StatsPanel::renderCPUTab() {
    // Add search functionality at the top
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    
    bool searchChanged = ImGui::InputText("##ProfileSearch", m_searchBuffer, sizeof(m_searchBuffer));
    m_searchActive = m_searchBuffer[0] != '\0';
    
    if (searchChanged && m_searchActive) {
        // Filter the profile data based on the search term
        std::string searchTerm(m_searchBuffer);
        m_filteredProfileData.clear();
        
        for (const auto& profileEntry : m_cachedProfileData) {
            if (containsIgnoreCase(profileEntry.first, searchTerm)) {
                m_filteredProfileData.push_back(profileEntry);
            }
        }
    }
    
    ImGui::PopItemWidth();
    
    // CPU stats overview
    if (ImGui::CollapsingHeader("CPU Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Create a 2-column layout
        ImGui::Columns(2, "cpu_stats_columns", false);
        
        ImGui::Text("Current Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedCPUFrameTime, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("Average Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedAverageFrameTime, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("Min Frame Time:");
        ImGui::NextColumn();
        ImGui::Text("%.2f ms", m_cachedMinFrameTime);
        ImGui::NextColumn();
        
        ImGui::Text("Max Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedMaxFrameTime, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("Frames Per Second:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedFPS, 60, 30, "%d", false);
        ImGui::NextColumn();
        
        ImGui::Columns(1);
    }
    
    // CPU frame time history
    if (ImGui::CollapsingHeader("CPU History", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderHistoryGraph(m_cachedCPUHistory, "CPU Frame Time", m_cachedMaxFrameTime * 1.2f);
    }
    
    // Full profile data table
    if (ImGui::CollapsingHeader("CPU Profiling Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& displayData = m_searchActive ? m_filteredProfileData : m_cachedProfileData;
        
        if (displayData.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                m_searchActive ? "No matching profile data" : "No profiling data available");
        } else {
            // Create a table for the profile data
            renderTimingTable(displayData, "CPUProfileTable");
        }
    }
    
    // Detailed timeline for top components
    if (ImGui::CollapsingHeader("Component Timelines", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& displayData = m_searchActive ? m_filteredProfileData : m_cachedProfileData;
        
        if (displayData.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                m_searchActive ? "No matching profile data" : "No profiling data available");
        } else {
            // Find the maximum value for consistent scaling
            float maxProfileValue = 0.0f;
            const size_t numToShow = std::min(displayData.size(), size_t(8));
            
            for (size_t i = 0; i < numToShow; i++) {
                const auto& componentName = displayData[i].first;
                const auto& history = m_componentHistories[componentName];
                
                for (const auto& value : history) {
                    maxProfileValue = std::max(maxProfileValue, value);
                }
            }
            
            // Add headroom to max value
            maxProfileValue *= 1.2f;
            
            // Display component graphs
            for (size_t i = 0; i < numToShow; i++) {
                const auto& componentName = displayData[i].first;
                float currentValue = static_cast<float>(displayData[i].second.duration);
                
                renderTimingGraph(componentName, m_componentHistories[componentName], 
                                 currentValue, maxProfileValue);
            }
        }
    }
}

void StatsPanel::renderGPUTab() {
    // GPU stats overview
    if (ImGui::CollapsingHeader("GPU Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Create a 2-column layout
        ImGui::Columns(2, "gpu_stats_columns", false);
        
        ImGui::Text("GPU Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_cachedGPUFrameTime, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("GPU/CPU Ratio:");
        ImGui::NextColumn();
        float gpuCpuRatio = m_cachedCPUFrameTime > 0 ? (m_cachedGPUFrameTime / m_cachedCPUFrameTime) : 0;
        ImGui::Text("%.2f", gpuCpuRatio);
        if (gpuCpuRatio > 0.8f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 1.0f), "(GPU bound)");
        }
        ImGui::NextColumn();
        
        ImGui::Columns(1);
    }
    
    // GPU frame time history
    if (ImGui::CollapsingHeader("GPU History", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderHistoryGraph(m_cachedGPUHistory, "GPU Frame Time", m_cachedGPUFrameTime * 1.5f);
    }
    
    // GPU operations breakdown
    if (ImGui::CollapsingHeader("GPU Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_gpuTimings.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No GPU timing data available");
        } else {
            if (ImGui::BeginTable("GPUTimingTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Operation");
                ImGui::TableSetupColumn("Time (ms)");
                ImGui::TableHeadersRow();
                
                for (const auto& [name, duration] : m_gpuTimings) {
                    ImGui::TableNextRow();
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", name.c_str());
                    
                    ImGui::TableNextColumn();
                    if (duration > FRAMETIME_ERROR_MS / 2) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.1f, 0.1f, 1.0f)); // Red
                    } else if (duration > FRAMETIME_WARNING_MS / 2) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.1f, 1.0f)); // Yellow
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.9f, 0.1f, 1.0f)); // Green
                    }
                    ImGui::Text("%.2f", duration);
                    ImGui::PopStyleColor();
                }
                
                ImGui::EndTable();
            }
        }
    }
}

void StatsPanel::renderMemoryTab() {
    // Calculate other memory once for all sections
    size_t otherMemory = m_totalMemoryUsage - m_textureMemoryUsage - m_meshMemoryUsage;
    
    // Memory overview
    if (ImGui::CollapsingHeader("Memory Usage", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Create a 2-column layout
        ImGui::Columns(2, "memory_stats_columns", false);
        
        ImGui::Text("Total Memory:");
        ImGui::NextColumn();
        float memoryMB = m_totalMemoryUsage / (1024.0f * 1024.0f);
        renderColoredValue(memoryMB, MEMORY_WARNING_MB, MEMORY_ERROR_MB, "%s", true, formatMemory(m_totalMemoryUsage).c_str());
        ImGui::NextColumn();
        
        ImGui::Text("Texture Memory:");
        ImGui::NextColumn();
        ImGui::Text("%s", formatMemory(m_textureMemoryUsage).c_str());
        ImGui::NextColumn();
        
        ImGui::Text("Mesh Memory:");
        ImGui::NextColumn();
        ImGui::Text("%s", formatMemory(m_meshMemoryUsage).c_str());
        ImGui::NextColumn();
        
        ImGui::Text("Other:");
        ImGui::NextColumn();
        ImGui::Text("%s", formatMemory(otherMemory).c_str());
        ImGui::NextColumn();
        
        ImGui::Columns(1);
    }
    
    // Memory breakdown visualization
    if (ImGui::CollapsingHeader("Memory Breakdown", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Draw a stacked bar chart showing memory breakdown
        const float BAR_HEIGHT = 24.0f;
        const float AVAIL_WIDTH = ImGui::GetContentRegionAvail().x;
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        
        // Calculate proportions
        float textureRatio = static_cast<float>(m_textureMemoryUsage) / m_totalMemoryUsage;
        float meshRatio = static_cast<float>(m_meshMemoryUsage) / m_totalMemoryUsage;
        float otherRatio = 1.0f - textureRatio - meshRatio;
        
        // Draw the main bar background
        drawList->AddRectFilled(
            startPos,
            ImVec2(startPos.x + AVAIL_WIDTH, startPos.y + BAR_HEIGHT),
            IM_COL32(60, 60, 60, 255));
        
        // Draw texture memory segment
        float textureWidth = AVAIL_WIDTH * textureRatio;
        drawList->AddRectFilled(
            startPos,
            ImVec2(startPos.x + textureWidth, startPos.y + BAR_HEIGHT),
            IM_COL32(230, 126, 34, 255)); // Orange
        
        // Draw mesh memory segment
        float meshWidth = AVAIL_WIDTH * meshRatio;
        drawList->AddRectFilled(
            ImVec2(startPos.x + textureWidth, startPos.y),
            ImVec2(startPos.x + textureWidth + meshWidth, startPos.y + BAR_HEIGHT),
            IM_COL32(41, 128, 185, 255)); // Blue
        
        // Draw other memory segment
        float otherWidth = AVAIL_WIDTH * otherRatio;
        drawList->AddRectFilled(
            ImVec2(startPos.x + textureWidth + meshWidth, startPos.y),
            ImVec2(startPos.x + textureWidth + meshWidth + otherWidth, startPos.y + BAR_HEIGHT),
            IM_COL32(46, 204, 113, 255)); // Green
        
        ImGui::Dummy(ImVec2(0, BAR_HEIGHT + 8));
        
        // Add a legend
        float legendTextHeight = ImGui::GetTextLineHeight();
        float legendBoxSize = legendTextHeight;
        float legendSpacing = 8.0f;
        
        // Legend for texture memory
        ImVec2 legendPos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(
            legendPos,
            ImVec2(legendPos.x + legendBoxSize, legendPos.y + legendBoxSize),
            IM_COL32(230, 126, 34, 255)); // Orange
        ImGui::Dummy(ImVec2(legendBoxSize, legendBoxSize));
        ImGui::SameLine();
        ImGui::Text("Textures: %s (%.1f%%)", formatMemory(m_textureMemoryUsage).c_str(), textureRatio * 100.0f);
        
        // Legend for mesh memory
        legendPos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(
            legendPos,
            ImVec2(legendPos.x + legendBoxSize, legendPos.y + legendBoxSize),
            IM_COL32(41, 128, 185, 255)); // Blue
        ImGui::Dummy(ImVec2(legendBoxSize, legendBoxSize));
        ImGui::SameLine();
        ImGui::Text("Meshes: %s (%.1f%%)", formatMemory(m_meshMemoryUsage).c_str(), meshRatio * 100.0f);
        
        // Legend for other memory
        legendPos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(
            legendPos,
            ImVec2(legendPos.x + legendBoxSize, legendPos.y + legendBoxSize),
            IM_COL32(46, 204, 113, 255)); // Green
        ImGui::Dummy(ImVec2(legendBoxSize, legendBoxSize));
        ImGui::SameLine();
        ImGui::Text("Other: %s (%.1f%%)", formatMemory(otherMemory).c_str(), otherRatio * 100.0f);
    }
}

void StatsPanel::renderRenderingTab() {
    // Rendering overview
    if (ImGui::CollapsingHeader("Rendering Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Create a 2-column layout
        ImGui::Columns(2, "render_stats_columns", false);
        
        ImGui::Text("Draw Calls:");
        ImGui::NextColumn();
        renderColoredValue(static_cast<float>(m_drawCalls), DRAWCALL_WARNING, DRAWCALL_ERROR, "%d");
        ImGui::NextColumn();
        
        ImGui::Text("Triangle Count:");
        ImGui::NextColumn();
        ImGui::Text("%d", m_triangleCount);
        ImGui::NextColumn();
        
        ImGui::Text("Batch Count:");
        ImGui::NextColumn();
        ImGui::Text("%d", m_batchCount);
        ImGui::NextColumn();
        
        ImGui::Text("Shader Binds:");
        ImGui::NextColumn();
        ImGui::Text("%d", m_shaderBinds);
        ImGui::NextColumn();
        
        ImGui::Columns(1);
    }
    
    // Frustum culling visualization would go here 
    // Pipeline state visualization would go here
}

void StatsPanel::renderHistoryGraph(const std::array<float, 100>& history, const char* label, float maxValue) {
    // Find the maximum value for scaling if not provided
    if (maxValue <= 0.0f) {
        for (const auto& value : history) {
            maxValue = std::max(maxValue, value);
        }
        maxValue *= 1.2f; // Add headroom
    }
    
    // Draw colored lines for thresholds
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Plot the frame times with a specified height
    const float GRAPH_HEIGHT = 80.0f;
    ImVec2 plotSize = ImVec2(-1, GRAPH_HEIGHT);
    
    // Save the cursor position and plot size for overlays
    ImVec2 plotStart = ImGui::GetCursorScreenPos();
    float plotWidth = ImGui::GetContentRegionAvail().x;
    
    ImGui::PlotLines("##historygraph", 
                    history.data(), 
                    history.size(), 
                    0,                  // values offset
                    nullptr,            // overlay text
                    0.0f,               // scale min
                    maxValue,           // scale max
                    plotSize);
    
    ImVec2 plotEnd = ImVec2(plotStart.x + plotWidth, plotStart.y + GRAPH_HEIGHT);
    
    // Draw warning threshold line
    if (FRAMETIME_WARNING_MS < maxValue) {
        float warningY = plotStart.y + GRAPH_HEIGHT * (1.0f - FRAMETIME_WARNING_MS / maxValue);
        drawList->AddLine(
            ImVec2(plotStart.x, warningY),
            ImVec2(plotEnd.x, warningY),
            IM_COL32(255, 180, 0, 128), // Yellow
            1.0f
        );
    }
    
    // Draw error threshold line
    if (FRAMETIME_ERROR_MS < maxValue) {
        float errorY = plotStart.y + GRAPH_HEIGHT * (1.0f - FRAMETIME_ERROR_MS / maxValue);
        drawList->AddLine(
            ImVec2(plotStart.x, errorY),
            ImVec2(plotEnd.x, errorY),
            IM_COL32(255, 0, 0, 128), // Red
            1.0f
        );
    }
    
    // Add labels for thresholds
    ImGui::Dummy(ImVec2(0, 5));
    
    // Calculate average manually instead of using std::accumulate
    float sum = 0.0f;
    for (const auto& value : history) {
        sum += value;
    }
    float average = sum / static_cast<float>(history.size());
    
    ImGui::Text("%s Average: %.2f ms", label, average);
    
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Warning: %.1f ms", FRAMETIME_WARNING_MS);
    
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.8f);
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Critical: %.1f ms", FRAMETIME_ERROR_MS);
}

void StatsPanel::renderTimingTable(const std::vector<std::pair<std::string, Rapture::ProfileTimingData>>& data,
                                  const char* title, bool hierarchical) {
    // Add search bar at the top of the table
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    bool searchChanged = ImGui::InputText("##ProfileSearch", m_searchBuffer, sizeof(m_searchBuffer));
    m_searchActive = m_searchBuffer[0] != '\0';
    ImGui::PopItemWidth();
    
    // Filter data based on search if active
    const auto& displayData = m_searchActive ? m_filteredProfileData : data;
    
    if (ImGui::BeginTable(title, 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Component/Operation");
        ImGui::TableSetupColumn("Total Time (ms)");
        ImGui::TableSetupColumn("Calls");
        ImGui::TableSetupColumn("Avg Time/Call (ms)");
        ImGui::TableHeadersRow();
        

        
        // Then render unpinned components
        for (const auto& [name, timing] : displayData) {
            if (m_pinnedComponents.find(name) == m_pinnedComponents.end()) {
                renderTimingRow(std::make_pair(name, timing), false);
            }
        }
        
        ImGui::EndTable();
    }
}

void StatsPanel::renderTimingRow(const std::pair<std::string, Rapture::ProfileTimingData>& row, bool isPinned) {
    ImGui::TableNextRow();
    

    // Component name column
    ImGui::TableNextColumn();
    ImGui::Text("%s", row.first.c_str());
    
    // Total time column
    ImGui::TableNextColumn();
    float totalTime = static_cast<float>(row.second.totalTime);
    if (totalTime > FRAMETIME_ERROR_MS / 2) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.1f, 0.1f, 1.0f)); // Red
    } else if (totalTime > FRAMETIME_WARNING_MS / 2) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.1f, 1.0f)); // Yellow
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.9f, 0.1f, 1.0f)); // Green
    }
    ImGui::Text("%.3f", totalTime);
    ImGui::PopStyleColor();
    
    // Call count column
    ImGui::TableNextColumn();
    ImGui::Text("%d", row.second.callCount);
    
    // Average time per call column
    ImGui::TableNextColumn();
    float avgTime = static_cast<float>(row.second.averageTime);
    if (avgTime > FRAMETIME_ERROR_MS / 2) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.1f, 0.1f, 1.0f)); // Red
    } else if (avgTime > FRAMETIME_WARNING_MS / 2) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.1f, 1.0f)); // Yellow
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.9f, 0.1f, 1.0f)); // Green
    }
    ImGui::Text("%.3f", avgTime);
    ImGui::PopStyleColor();
}

void StatsPanel::renderTimingGraph(const std::string& name, const std::array<float, 30>& history,
                                 float currentValue, float maxValue) {
    // Create a label with the current value
    std::string label = name + " (" + std::to_string(currentValue) + " ms)";
    
    // Color the graph based on the current value
    ImVec4 graphColor;
    if (currentValue > FRAMETIME_ERROR_MS / 2) {
        graphColor = ImVec4(0.9f, 0.1f, 0.1f, 1.0f); // Red
    } else if (currentValue > FRAMETIME_WARNING_MS / 2) {
        graphColor = ImVec4(0.9f, 0.6f, 0.1f, 1.0f); // Yellow
    } else {
        graphColor = ImVec4(0.1f, 0.9f, 0.1f, 1.0f); // Green
    }
    
    ImGui::PushStyleColor(ImGuiCol_PlotLines, graphColor);
    ImGui::PlotLines(("##" + name).c_str(), 
                    history.data(), 
                    history.size(), 
                    0,               // values offset
                    label.c_str(),   // overlay text
                    0.0f,            // scale min
                    maxValue,        // scale max
                    ImVec2(0, 40));  // graph size
    ImGui::PopStyleColor();
}

void StatsPanel::renderFrameTimeBreakdown() {
    // Create a pie chart showing CPU vs GPU time
    const float PIE_RADIUS = 60.0f;
    const ImVec2 pieCenter = ImGui::GetCursorScreenPos() + ImVec2(PIE_RADIUS + 10.0f, PIE_RADIUS + 10.0f);
    
    // Calculate total frame time and percentages
    float totalTime = m_cachedCPUFrameTime + m_cachedGPUFrameTime;
    float cpuPercent = totalTime > 0 ? (m_cachedCPUFrameTime / totalTime) : 0.5f;
    float gpuPercent = 1.0f - cpuPercent;
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Draw a full circle background
    drawList->AddCircleFilled(pieCenter, PIE_RADIUS, IM_COL32(60, 60, 60, 255));
    
    // Draw CPU segment
    const int NUM_SEGMENTS = 40;
    float startAngle = 0.0f;
    float endAngle = 2.0f * IM_PI * cpuPercent;
    
    drawList->PathClear();
    drawList->PathLineTo(pieCenter);
    
    for (int i = 0; i <= NUM_SEGMENTS; i++) {
        float angle = startAngle + (endAngle - startAngle) * i / static_cast<float>(NUM_SEGMENTS);
        drawList->PathLineTo(pieCenter + ImVec2(cosf(angle) * PIE_RADIUS, sinf(angle) * PIE_RADIUS));
    }
    
    drawList->PathFillConvex(IM_COL32(52, 152, 219, 255)); // Blue for CPU
    
    // Draw GPU segment
    startAngle = endAngle;
    endAngle = 2.0f * IM_PI;
    
    drawList->PathClear();
    drawList->PathLineTo(pieCenter);
    
    for (int i = 0; i <= NUM_SEGMENTS; i++) {
        float angle = startAngle + (endAngle - startAngle) * i / static_cast<float>(NUM_SEGMENTS);
        drawList->PathLineTo(pieCenter + ImVec2(cosf(angle) * PIE_RADIUS, sinf(angle) * PIE_RADIUS));
    }
    
    drawList->PathFillConvex(IM_COL32(231, 76, 60, 255)); // Red for GPU
    
    // Leave space for the pie chart
    ImGui::Dummy(ImVec2(0, PIE_RADIUS * 2 + 20));
    
    // Add a legend
    ImGui::Columns(2, "pie_legend", false);
    
    ImGui::SetCursorScreenPos(ImVec2(pieCenter.x + PIE_RADIUS + 20, pieCenter.y - PIE_RADIUS));
    
    const float BOX_SIZE = 16.0f;
    ImVec2 legendPos = ImGui::GetCursorScreenPos();
    
    // CPU legend
    drawList->AddRectFilled(
        legendPos,
        ImVec2(legendPos.x + BOX_SIZE, legendPos.y + BOX_SIZE),
        IM_COL32(52, 152, 219, 255)); // Blue
    
    ImGui::Dummy(ImVec2(BOX_SIZE, BOX_SIZE));
    ImGui::SameLine();
    ImGui::Text("CPU: %.2f ms (%.1f%%)", m_cachedCPUFrameTime, cpuPercent * 100.0f);
    
    // GPU legend
    legendPos = ImVec2(legendPos.x, legendPos.y + ImGui::GetTextLineHeightWithSpacing() + 4.0f);
    drawList->AddRectFilled(
        legendPos,
        ImVec2(legendPos.x + BOX_SIZE, legendPos.y + BOX_SIZE),
        IM_COL32(231, 76, 60, 255)); // Red
    
    ImGui::SetCursorScreenPos(legendPos);
    ImGui::Dummy(ImVec2(BOX_SIZE, BOX_SIZE));
    ImGui::SameLine();
    ImGui::Text("GPU: %.2f ms (%.1f%%)", m_cachedGPUFrameTime, gpuPercent * 100.0f);
    
    ImGui::Columns(1);
}

void StatsPanel::renderColoredValue(float value, float warningThreshold, float errorThreshold, 
                                  const char* format, bool lowerIsBetter, const char* customText) {
    ImVec4 color;
    
    if (lowerIsBetter) {
        if (value > errorThreshold) {
            color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f); // Red
        } else if (value > warningThreshold) {
            color = ImVec4(0.9f, 0.6f, 0.1f, 1.0f); // Yellow
        } else {
            color = ImVec4(0.1f, 0.9f, 0.1f, 1.0f); // Green
        }
    } else {
        if (value < errorThreshold) {
            color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f); // Red
        } else if (value < warningThreshold) {
            color = ImVec4(0.9f, 0.6f, 0.1f, 1.0f); // Yellow
        } else {
            color = ImVec4(0.1f, 0.9f, 0.1f, 1.0f); // Green
        }
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    if (customText)
        ImGui::Text("%s", customText);
    else
        ImGui::Text(format, value);
    ImGui::PopStyleColor();
}

void StatsPanel::updateCachedData() {
    // Update CPU profiler stats
    m_cachedCPUFrameTime = Rapture::Profiler::getLastFrameTime();
    m_cachedAverageFrameTime = Rapture::Profiler::getAverageFrameTime();
    m_cachedMinFrameTime = Rapture::Profiler::getMinFrameTime();
    m_cachedMaxFrameTime = Rapture::Profiler::getMaxFrameTime();
    m_cachedFPS = Rapture::Profiler::getFramesPerSecond();
    
    // Update GPU profiler stats
    m_cachedGPUFrameTime = Rapture::GPUProfiler::getLastGPUTime();
    
    // Copy frame time histories
    m_cachedCPUHistory = Rapture::Profiler::getFrameTimeHistory();
    m_cachedGPUHistory = Rapture::GPUProfiler::getGPUTimeHistory();
    
    // Update component profiling data
    const auto& profilingData = Rapture::Profiler::getProfilingData();
    m_cachedProfileData.clear();
    for (const auto& [name, data] : profilingData) {
        m_cachedProfileData.push_back(std::make_pair(name, data));
        
        // Update component history
        auto& history = m_componentHistories[name];
        
        // Shift values to make room for the new one
        for (size_t i = history.size() - 1; i > 0; --i) {
            history[i] = history[i-1];
        }
        history[0] = static_cast<float>(data.duration);
    }
    
    // Sort by duration (descending)
    std::sort(m_cachedProfileData.begin(), m_cachedProfileData.end(), 
        [](const auto& a, const auto& b) {
            return a.second.duration > b.second.duration;
        });
        
    // Update filtered data if search is active
    if (m_searchActive) {
        std::string searchTerm(m_searchBuffer);
        m_filteredProfileData.clear();
        
        for (const auto& profileEntry : m_cachedProfileData) {
            if (containsIgnoreCase(profileEntry.first, searchTerm)) {
                m_filteredProfileData.push_back(profileEntry);
            }
        }
    }
    
    // Update GPU timing data
    const auto& gpuTimingData = Rapture::GPUProfiler::getTimingData();
    m_gpuTimings.clear();
    for (const auto& [name, data] : gpuTimingData) {
        m_gpuTimings.push_back(std::make_pair(name, data.duration));
    }

    // Sort GPU timings by duration
    std::sort(m_gpuTimings.begin(), m_gpuTimings.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
    // TODO: Update memory and rendering stats
    // These would come from your engine's statistics tracking system
    // For now, we'll use placeholder data
    m_drawCalls = 1250;  // Example value
    m_triangleCount = 250000;  // Example value
    m_batchCount = 120;  // Example value
    m_shaderBinds = 85;  // Example value
    
    m_totalMemoryUsage = 768 * 1024 * 1024;  // Example: 768 MB
    m_textureMemoryUsage = 384 * 1024 * 1024;  // Example: 384 MB
    m_meshMemoryUsage = 256 * 1024 * 1024;  // Example: 256 MB
}

