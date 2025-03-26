#pragma once

#include "../../Engine/src/Logger/Log.h"
#include <imgui.h>
#include <array>
#include <unordered_map>


class LogPanel {
public:
    LogPanel();
    ~LogPanel() = default;

    void render();
    void clear();
    
    // Enable/disable specific log levels or categories
    void setShowLevel(spdlog::level::level_enum level, bool show);
    void setShowCategory(Rapture::LogCategory category, bool show);
    
    // Filter logs by text
    void setFilter(const std::string& filter);
    
private:
    // Display state
    bool m_AutoScroll = true;
    ImGuiTextFilter m_Filter;
    float m_FooterHeight = 0.0f;
    
    // Category/level visibility
    std::unordered_map<Rapture::LogCategory, bool> m_ShowCategory;
    std::array<bool, spdlog::level::n_levels> m_ShowLevel;
    
    // Color settings
    ImVec4 getLogLevelColor(spdlog::level::level_enum level) const;
    std::unordered_map<Rapture::LogCategory, ImVec4> m_CategoryColors;
    
    // Helper methods
    bool passesFilter(const Rapture::LogMessage& logMessage);
    void drawLogLine(const Rapture::LogMessage& logMessage);
    void drawToolbar();
    void drawOptions();
    
    void initializeColors();
};
