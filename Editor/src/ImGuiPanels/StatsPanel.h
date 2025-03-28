#pragma once

#include <imgui.h>
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include "../../Engine/src/Debug/TracyProfiler.h"

class StatsPanel {
public:
    StatsPanel() = default;
    ~StatsPanel() = default;

    void render(float timestep);

private:
    // Tabs
    enum class TabType {
        Overview,
        Tracy
    };
    TabType m_activeTab = TabType::Overview;
    
    // Cached data for performance
    void updateCachedData();
    void renderOverviewTab();
    void renderTracyTab();
    
    // Helpers for sections
    void renderHistoryGraph(const std::array<float, 100>& history, const char* label, float maxValue);
    void renderColoredValue(float value, float warningThreshold, float errorThreshold, 
                           const char* format, bool lowerIsBetter = true, const char* customText = nullptr);
    
    // Tracy-specific functionality
    void renderTracyEmbeddedView();  // Renders Tracy UI directly in ImGui
    void renderTracyServerStatus();  // Shows connection status with Tracy server
    void renderTracyControlPanel();  // Control panel for Tracy settings

    // Stats update timing
    static constexpr float UPDATE_INTERVAL = 0.5f; // Update interval
    float m_updateTimer = 0.0f;
    
    // Simple performance history (we'll keep this even without the old profilers)
    std::array<float, 100> m_frameTimeHistory = {};
    int m_frameTimeHistoryIndex = 0;
    
    // Rendering stats (placeholder values)
    int m_drawCalls = 0;
    int m_triangleCount = 0;
    int m_batchCount = 0;
    int m_shaderBinds = 0;
    
    // Memory stats (placeholder values)
    size_t m_totalMemoryUsage = 0;
    size_t m_textureMemoryUsage = 0;
    size_t m_meshMemoryUsage = 0;
    
    // Tracy-specific data
    bool m_tracyEnabled = false;
    bool m_tracyConnected = false;
    ImVec2 m_tracyViewSize = ImVec2(0, 400);
    
    // Performance data
    float m_lastFrameTimeMs = 0.0f;
    float m_minFrameTimeMs = 0.0f;
    float m_maxFrameTimeMs = 0.0f;
    float m_avgFrameTimeMs = 0.0f;
    int m_fps = 0;
    
    // Performance thresholds
    static constexpr float FRAMETIME_WARNING_MS = 16.0f;  // ~60 FPS
    static constexpr float FRAMETIME_ERROR_MS = 33.0f;    // ~30 FPS
    static constexpr float MEMORY_WARNING_MB = 1024.0f;   // 1 GB
    static constexpr float MEMORY_ERROR_MB = 1536.0f;     // 1.5 GB
    static constexpr int DRAWCALL_WARNING = 1000;
    static constexpr int DRAWCALL_ERROR = 2000;
    static constexpr float IM_PI = 3.14159265358979323846f;
};

