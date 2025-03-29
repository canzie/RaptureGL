#pragma once

#include "imgui.h"
#include <unordered_map>
#include <string>

namespace ImGuiPanelStyle {
    // Font pointers
    static ImFont* s_RegularFont = nullptr;
    static ImFont* s_BoldFont = nullptr;
    static ImFont* s_LightFont = nullptr;
    static ImFont* s_ItalicFont = nullptr;

    // Color palette inspired by Obsidian's dark mode
    // Using colors from: https://docs.obsidian.md/Reference/CSS+variables/Foundations/Colors
    
    // Base interface colors - keeping these as they are
    inline const ImVec4 BACKGROUND_PRIMARY = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);    // #1a1a1a - Main background
    inline const ImVec4 BACKGROUND_SECONDARY = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);  // #262626 - Sidebars, secondary areas
    inline const ImVec4 BACKGROUND_TERTIARY = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);   // #2e2e2e - Panel backgrounds, input fields
    
    // Text colors - keeping these as they are
    inline const ImVec4 TEXT_NORMAL = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);           // #d1d1d1 - Primary text
    inline const ImVec4 TEXT_MUTED = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);            // #999999 - Secondary text, hints
    inline const ImVec4 TEXT_FAINT = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);            // #666666 - Tertiary text, disabled items
    
    // Accent colors - updated to match Obsidian's subtle purple theme
    inline const ImVec4 ACCENT_PRIMARY = ImVec4(0.431f, 0.365f, 0.659f, 1.00f);     // #6E5DA8 - Main accent (darker purple)
    inline const ImVec4 ACCENT_SECONDARY = ImVec4(0.659f, 0.510f, 1.000f, 1.00f);   // #A882FF - Secondary accent (lighter purple)
    inline const ImVec4 ACCENT_TERTIARY = ImVec4(0.33f, 0.75f, 0.59f, 1.00f);       // #54bf96 - Tertiary accent (green)
    
    // State colors - keeping these as they are
    inline const ImVec4 SUCCESS_COLOR = ImVec4(0.33f, 0.75f, 0.59f, 1.00f);         // #54bf96 - Success (green)
    inline const ImVec4 WARNING_COLOR = ImVec4(0.93f, 0.68f, 0.35f, 1.00f);         // #edae59 - Warning (orange)
    inline const ImVec4 ERROR_COLOR = ImVec4(0.93f, 0.35f, 0.39f, 1.00f);           // #ed5963 - Error (red)
    inline const ImVec4 INFO_COLOR = ImVec4(0.40f, 0.73f, 0.93f, 1.00f);            // #66baed - Info (blue)
    
    // UI element colors - updated to match Obsidian's theme
    inline const ImVec4 BORDER = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);                // #38383f - Borders
    inline const ImVec4 SEPARATOR = ImVec4(0.27f, 0.27f, 0.32f, 1.00f);             // #43434f - Separators, dividers
    inline const ImVec4 HIGHLIGHT = ImVec4(0.431f, 0.365f, 0.659f, 1.00f);          // #6E5DA8 - Highlighted items (darker purple)
    inline const ImVec4 SELECTION = ImVec4(0.431f, 0.365f, 0.659f, 0.50f);          // #6E5DA8 with 0.5 alpha - Selected text background
    
    // New light purple for hover states
    inline const ImVec4 LIGHT_PURPLE = ImVec4(0.549f, 0.478f, 0.773f, 1.00f);       // #8C7AC5 - Lighter purple for hover states

    // Interactive state modifiers - reduced opacity for more subtle hover effects
    inline const ImVec4 HOVER_OVERLAY = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);         // Reduced from 0.07f to 0.04f
    inline const ImVec4 ACTIVE_OVERLAY = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);        // Reduced from 0.12f to 0.08f

    // Named style mapping (for easy access/reference)
    inline const std::unordered_map<std::string, ImVec4> NamedColors = {
        {"background_primary", BACKGROUND_PRIMARY},
        {"background_secondary", BACKGROUND_SECONDARY},
        {"background_tertiary", BACKGROUND_TERTIARY},
        {"text_normal", TEXT_NORMAL},
        {"text_muted", TEXT_MUTED},
        {"text_faint", TEXT_FAINT},
        {"accent_primary", ACCENT_PRIMARY},
        {"accent_secondary", ACCENT_SECONDARY},
        {"accent_tertiary", ACCENT_TERTIARY},
        {"success", SUCCESS_COLOR},
        {"warning", WARNING_COLOR},
        {"error", ERROR_COLOR},
        {"info", INFO_COLOR},
        {"border", BORDER},
        {"separator", SEPARATOR},
        {"highlight", HIGHLIGHT},
        {"selection", SELECTION},
        {"light_purple", LIGHT_PURPLE}
    };
    
    // Global flag to track if style has been initialized
    static bool s_StyleInitialized = false;

    // Initialize fonts - must be called before any ImGui rendering
    inline void InitializeFonts() {
        ImGuiIO& io = ImGui::GetIO();
        
        // Load fonts
        s_RegularFont = io.Fonts->AddFontFromFileTTF("assets/fonts/IBM_Plex_Mono/IBMPlexMono-Regular.ttf", 16.0f);
        s_BoldFont = io.Fonts->AddFontFromFileTTF("assets/fonts/IBM_Plex_Mono/IBMPlexMono-Bold.ttf", 16.0f);
        s_LightFont = io.Fonts->AddFontFromFileTTF("assets/fonts/IBM_Plex_Mono/IBMPlexMono-Light.ttf", 16.0f);
        s_ItalicFont = io.Fonts->AddFontFromFileTTF("assets/fonts/IBM_Plex_Mono/IBMPlexMono-Italic.ttf", 16.0f);

        // Set default font
        if (s_RegularFont) {
            io.FontDefault = s_RegularFont;
        }
    }

    // Apply the style to ImGui - for use in individual panels if needed
    inline void ApplyStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Colors
        ImVec4* colors = style.Colors;
        
        // Main
        colors[ImGuiCol_WindowBg] = BACKGROUND_PRIMARY;
        colors[ImGuiCol_ChildBg] = BACKGROUND_PRIMARY;
        colors[ImGuiCol_PopupBg] = BACKGROUND_SECONDARY;
        colors[ImGuiCol_Border] = BORDER;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = BACKGROUND_TERTIARY;
        colors[ImGuiCol_FrameBgHovered] = ImVec4(BACKGROUND_TERTIARY.x + HOVER_OVERLAY.x * HOVER_OVERLAY.w,
                                               BACKGROUND_TERTIARY.y + HOVER_OVERLAY.y * HOVER_OVERLAY.w,
                                               BACKGROUND_TERTIARY.z + HOVER_OVERLAY.z * HOVER_OVERLAY.w,
                                               BACKGROUND_TERTIARY.w);
        colors[ImGuiCol_FrameBgActive] = ImVec4(BACKGROUND_TERTIARY.x + ACTIVE_OVERLAY.x * ACTIVE_OVERLAY.w,
                                              BACKGROUND_TERTIARY.y + ACTIVE_OVERLAY.y * ACTIVE_OVERLAY.w,
                                              BACKGROUND_TERTIARY.z + ACTIVE_OVERLAY.z * ACTIVE_OVERLAY.w,
                                              BACKGROUND_TERTIARY.w);
        
        // Text
        colors[ImGuiCol_Text] = TEXT_NORMAL;
        colors[ImGuiCol_TextDisabled] = TEXT_FAINT;
        
        // Headers - now using gray color instead of purple
        colors[ImGuiCol_Header] = BACKGROUND_SECONDARY;                                 // Gray color for headers
        colors[ImGuiCol_HeaderHovered] = ImVec4(BACKGROUND_SECONDARY.x + HOVER_OVERLAY.x * HOVER_OVERLAY.w,
                                              BACKGROUND_SECONDARY.y + HOVER_OVERLAY.y * HOVER_OVERLAY.w,
                                              BACKGROUND_SECONDARY.z + HOVER_OVERLAY.z * HOVER_OVERLAY.w,
                                              BACKGROUND_SECONDARY.w);                   // Gray with hover overlay
        colors[ImGuiCol_HeaderActive] = ImVec4(BACKGROUND_SECONDARY.x + ACTIVE_OVERLAY.x * ACTIVE_OVERLAY.w,
                                             BACKGROUND_SECONDARY.y + ACTIVE_OVERLAY.y * ACTIVE_OVERLAY.w,
                                             BACKGROUND_SECONDARY.z + ACTIVE_OVERLAY.z * ACTIVE_OVERLAY.w,
                                             BACKGROUND_SECONDARY.w);                    // Gray with active overlay
        
        // Buttons - now using the same color as headers (purple theme)
        colors[ImGuiCol_Button] = ImVec4(0.431f, 0.365f, 0.659f, 0.40f);            // Same as Header - #6E5DA8 with 0.4 alpha
        colors[ImGuiCol_ButtonHovered] = LIGHT_PURPLE;                               // Lighter purple for hover
        colors[ImGuiCol_ButtonActive] = ImVec4(0.431f, 0.365f, 0.659f, 0.80f);      // Same as HeaderActive - #6E5DA8 with 0.8 alpha
        
        // Tabs
        colors[ImGuiCol_Tab] = BACKGROUND_SECONDARY;
        colors[ImGuiCol_TabHovered] = ImVec4(BACKGROUND_SECONDARY.x + HOVER_OVERLAY.x * HOVER_OVERLAY.w,
                                           BACKGROUND_SECONDARY.y + HOVER_OVERLAY.y * HOVER_OVERLAY.w,
                                           BACKGROUND_SECONDARY.z + HOVER_OVERLAY.z * HOVER_OVERLAY.w,
                                           BACKGROUND_SECONDARY.w);
        colors[ImGuiCol_TabActive] = HIGHLIGHT;
        colors[ImGuiCol_TabUnfocused] = BACKGROUND_SECONDARY;
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(HIGHLIGHT.x * 0.8f, HIGHLIGHT.y * 0.8f, HIGHLIGHT.z * 0.8f, HIGHLIGHT.w);
        
        // Title
        colors[ImGuiCol_TitleBg] = BACKGROUND_SECONDARY;
        colors[ImGuiCol_TitleBgActive] = BACKGROUND_TERTIARY;
        colors[ImGuiCol_TitleBgCollapsed] = BACKGROUND_SECONDARY;
        
        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = BACKGROUND_PRIMARY;
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(BORDER.x + 0.05f, BORDER.y + 0.05f, BORDER.z + 0.05f, BORDER.w);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(TEXT_FAINT.x, TEXT_FAINT.y, TEXT_FAINT.z, 0.8f);
        colors[ImGuiCol_ScrollbarGrabActive] = TEXT_FAINT;
        
        // Checkmark
        colors[ImGuiCol_CheckMark] = ACCENT_PRIMARY;
        
        // Slider
        colors[ImGuiCol_SliderGrab] = ACCENT_PRIMARY;
        colors[ImGuiCol_SliderGrabActive] = ImVec4(ACCENT_PRIMARY.x * 1.2f, ACCENT_PRIMARY.y * 1.2f, ACCENT_PRIMARY.z * 1.2f, ACCENT_PRIMARY.w);
        
        // Other interactive elements
        colors[ImGuiCol_ResizeGrip] = ImVec4(ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.2f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.6f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.9f);
        
        // Separator
        colors[ImGuiCol_Separator] = SEPARATOR;
        colors[ImGuiCol_SeparatorHovered] = ImVec4(SEPARATOR.x + 0.1f, SEPARATOR.y + 0.1f, SEPARATOR.z + 0.1f, SEPARATOR.w);
        colors[ImGuiCol_SeparatorActive] = ImVec4(SEPARATOR.x + 0.2f, SEPARATOR.y + 0.2f, SEPARATOR.z + 0.2f, SEPARATOR.w);
        
        // Styles
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(4, 4);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 6.0f;
        
        // Note: For bold titles, you'll need to:
        // 1. Load a bold font (e.g., using ImGui::GetIO().Fonts->AddFontFromFileTTF("path/to/bold.ttf", size))
        // 2. Store the font pointer
        // 3. Use ImGui::PushFont(boldFont) before rendering titles
        // 4. Use ImGui::PopFont() after rendering titles
        
        // Borders and rounding
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        
        style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
    }
    
    // Initialize the style once at application startup
    inline void InitializeStyle() {
        // Only apply the style once
        if (!s_StyleInitialized) {
            ApplyStyle();
            s_StyleInitialized = true;
        }
    }
    
    // Font access methods
    inline ImFont* GetRegularFont() { return s_RegularFont; }
    inline ImFont* GetBoldFont() { return s_BoldFont; }
    inline ImFont* GetLightFont() { return s_LightFont; }
    inline ImFont* GetItalicFont() { return s_ItalicFont; }
    
    // Returns a specific color by name
    inline ImVec4 GetColor(const std::string& colorName) {
        auto it = NamedColors.find(colorName);
        if (it != NamedColors.end()) {
            return it->second;
        }
        // Return a default color if not found
        return TEXT_NORMAL;
    }
}
