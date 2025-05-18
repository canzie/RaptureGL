#include "SettingsPanel.h"
#include "../../Engine/src/WindowContext/WindowContext.h"
#include "../../Engine/src/WindowContext/OpenGLWindowContext/OpenGLWindowContext.h"
#include "Logger/Log.h"
#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>
#include <cstring>

#include "Sorting/SpatialSorting/BVH/LBVH/LBVH.h"
#include "Renderer/Deferred Shading/DeferredRenderer.h"

// Platform-specific file dialog includes
#ifdef _WIN32
// For Windows file dialog
#include <Windows.h>
#include <commdlg.h>
#endif

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
    m_frustumCullingEnabled = false;
}

bool SettingsPanel::openSkyboxFileDialog(char* outPath, size_t outPathSize) {
#ifdef _WIN32
    // Windows file dialog implementation
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR; // Added OFN_NOCHANGEDIR
    
    // Show the dialog
    if (GetOpenFileNameA(&ofn)) {
        // Use strncpy_s for safety if available, otherwise standard strncpy
#ifdef _MSC_VER
        strncpy_s(outPath, outPathSize, ofn.lpstrFile, _TRUNCATE);
#else
        std::strncpy(outPath, ofn.lpstrFile, outPathSize - 1);
        outPath[outPathSize - 1] = '\0'; // Ensure null termination
#endif
        return true;
    }
    return false;
#elif defined(__linux__)
    Rapture::GE_CORE_WARN("File dialog not implemented for Linux in SettingsPanel.");
    return false;
#else
    #pragma message("Warning: File dialog not implemented for this platform in SettingsPanel.")
    Rapture::GE_CORE_WARN("File dialog not implemented for this platform in SettingsPanel.");
    return false;
#endif
}

void SettingsPanel::render()
{
    if (ImGui::Begin("Settings")) {
        // Graphics settings with collapsible header
        if (ImGui::CollapsingHeader("Graphics Settings")) {
            renderGraphicsSettings();
        }
        
        
        // Scene settings with collapsible header
        if (ImGui::CollapsingHeader("Scene Settings")) {
            renderSceneSettings();
        }
    }
    ImGui::End();
}

void SettingsPanel::renderGraphicsSettings()
{
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
    
    // Add display of current buffer mode
    ImGui::Separator();
    ImGui::Text("Current Buffer Mode: ");
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


void SettingsPanel::renderSceneSettings()
{
    if (!m_activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active scene selected");
        return;
    }
    
    // Scene Name & Info
    ImGui::Text("Scene Name: Scene_%p", (void*)m_activeScene.get());  // Using pointer address as unique ID
    
    // Display scene settings from the scene's SceneSettings struct
    Rapture::SceneSettings& settings = m_activeScene->getSettings();
    
    ImGui::Separator();
    ImGui::Text("Scene Options:");
    
    // Frustum culling setting (sync with the renderer setting)
    if (ImGui::Checkbox("Frustum Culling##SceneSetting", &settings.frustumCullingEnabled)) {
        // Sync with the panel's setting and update renderer
        m_frustumCullingEnabled = settings.frustumCullingEnabled;
    }
    
    // Raycast debug option
    if (ImGui::Checkbox("Raycast Debug", &settings.rayCastDebugEnabled)) {
        // Toggled raycast debug
        Rapture::GE_INFO("Raycast debugging {0} from settings panel", 
                   settings.rayCastDebugEnabled ? "enabled" : "disabled");
    }
    
    // Async rendering option
    if (ImGui::Checkbox("Async Rendering", &settings.useAsyncRendering)) {
        Rapture::GE_INFO("Async rendering {0} from settings panel", 
                   settings.useAsyncRendering ? "enabled" : "disabled");
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable asynchronous rendering for better performance.\nBuilds the render queue in a separate thread.");
    }
    
    // Button to reset scene settings
    if (ImGui::Button("Reset Scene Settings")) {
        settings.frustumCullingEnabled = true;
        settings.rayCastDebugEnabled = false;
        settings.useAsyncRendering = true;
        m_frustumCullingEnabled = true;
        
        Rapture::GE_INFO("Scene settings reset to defaults");
    }

    ImGui::Separator();

    auto camera = m_activeScene->getMainCamera();
    auto& cameraController = camera->getComponent<Rapture::CameraControllerComponent>();
    auto fovy = cameraController.fov;
    auto near_plane = cameraController.near_plane;
    auto far_plane = cameraController.far_plane;

    if (ImGui::CollapsingHeader("Camera Settings")) {
        if (ImGui::DragFloat("Field of View", &fovy, 1.0f, 15.0f, 120.0f)) {
            cameraController.updateProjectionMatrix(fovy, cameraController.aspect_ratio, near_plane, far_plane);
        }
        if (ImGui::DragFloat("Near Plane", &near_plane, 0.5f, 0.1f, 10.0f)) {
            near_plane = (std::min)(near_plane, far_plane);
            cameraController.updateProjectionMatrix(fovy, cameraController.aspect_ratio, near_plane, far_plane);
        }
        if (ImGui::DragFloat("Far Plane", &far_plane, 1.0f, 10.0f, 1000.0f)) {
            far_plane = (std::max)(far_plane, near_plane + 10.0f);
            cameraController.updateProjectionMatrix(fovy, cameraController.aspect_ratio, near_plane, far_plane);
        }

    }
    ImGui::Separator();
    
    
    // BVH Visualization Range Slider


        ImGui::Text("BVH Visualization Range:");
        

        ImGui::DragInt("Depth Level", &m_bvhDisplayDepth, 0.1f, 0, 32);


        Rapture::LBVHManager::setDepthLevel(m_bvhDisplayDepth);



    ImGui::Separator();

    bool ddgiEnabled = Rapture::DeferredRenderer::getEnableDDGI();

    if (ImGui::Checkbox("Enable DDGI", &ddgiEnabled)) {
        Rapture::DeferredRenderer::setEnableDDGI(ddgiEnabled);
    }



    ImGui::Separator();
        ImGui::Text("DDGI Debug Settings:");

        Rapture::DebugConfig* debugConfig = Rapture::DeferredRenderer::getDDGIDebugConfig();

        ImGui::Checkbox("Show Diffuse", &debugConfig->showDiffuse);

        ImGui::Checkbox("Show Direct", &debugConfig->showDirect);

        ImGui::Checkbox("Show Direct Ambient", &debugConfig->showDirectAmbient);

        ImGui::Checkbox("Show Final", &debugConfig->showFinal);

        ImGui::Checkbox("Show Diffuse Intensity", &debugConfig->showDiffuseIntensity);

        ImGui::Checkbox("Show Trilinear Weight Sum", &debugConfig->debug_DDGI_TrilinearWeightSum);

        ImGui::Checkbox("Show Backface Weight Sum", &debugConfig->debug_DDGI_BackfaceWeightSum);

        ImGui::Checkbox("Show Visibility Factor Sum", &debugConfig->debug_DDGI_VisibilityFactorSum);
        
        ImGui::Checkbox("Show Final Probe Weight Sum", &debugConfig->debug_DDGI_FinalProbeWeightSum);

        ImGui::Checkbox("Show Raw Indirect Sum", &debugConfig->debug_DDGI_RawIndirectSum);

        ImGui::Checkbox("Show Total Weight", &debugConfig->debug_DDGI_TotalWeight);

        ImGui::Checkbox("Show Probe Irradiance Sum", &debugConfig->debug_DDGI_ProbeIrradianceSum);

        ImGui::Checkbox("Show Probe Influence", &debugConfig->debug_DDGI_ProbeInfluenc);


    // Skybox Settings
    ImGui::Separator();


    
    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        Rapture::SkyBox& skybox = m_activeScene->getSkyBox();
        
        // Copy current skybox paths to the editing buffers if they exist and aren't already loaded
        if (!skybox.texturePaths.empty() && !m_skyboxPathsChanged) {
            if (skybox.texturePaths.size() >= 6) {
                std::strncpy(m_skyboxRightPath.data(), skybox.texturePaths[0].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxLeftPath.data(), skybox.texturePaths[1].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxTopPath.data(), skybox.texturePaths[2].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxBottomPath.data(), skybox.texturePaths[3].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxFrontPath.data(), skybox.texturePaths[4].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxBackPath.data(), skybox.texturePaths[5].string().c_str(), MAX_PATH_LENGTH - 1);
            }
        }
        
        ImGui::Text("Skybox Texture Paths:");
        
        // Function to create a row with text input and browse button
        auto drawTextureRow = [this](const char* label, char* buffer, size_t bufferSize) {
            ImGui::Text("%-7s", label);
            ImGui::SameLine();
            
            // Calculate available width for the input field (leaving space for the button)
            float availWidth = ImGui::GetContentRegionAvail().x;
            float buttonWidth = 80.0f;
            float inputWidth = availWidth - buttonWidth - ImGui::GetStyle().ItemSpacing.x;
            
            // Display filename only, but still edit the full path
            std::string filename = buffer[0] != '\0' ? extractFilename(buffer) : "";
            char displayBuffer[256];
            std::strncpy(displayBuffer, filename.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            
            ImGui::PushItemWidth(inputWidth);
            if (ImGui::InputText(("##" + std::string(label)).c_str(), displayBuffer, sizeof(displayBuffer), ImGuiInputTextFlags_ReadOnly)) {
                // This won't happen since it's read-only
            }
            
            // Add drop target for image files
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_IMAGE_PATH")) {
                    // Cast payload data to const char* and copy to buffer
                    const char* path = static_cast<const char*>(payload->Data);
                    if (path) {
                        std::strncpy(buffer, path, bufferSize - 1);
                        buffer[bufferSize - 1] = '\0'; // Ensure null termination
                        m_skyboxPathsChanged = true;
                        
                        Rapture::GE_INFO("Dropped image file onto {0} skybox slot: {1}", label, buffer);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button(("Browse##" + std::string(label)).c_str(), ImVec2(buttonWidth, 0))) {
                if (openSkyboxFileDialog(buffer, bufferSize)) {
                    m_skyboxPathsChanged = true;
                }
            }
        };
        
        // Text inputs and browse buttons for each face of the cubemap
        drawTextureRow("Right:", m_skyboxRightPath.data(), MAX_PATH_LENGTH);
        drawTextureRow("Left:", m_skyboxLeftPath.data(), MAX_PATH_LENGTH);
        drawTextureRow("Top:", m_skyboxTopPath.data(), MAX_PATH_LENGTH);
        drawTextureRow("Bottom:", m_skyboxBottomPath.data(), MAX_PATH_LENGTH);
        drawTextureRow("Front:", m_skyboxFrontPath.data(), MAX_PATH_LENGTH);
        drawTextureRow("Back:", m_skyboxBackPath.data(), MAX_PATH_LENGTH);
        
        // Apply button to update the skybox
        if (ImGui::Button("Apply Skybox Changes") && m_skyboxPathsChanged) {
            // Create a vector of the new paths
            std::vector<std::filesystem::path> newPaths = {
                m_skyboxRightPath.data(),
                m_skyboxLeftPath.data(),
                m_skyboxTopPath.data(),
                m_skyboxBottomPath.data(),
                m_skyboxFrontPath.data(),
                m_skyboxBackPath.data()
            };
            
            // Update the skybox with the new paths
            skybox.setTexturePaths(newPaths);
            
            m_skyboxPathsChanged = false;
            Rapture::GE_INFO("Skybox textures updated");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Reset to Current")) {
            // Reset the editing buffers to the current skybox paths
            if (!skybox.texturePaths.empty() && skybox.texturePaths.size() >= 6) {
                std::strncpy(m_skyboxRightPath.data(), skybox.texturePaths[0].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxLeftPath.data(), skybox.texturePaths[1].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxTopPath.data(), skybox.texturePaths[2].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxBottomPath.data(), skybox.texturePaths[3].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxFrontPath.data(), skybox.texturePaths[4].string().c_str(), MAX_PATH_LENGTH - 1);
                std::strncpy(m_skyboxBackPath.data(), skybox.texturePaths[5].string().c_str(), MAX_PATH_LENGTH - 1);
                
                m_skyboxPathsChanged = false;
            }
        }
        
        // Display the current skybox information
        ImGui::Separator();
        ImGui::Text("Current Skybox:");
        if (!skybox.texturePaths.empty()) {
            for (size_t i = 0; i < skybox.texturePaths.size() && i < 6; i++) {
                // Display only filename part for cleaner UI
                std::string filename = extractFilename(skybox.texturePaths[i]);
                
                const char* faceName = nullptr;
                switch (i) {
                    case 0: faceName = "Right: "; break;
                    case 1: faceName = "Left:  "; break;
                    case 2: faceName = "Top:   "; break;
                    case 3: faceName = "Bottom:"; break;
                    case 4: faceName = "Front: "; break;
                    case 5: faceName = "Back:  "; break;
                }
                
                ImGui::Text("%s %s", faceName, filename.c_str());
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No skybox textures loaded");
        }
    }
} 