#pragma once

#include <imgui.h>
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include "../../Engine/src/Debug/Profiler.h"

class StatsPanel {
public:
    StatsPanel() = default;
    ~StatsPanel() = default;

    void render(float timestep);

private:
    // Tabs
    enum class TabType {
        Overview,
        CPU,
        GPU,
        Memory,
        Rendering
    };
    TabType m_activeTab = TabType::Overview;
    
    // Cached data for performance
    void updateCachedData();
    void renderOverviewTab();
    void renderCPUTab();
    void renderGPUTab();
    void renderMemoryTab();
    void renderRenderingTab();
    
    // Helpers for sections
    void renderHistoryGraph(const std::array<float, 100>& history, const char* label, float maxValue);
    void renderTimingTable(const std::vector<std::pair<std::string, Rapture::ProfileTimingData>>& data, 
                          const char* title, bool hierarchical = false);
    void renderTimingRow(const std::pair<std::string, Rapture::ProfileTimingData>& row, bool isPinned);
    void renderTimingGraph(const std::string& name, const std::array<float, 30>& history, 
                          float currentValue, float maxValue);
    void renderFrameTimeBreakdown();
    void renderColoredValue(float value, float warningThreshold, float errorThreshold, 
                           const char* format, bool lowerIsBetter = true, const char* customText = nullptr);

    // Stats update timing
    static constexpr float UPDATE_INTERVAL = 0.5f; // Shorter update interval
    float m_updateTimer = 0.0f;
    
    // Search functionality
    char m_searchBuffer[128] = "";
    bool m_searchActive = false;
    
    // Pinned components
    std::set<std::string> m_pinnedComponents;
    
    // Cached performance data
    float m_cachedCPUFrameTime = 0.0f;
    float m_cachedGPUFrameTime = 0.0f;
    float m_cachedAverageFrameTime = 0.0f;
    float m_cachedMinFrameTime = 0.0f;
    float m_cachedMaxFrameTime = 0.0f;
    int m_cachedFPS = 0;
    std::array<float, 100> m_cachedCPUHistory = {};
    std::array<float, 100> m_cachedGPUHistory = {};
    std::vector<std::pair<std::string, Rapture::ProfileTimingData>> m_cachedProfileData;
    std::vector<std::pair<std::string, Rapture::ProfileTimingData>> m_filteredProfileData;
    std::unordered_map<std::string, std::array<float, 30>> m_componentHistories;
    
    // Rendering stats
    int m_drawCalls = 0;
    int m_triangleCount = 0;
    int m_batchCount = 0;
    int m_shaderBinds = 0;
    
    // Memory stats
    size_t m_totalMemoryUsage = 0;
    size_t m_textureMemoryUsage = 0;
    size_t m_meshMemoryUsage = 0;
    
    // GPU timing data
    std::vector<std::pair<std::string, float>> m_gpuTimings;

    // Performance thresholds
    static constexpr float FRAMETIME_WARNING_MS = 16.0f;  // ~60 FPS
    static constexpr float FRAMETIME_ERROR_MS = 33.0f;    // ~30 FPS
    static constexpr float MEMORY_WARNING_MB = 1024.0f;   // 1 GB
    static constexpr float MEMORY_ERROR_MB = 1536.0f;     // 1.5 GB
    static constexpr int DRAWCALL_WARNING = 1000;
    static constexpr int DRAWCALL_ERROR = 2000;
    static constexpr float IM_PI = 3.14159265358979323846f;
};

