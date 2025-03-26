#include "SettingsPanel.h"
#include "../../Engine/src/WindowContext/WindowContext.h"
#include "../../Engine/src/WindowContext/OpenGLWindowContext/OpenGLWindowContext.h"
#include "Logger/Log.h"
#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

SettingsPanel::SettingsPanel(Rapture::WindowContext* context)
    : m_windowContext(context)
{
    // Initialize with current settings
    if (m_windowContext) {
        // Get current swap mode
        m_currentSwapMode = m_windowContext->getSwapMode();
        
        // Set UI state based on current mode
        switch (m_currentSwapMode) {
            case Rapture::SwapMode::VSync:
                m_vsyncEnabled = true;
                m_tripleBufferingEnabled = false;
                break;
                
            case Rapture::SwapMode::AdaptiveVSync:
                m_vsyncEnabled = true;
                m_tripleBufferingEnabled = true;
                break;
                
            case Rapture::SwapMode::TripleBuffering:
                m_vsyncEnabled = false;
                m_tripleBufferingEnabled = true;
                break;
                
            case Rapture::SwapMode::Immediate:
            default:
                m_vsyncEnabled = false;
                m_tripleBufferingEnabled = false;
                break;
        }
    }
    
    // Initialize frustum culling state from renderer
    m_frustumCullingEnabled = Rapture::Renderer::isFrustumCullingEnabled();
}

void SettingsPanel::render()
{
    if (ImGui::Begin("Settings")) {
        // Tab bar
        if (ImGui::BeginTabBar("SettingsTabs")) {
            if (ImGui::BeginTabItem("Graphics")) {
                m_activeTab = TabType::Graphics;
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Performance")) {
                m_activeTab = TabType::Performance;
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
            case TabType::Graphics:
                renderGraphicsSettings();
                break;
            case TabType::Performance:
                renderPerformanceSettings();
                break;
            case TabType::Rendering:
                renderRenderingSettings();
                break;
        }
    }
    ImGui::End();
}

void SettingsPanel::renderGraphicsSettings()
{
    ImGui::Text("Display Settings");
    ImGui::Separator();
    
    bool vsyncChanged = false;
    bool tripleBufferingChanged = false;
    
    // VSync toggle
    if (ImGui::Checkbox("VSync", &m_vsyncEnabled)) {
        vsyncChanged = true;
    }
    
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Vertical synchronization limits frame rate to monitor refresh rate");
        ImGui::Text("Reduces tearing but may increase input lag");
        ImGui::EndTooltip();
    }
    
    // Triple buffering toggle - now available always
    ImGui::Checkbox("Triple Buffering", &m_tripleBufferingEnabled);
    tripleBufferingChanged = ImGui::IsItemEdited();
    
    // Only enable the checkbox if triple buffering is supported
    bool tripleBufferingSupported = m_windowContext->isTripleBufferingSupported();
    if (!tripleBufferingSupported) {
        // If not supported, disable the checkbox after it's drawn
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Not supported on this GPU/driver)");
        // Reset to false if hardware doesn't support it
        m_tripleBufferingEnabled = false;
    }
    
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Uses three buffers instead of two to reduce stuttering and improve performance");
        if (m_vsyncEnabled) {
            ImGui::Text("With VSync: Provides smoother frame pacing with reduced input lag");
            ImGui::Text("Requires WGL_EXT_swap_control_tear or GLX_EXT_swap_control_tear extension");
        } else {
            ImGui::Text("Without VSync: Reduces stuttering while maintaining uncapped framerate");
        }
        ImGui::EndTooltip();
    }
    
    // Apply changes
    if (vsyncChanged || tripleBufferingChanged) {
        if (m_windowContext) {
            // Determine the appropriate swap mode based on settings
            Rapture::SwapMode newMode;
            
            if (m_vsyncEnabled) {
                if (m_tripleBufferingEnabled && tripleBufferingSupported) {
                    newMode = Rapture::SwapMode::AdaptiveVSync; // Triple buffering with VSync
                } else {
                    newMode = Rapture::SwapMode::VSync; // Regular VSync
                }
            } else {
                if (m_tripleBufferingEnabled && tripleBufferingSupported) {
                    newMode = Rapture::SwapMode::TripleBuffering; // Triple buffering without VSync
                } else {
                    newMode = Rapture::SwapMode::Immediate; // No VSync, double buffering
                }
            }
            
            // Apply the new mode
            m_windowContext->setSwapMode(newMode);
            
            // Update current mode
            m_currentSwapMode = m_windowContext->getSwapMode();
        }
    }
}

void SettingsPanel::renderPerformanceSettings()
{
    ImGui::Text("Performance Information");
    ImGui::Separator();
    
    // Display current buffer mode
    ImGui::Text("Buffer Mode: ");
    ImGui::SameLine();
    
    switch (m_currentSwapMode) {
        case Rapture::SwapMode::Immediate:
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Uncapped (Double Buffering)");
            break;
        case Rapture::SwapMode::VSync:
            ImGui::TextColored(ImVec4(0.0f, 0.7f, 1.0f, 1.0f), "Double Buffered (VSync On)");
            break;
        case Rapture::SwapMode::AdaptiveVSync:
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Triple Buffered (Adaptive VSync)");
            break;
        case Rapture::SwapMode::TripleBuffering:
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Triple Buffered (Uncapped)");
            break;
        default:
            ImGui::Text("Unknown");
    }
    
    // Display extension support information
    bool tearControlSupported = m_windowContext->isTripleBufferingSupported();
    
    ImGui::Text("Tear Control Extension: %s", tearControlSupported ? "Supported" : "Not Supported");
    
    if (tearControlSupported) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Triple buffering is available on this system");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Triple buffering not available on this system");
        ImGui::TextWrapped("Your GPU or driver does not support the required extensions (WGL_EXT_swap_control_tear or GLX_EXT_swap_control_tear)");
    }
    
    // Performance tips
    ImGui::Separator();
    ImGui::Text("Performance Tips:");
    
    switch (m_currentSwapMode) {
        case Rapture::SwapMode::Immediate:
            ImGui::BulletText("Current mode provides high performance but may cause screen tearing");
            ImGui::BulletText("Best for high-FPS competitive gameplay where input latency is critical");
            break;
        case Rapture::SwapMode::VSync:
            ImGui::BulletText("Current mode eliminates tearing but may increase input latency");
            ImGui::BulletText("Performance will be limited to monitor refresh rate");
            break;
        case Rapture::SwapMode::AdaptiveVSync:
            ImGui::BulletText("Current mode reduces tearing while minimizing input latency");
            ImGui::BulletText("Best balance between visual quality and responsiveness");
            break;
        case Rapture::SwapMode::TripleBuffering:
            ImGui::BulletText("Current mode provides smoother frame delivery at high framerates");
            ImGui::BulletText("May have screen tearing, but with reduced stuttering compared to double buffering");
            ImGui::BulletText("Best for high-FPS gameplay with smoother frame pacing");
            break;
    }
}

void SettingsPanel::renderRenderingSettings()
{
    ImGui::Text("Rendering Optimizations");
    ImGui::Separator();
    
    // Frustum culling toggle
    bool frustumCullingChanged = false;
    if (ImGui::Checkbox("Frustum Culling", &m_frustumCullingEnabled)) {
        frustumCullingChanged = true;
        
        // Apply the change immediately
        Rapture::Renderer::enableFrustumCulling(m_frustumCullingEnabled);
        
        // Log the change
        Rapture::GE_INFO("Frustum culling {0} from settings panel", 
                   m_frustumCullingEnabled ? "enabled" : "disabled");
    }
    
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Frustum culling prevents rendering objects that are outside the camera view");
        ImGui::Text("Improves performance but may introduce popping if bounding boxes are inaccurate");
        if (m_frustumCullingEnabled) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Currently: Enabled (Better Performance)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Currently: Disabled (Lower Performance)");
        }
        ImGui::EndTooltip();
    }
    
    ImGui::Separator();
    ImGui::Text("Rendering Information");
    
    // Display culling statistics if available
    if (m_frustumCullingEnabled) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Frustum culling is active");
        ImGui::Text("Objects outside the camera view are not rendered");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Frustum culling is disabled");
        ImGui::Text("All objects are rendered regardless of visibility");
    }
    
    // Rendering tips
    ImGui::Separator();
    ImGui::Text("Rendering Tips:");
    ImGui::BulletText("Enable frustum culling for better performance in complex scenes");
    ImGui::BulletText("Disable frustum culling only if you experience object popping issues");
    ImGui::BulletText("Ensure bounding boxes are accurate for optimal culling");
} 