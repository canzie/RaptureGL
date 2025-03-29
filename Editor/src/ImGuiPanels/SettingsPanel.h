#pragma once

#include <imgui.h>
#include <string>
#include <array>
#include <filesystem>
#include "../../Engine/src/WindowContext/OpenGLWindowContext/OpenGLWindowContext.h"
#include "../../Engine/src/Scenes/Scene.h"

namespace Rapture {
    class WindowContext;
}

class SettingsPanel {
public:
    SettingsPanel(Rapture::WindowContext* context);
    ~SettingsPanel() = default;

    void render();
    
    void setActiveScene(std::shared_ptr<Rapture::Scene> scene) { m_activeScene = scene; }
    std::shared_ptr<Rapture::Scene> getActiveScene() const { return m_activeScene; }

    // Accessor for triple buffering state
    bool isTripleBufferingEnabled() const { 
        return m_currentSwapMode == Rapture::SwapMode::AdaptiveVSync ||
               m_currentSwapMode == Rapture::SwapMode::TripleBuffering;
    }

private:
    void renderGraphicsSettings();
    void renderRenderingSettings();
    void renderSceneSettings();
    
    // Helper to extract filename from path
    std::string extractFilename(const std::string& path) const {
        return std::filesystem::path(path).filename().string();
    }
    
    // Helper to open file dialog for skybox textures
    bool openSkyboxFileDialog(char* outPath, size_t outPathSize);
    
    // Window context reference for applying settings
    Rapture::WindowContext* m_windowContext = nullptr;
    
    // UI state
    bool m_vsyncEnabled = false;
    bool m_tripleBufferingEnabled = false;
    bool m_frustumCullingEnabled = true; // Default to true
    
    // Current settings
    Rapture::SwapMode m_currentSwapMode = Rapture::SwapMode::Immediate;
    
    // Active scene reference
    std::shared_ptr<Rapture::Scene> m_activeScene = nullptr;
    
    // Skybox editing
    static constexpr size_t MAX_PATH_LENGTH = 256;
    std::array<char, MAX_PATH_LENGTH> m_skyboxRightPath = {0};
    std::array<char, MAX_PATH_LENGTH> m_skyboxLeftPath = {0};
    std::array<char, MAX_PATH_LENGTH> m_skyboxTopPath = {0};
    std::array<char, MAX_PATH_LENGTH> m_skyboxBottomPath = {0};
    std::array<char, MAX_PATH_LENGTH> m_skyboxFrontPath = {0};
    std::array<char, MAX_PATH_LENGTH> m_skyboxBackPath = {0};
    bool m_skyboxPathsChanged = false;
}; 