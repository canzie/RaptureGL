#include "LogPanel.h"
#include <imgui_internal.h>
#include <algorithm>


    LogPanel::LogPanel()
    {
        // Initialize show filters - show all levels and categories by default
        for (int i = 0; i < spdlog::level::n_levels; i++) {
            m_ShowLevel[i] = true;
        }
        
        m_ShowCategory[Rapture::LogCategory::Core] = true;
        m_ShowCategory[Rapture::LogCategory::Client] = true;
        m_ShowCategory[Rapture::LogCategory::Debug] = true;
        m_ShowCategory[Rapture::LogCategory::Render] = true;
        m_ShowCategory[Rapture::LogCategory::Physics] = true;
        m_ShowCategory[Rapture::LogCategory::Audio] = true;
        
        initializeColors();
    }

    void LogPanel::initializeColors()
    {
        // Set category colors
        m_CategoryColors[Rapture::LogCategory::Core] = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);     // Blue
        m_CategoryColors[Rapture::LogCategory::Client] = ImVec4(0.9f, 0.9f, 0.5f, 1.0f);   // Yellow
        m_CategoryColors[Rapture::LogCategory::Debug] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // Gray
        m_CategoryColors[Rapture::LogCategory::Render] = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);   // Green
        m_CategoryColors[Rapture::LogCategory::Physics] = ImVec4(0.9f, 0.5f, 0.9f, 1.0f);  // Purple
        m_CategoryColors[Rapture::LogCategory::Audio] = ImVec4(0.5f, 0.9f, 0.9f, 1.0f);    // Cyan
    }

    ImVec4 LogPanel::getLogLevelColor(spdlog::level::level_enum level) const
    {
        switch (level) {
            case spdlog::level::trace:    return ImVec4(0.75f, 0.75f, 0.75f, 1.0f); // Light Gray
            case spdlog::level::debug:    return ImVec4(0.5f, 0.8f, 1.0f, 1.0f);    // Light Blue
            case spdlog::level::info:     return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);    // Green
            case spdlog::level::warn:     return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);    // Yellow
            case spdlog::level::err:      return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);    // Orange
            case spdlog::level::critical: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);    // Red
            case spdlog::level::off:      return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // Gray
            default:                      return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    // White
        }
    }

    void LogPanel::render()
    {
        if (ImGui::Begin("Log Panel")) {
            drawToolbar();
            
            ImGui::Separator();
            
            // Options popout section
            if (ImGui::Button("Options")) {
                ImGui::OpenPopup("LogOptions");
            }
            
            if (ImGui::BeginPopup("LogOptions")) {
                drawOptions();
                ImGui::EndPopup();
            }
            
            ImGui::Separator();
            
            // Reserve height for content
            const float footerHeight = ImGui::GetStyle().ItemSpacing.y + m_FooterHeight;
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
            
            // Display logs
            const auto& logs = Rapture::Log::GetRecentLogs();
            
            // Lock header row
            ImGui::Columns(3);
            ImGui::Text("Time"); ImGui::NextColumn();
            ImGui::Text("Category"); ImGui::NextColumn();
            ImGui::Text("Message"); ImGui::NextColumn();
            ImGui::Separator();
            
            for (const auto& logMessage : logs) {
                if (passesFilter(logMessage)) {
                    drawLogLine(logMessage);
                }
            }
            
            ImGui::Columns(1);
            
            // Auto-scroll when at bottom
            if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            
            ImGui::EndChild();
            
            ImGui::Separator();
            
            // Filter text input
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Filter:"); ImGui::SameLine();
            m_Filter.Draw("##LogFilter", ImGui::GetContentRegionAvail().x - 100);
            
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                clear();
            }
            
            ImGui::SameLine();
            ImGui::Checkbox("Auto-Scroll", &m_AutoScroll);
        }
        ImGui::End();
    }

    void LogPanel::drawToolbar()
    {
        // Log level buttons
        ImGui::Text("Levels:"); ImGui::SameLine();
        
        static const char* levelNames[] = {"Trace", "Debug", "Info", "Warn", "Error", "Critical"};
        
        for (int i = 0; i < 6; i++) {
            spdlog::level::level_enum level = static_cast<spdlog::level::level_enum>(i);
            ImGui::PushStyleColor(ImGuiCol_Text, getLogLevelColor(level));
            
            ImGui::Checkbox(levelNames[i], &m_ShowLevel[i]);
            
            ImGui::PopStyleColor();
            if (i < 5) ImGui::SameLine();
        }
    }

    void LogPanel::drawOptions()
    {
        ImGui::Text("Log Categories");
        
        struct CategoryLabel {
            const char* name;
            Rapture::LogCategory category;
        };
        
        static const CategoryLabel categories[] = {
            {"Core", Rapture::LogCategory::Core},
            {"Client", Rapture::LogCategory::Client},
            {"Debug", Rapture::LogCategory::Debug},
            {"Render", Rapture::LogCategory::Render},
            {"Physics", Rapture::LogCategory::Physics},
            {"Audio", Rapture::LogCategory::Audio}
        };
        
        for (const auto& cat : categories) {
            const ImVec4& color = m_CategoryColors[cat.category];
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::Checkbox(cat.name, &m_ShowCategory[cat.category]);
            ImGui::PopStyleColor();
        }
        
        ImGui::Separator();
        
        ImGui::Text("Appearance");
        
        static bool coloredOutput = true;
        ImGui::Checkbox("Colored output", &coloredOutput);
    }

    bool LogPanel::passesFilter(const Rapture::LogMessage& logMessage)
    {
        // Check if this log level is enabled
        if (!m_ShowLevel[static_cast<int>(logMessage.level)])
            return false;
        
        // Check if this category is enabled
        if (!m_ShowCategory[logMessage.category])
            return false;
        
        // Check if the message passes the filter
        if (!m_Filter.PassFilter(logMessage.message.c_str()))
            return false;
        
        return true;
    }

    void LogPanel::drawLogLine(const Rapture::LogMessage& logMessage)
    {
        ImGui::PushID(&logMessage);
        
        // Time column
        ImGui::Text("%s", logMessage.timestamp.c_str());
        ImGui::NextColumn();
        
        // Category column - colored by category
        ImGui::PushStyleColor(ImGuiCol_Text, m_CategoryColors[logMessage.category]);
        
        const char* categoryNames[] = {"CORE", "APP", "DEBUG", "RENDER", "PHYSICS", "AUDIO"};
        ImGui::Text("%s", categoryNames[static_cast<int>(logMessage.category)]);
        
        ImGui::PopStyleColor();
        ImGui::NextColumn();
        
        // Message column - colored by log level
        ImGui::PushStyleColor(ImGuiCol_Text, getLogLevelColor(logMessage.level));
        ImGui::TextWrapped("%s", logMessage.message.c_str());
        ImGui::PopStyleColor();
        ImGui::NextColumn();
        
        ImGui::PopID();
    }

    void LogPanel::clear()
    {
        Rapture::Log::ClearRecentLogs();
    }

    void LogPanel::setShowLevel(spdlog::level::level_enum level, bool show)
    {
        if (level < spdlog::level::n_levels) {
            m_ShowLevel[static_cast<int>(level)] = show;
        }
    }

    void LogPanel::setShowCategory(Rapture::LogCategory category, bool show)
    {
        m_ShowCategory[category] = show;
    }

    void LogPanel::setFilter(const std::string& filter)
    {
        m_Filter.Clear();
        if (!filter.empty()) {
            // ImGuiTextFilter doesn't have SetText, we need to use InputBuf directly
            strncpy(m_Filter.InputBuf, filter.c_str(), 256);
            m_Filter.Build();
        }
    }

