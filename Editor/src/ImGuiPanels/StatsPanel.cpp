#include "StatsPanel.h"
//#include "Debug/Profiler.h"
//#include "Debug/GPUProfiler.h"
#include "Debug/TracyProfiler.h"
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

// Tracy include is already handled in TracyProfiler.h, no need to include it here
// The actual Tracy ImGui integration will be conditionally compiled
#if RAPTURE_TRACY_PROFILING_ENABLED
    // This will be available once Tracy is properly linked
    // Using forward declaration to avoid direct dependency
    namespace tracy { class View; }
#endif

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
    
    // Calculate and update FPS and frame time
    m_lastFrameTimeMs = timestep * 1000.0f;
    m_fps = m_lastFrameTimeMs > 0 ? int(1000.0f / m_lastFrameTimeMs) : 0;
    
    // Add frame time to history
    if (m_updateTimer >= UPDATE_INTERVAL) {
        updateCachedData();
        m_updateTimer = 0.0f;
    }
    
    // Begin main panel
    ImGui::Begin("Engine Statistics", nullptr, ImGuiWindowFlags_MenuBar);
    
    // Optional menu bar for settings, etc.
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
        if (ImGui::BeginTabItem("Tracy")) {
            m_activeTab = TabType::Tracy;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    // Render active tab content
    switch (m_activeTab) {
        case TabType::Overview: renderOverviewTab(); break;
        case TabType::Tracy:    renderTracyTab();    break;
    }
    
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
        renderColoredValue(m_fps, 60, 30, "%d", false);
        ImGui::NextColumn();
        
        ImGui::Text("Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_lastFrameTimeMs, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("Min Frame Time:");
        ImGui::NextColumn();
        ImGui::Text("%.2f ms", m_minFrameTimeMs);
        ImGui::NextColumn();
        
        ImGui::Text("Max Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_maxFrameTimeMs, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        ImGui::Text("Average Frame Time:");
        ImGui::NextColumn();
        renderColoredValue(m_avgFrameTimeMs, FRAMETIME_WARNING_MS, FRAMETIME_ERROR_MS, "%.2f ms");
        ImGui::NextColumn();
        
        // Basic memory usage
        ImGui::Text("Total Memory:");
        ImGui::NextColumn();
        float memoryMB = m_totalMemoryUsage / (1024.0f * 1024.0f);
        renderColoredValue(memoryMB, MEMORY_WARNING_MB, MEMORY_ERROR_MB, "%s", true, formatMemory(m_totalMemoryUsage).c_str());
        ImGui::NextColumn();
        
        ImGui::Columns(1);
    }
    
    // Frame time history
    if (ImGui::CollapsingHeader("Frame Time History", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Adjust max value for the graph
        float maxValue = m_maxFrameTimeMs * 1.2f;
        if (maxValue < FRAMETIME_WARNING_MS) maxValue = FRAMETIME_WARNING_MS * 1.5f;
        
        // Display frame time history graph
        renderHistoryGraph(m_frameTimeHistory, "Frame Time", maxValue);
    }
    
    // Tracy status
    if (ImGui::CollapsingHeader("Tracy Profiler Status", ImGuiTreeNodeFlags_DefaultOpen)) {
        m_tracyEnabled = Rapture::TracyProfiler::isEnabled();
        
        ImGui::Text("Tracy Profiler:");
        ImGui::SameLine();
        
        if (m_tracyEnabled) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Enabled");
            ImGui::Text("For detailed profiling information, go to the Tracy tab");
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "Disabled");
            ImGui::TextWrapped("Tracy is disabled in this build. To enable Tracy, rebuild with RAPTURE_TRACY_PROFILING_ENABLED=1.");
        }
    }
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
    
    // Calculate average manually
    float sum = 0.0f;
    int validCount = 0;
    for (const auto& value : history) {
        if (value > 0.0f) {
            sum += value;
            validCount++;
        }
    }
    float average = validCount > 0 ? sum / static_cast<float>(validCount) : 0.0f;
    
    ImGui::Text("%s Average: %.2f ms", label, average);
    
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Warning: %.1f ms", FRAMETIME_WARNING_MS);
    
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.8f);
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Critical: %.1f ms", FRAMETIME_ERROR_MS);
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

void StatsPanel::renderTracyTab() {
    // Check if Tracy is enabled
    m_tracyEnabled = Rapture::TracyProfiler::isEnabled();
    
    if (!m_tracyEnabled) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
            "Tracy profiling is not enabled in this build.");
        ImGui::TextWrapped(
            "To enable Tracy, rebuild with RAPTURE_TRACY_PROFILING_ENABLED=1 or in debug mode. "
            "Tracy provides advanced profiling capabilities including multi-threading analysis, "
            "lock contention visualization, and detailed timeline views.");
        return;
    }
    
    // Tracy server connection status
    renderTracyServerStatus();
    
    // Tracy control panel
    if (ImGui::CollapsingHeader("Tracy Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderTracyControlPanel();
    }
    
    // Tracy embedded view (when possible)
    if (ImGui::CollapsingHeader("Tracy Timeline", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderTracyEmbeddedView();
    }
    
    // Tracy usage information
    if (ImGui::CollapsingHeader("Tracy Usage Guide")) {
        ImGui::TextWrapped(
            "Tracy is a real-time frame profiler that helps identify performance bottlenecks "
            "in your application. Here are some tips for using Tracy effectively:");
        
        ImGui::BulletText("Use RAPTURE_PROFILE_FUNCTION() to profile entire functions");
        ImGui::BulletText("Use RAPTURE_PROFILE_SCOPE(\"Name\") to profile specific code blocks");
        ImGui::BulletText("Use RAPTURE_PROFILE_THREAD(\"Name\") to name threads for better visibility");
        ImGui::BulletText("Use RAPTURE_PROFILE_GPU_SCOPE(\"Name\") to profile GPU operations");
        ImGui::BulletText("Use RAPTURE_PROFILE_LOCKABLE() to track mutex contention");
        ImGui::BulletText("Use RAPTURE_PROFILE_PLOT() to plot numerical values over time");
        
        ImGui::Separator();
        
        ImGui::TextWrapped(
            "The Tracy profiler server provides a more comprehensive view of performance data. "
            "You can launch it separately and connect to this application for detailed analysis.");
    }
}

void StatsPanel::renderTracyServerStatus() {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Tracy Profiling Active");
    
    #if RAPTURE_TRACY_PROFILING_ENABLED
        // In a real implementation, we'd check if Tracy is connected to a server
        // For now we'll just show it's available
        ImGui::Text("Tracy Server: ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.7f, 0.2f, 1.0f), "Available");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextWrapped(
                "Tracy server connection status. For best results, launch the Tracy server "
                "application before running your application. You can download it from "
                "https://github.com/wolfpld/tracy");
            ImGui::EndTooltip();
        }
    #else
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Tracy integration disabled in this build");
    #endif
}

void StatsPanel::renderTracyControlPanel() {
    #if RAPTURE_TRACY_PROFILING_ENABLED
        // These controls would actually connect to Tracy's API in a real implementation
        // This is simplified for demonstration
        ImGui::Checkbox("Enable Frame Markers", &m_tracyConnected);
        
        static int plotUpdate = 1;
        ImGui::SliderInt("Plot Update Frequency", &plotUpdate, 1, 100);
        
        static int historyLength = 20;
        ImGui::SliderInt("History Length (seconds)", &historyLength, 5, 60);
        
        if (ImGui::Button("Capture CPU Trace")) {
            // Would trigger a Tracy capture
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear History")) {
            // Would clear Tracy history
        }
    #else
        ImGui::TextDisabled("Tracy controls unavailable (profiling disabled)");
    #endif
}

void StatsPanel::renderTracyEmbeddedView() {
    #if RAPTURE_TRACY_PROFILING_ENABLED
        // In a real implementation with Tracy properly integrated:
        // 1. Create a Tracy::View instance if not already created
        // 2. Call the tracy::View::Draw() method to render the Tracy UI within ImGui
        
        // For now, just inform the user how to use Tracy
        ImGui::TextWrapped(
            "Tracy timeline integration is available but requires the standalone Tracy Profiler application "
            "for full functionality. Run the Tracy Profiler application while your application is running "
            "to see detailed performance data.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), 
            "Advanced Tracy visualization requires the standalone Tracy Profiler application");
        ImGui::TextWrapped(
            "The Tracy Profiler can be downloaded from https://github.com/wolfpld/tracy/releases");
        
        // Still draw our placeholder visualization as a preview
        // Adjust the size as needed based on available space
        m_tracyViewSize.x = ImGui::GetContentRegionAvail().x;
        
        // This blank space represents where the Tracy view would be embedded
        ImGui::BeginChild("TracyView", m_tracyViewSize, true);
        ImGui::Text("Tracy Timeline Preview");
        
        // Draw a placeholder timeline-like visualization
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetCursorScreenPos();
        ImVec2 winSize = ImGui::GetContentRegionAvail();
        
        const float timelineHeight = 20.0f;
        const float startY = winPos.y + 30.0f;
        float currentY = startY;
        
        // Draw timeline threads
        for (int i = 0; i < 5; i++) {
            ImVec2 threadStart = ImVec2(winPos.x + 10, currentY);
            ImVec2 threadEnd = ImVec2(winPos.x + winSize.x - 10, currentY);
            
            // Draw thread line
            drawList->AddLine(threadStart, threadEnd, IM_COL32(150, 150, 150, 255), 1.0f);
            
            // Draw some sample spans
            float spanStart = threadStart.x + 20.0f * i;
            for (int j = 0; j < 10; j++) {
                float spanWidth = 30.0f + (rand() % 100);
                ImVec2 spanRectMin = ImVec2(spanStart, currentY - timelineHeight/2.0f);
                ImVec2 spanRectMax = ImVec2(spanStart + spanWidth, currentY + timelineHeight/2.0f);
                
                ImU32 spanColor = IM_COL32(
                    50 + (j * 20) % 205, 
                    100 + (i * 30) % 155, 
                    150 + (i + j) % 105, 
                    255);
                
                drawList->AddRectFilled(spanRectMin, spanRectMax, spanColor);
                drawList->AddRect(spanRectMin, spanRectMax, IM_COL32(0, 0, 0, 200));
                
                spanStart += spanWidth + 5.0f;
                if (spanStart > threadEnd.x - 50.0f) break;
            }
            
            // Draw thread name
            char threadName[32];
            sprintf(threadName, "Thread %d", i);
            drawList->AddText(ImVec2(winPos.x + 5, currentY - timelineHeight/2.0f), 
                             IM_COL32(200, 200, 200, 255), threadName);
            
            currentY += timelineHeight * 1.5f;
        }
        
        ImGui::Dummy(ImVec2(0, currentY - startY + 50.0f));
        ImGui::EndChild();
        
        // Add some basic controls that would be available in the real Tracy UI
        if (ImGui::Button("Connect to Profiler")) { 
            // In real implementation: would launch connection to Tracy Profiler
        }
        ImGui::SameLine();
        if (ImGui::Button("Export Trace")) { 
            // In real implementation: would export current capture data
        }
    #else
        ImGui::TextWrapped("Tracy timeline visualization is not available in this build.");
        ImGui::TextWrapped("Rebuild with RAPTURE_TRACY_PROFILING_ENABLED=1 to enable this feature.");
    #endif
}

void StatsPanel::updateCachedData() {
    // Update frame time stats using a simple rolling average
    static float frameTimes[30] = {0};
    static int frameIndex = 0;
    
    // Update rolling frame time history
    frameTimes[frameIndex] = m_lastFrameTimeMs;
    frameIndex = (frameIndex + 1) % 30;
    
    // Calculate min, max, avg
    m_minFrameTimeMs = frameTimes[0];
    m_maxFrameTimeMs = frameTimes[0];
    float sum = 0.0f;
    int count = 0;
    
    for (int i = 0; i < 30; i++) {
        if (frameTimes[i] > 0.0f) {
            m_minFrameTimeMs = std::min(m_minFrameTimeMs, frameTimes[i]);
            m_maxFrameTimeMs = std::max(m_maxFrameTimeMs, frameTimes[i]);
            sum += frameTimes[i];
            count++;
        }
    }
    
    m_avgFrameTimeMs = count > 0 ? sum / count : 0.0f;
    
    // Update frame time history
    m_frameTimeHistory[m_frameTimeHistoryIndex] = m_lastFrameTimeMs;
    m_frameTimeHistoryIndex = (m_frameTimeHistoryIndex + 1) % m_frameTimeHistory.size();
    
    // Use placeholder values for memory stats, etc. since we don't have the old profilers
    // In a real implementation, these would be retrieved from actual system APIs
    m_totalMemoryUsage = 768 * 1024 * 1024;  // Example: 768 MB
    m_textureMemoryUsage = 384 * 1024 * 1024;  // Example: 384 MB
    m_meshMemoryUsage = 256 * 1024 * 1024;  // Example: 256 MB
    
    m_drawCalls = 1250;  // Example value
    m_triangleCount = 250000;  // Example value
    m_batchCount = 120;  // Example value
    m_shaderBinds = 85;  // Example value
    
    // Check Tracy availability
    m_tracyEnabled = Rapture::TracyProfiler::isEnabled();
}

