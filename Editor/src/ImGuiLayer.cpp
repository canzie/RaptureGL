#include "ImGuiLayer.h"
#include "WindowContext/Application.h"
#include "TestLayer.h"
#include "Logger/Log.h"
#include "Scenes/Components/Components.h"
#include "Scenes/Entity.h"
#include "Scenes/SceneManager.h"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "Debug/TracyProfiler.h"
#include "ImGuiPanels/imGuiPanelStyle.h"

ImGuiLayer::ImGuiLayer()
    : Layer("ImGuiLayer")
{
}

void ImGuiLayer::onAttach()
{
    Rapture::GE_INFO("ImGui Layer Attached");
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Apply the custom style
    ImGuiPanelStyle::InitializeFonts();

    // Enable keyboard controls, docking, and viewports
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport
    io.FontGlobalScale = m_FontScale;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled, tweak WindowRounding/WindowBg so platform windows look identical to regular ones
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }


    // Get window context from application
    GLFWwindow* window = static_cast<GLFWwindow*>(Rapture::Application::getInstance().getWindowContext().getNativeWindowContext());
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    
    // Initialize the SettingsPanel with the window context
    m_SettingsPanel = new SettingsPanel(&Rapture::Application::getInstance().getWindowContext());
    
    // Initialize the AssetsPanel with the project root directory
    // Use a valid absolute path that exists to avoid crashes
    std::string currentPath = std::filesystem::current_path().string();
    m_AssetsPanel.setRootDirectory(currentPath);
    
    // Standard ImGui style overrides
    style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.4f, 0.8f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2f, 0.4f, 0.8f, 0.65f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.2f, 0.4f, 0.8f, 0.80f);

    ImGuiPanelStyle::InitializeStyle();

    // Initialize TextureViewPanel with validation
    try {
        m_TextureViewPanel.setPanelName("Texture Viewer");
        Rapture::GE_INFO("ImGuiLayer: TextureViewPanel initialized successfully");
    } catch (const std::exception& e) {
        Rapture::GE_ERROR("ImGuiLayer: Failed to initialize TextureViewPanel: {}", e.what());
    } catch (...) {
        Rapture::GE_ERROR("ImGuiLayer: Unknown error initializing TextureViewPanel");
    }
    
    // Connect TextureViewPanel to PropertiesPanel via callback with validation
    try {
        auto callback = getTextureViewCallback();
        if (callback) {
            m_PropertiesPanel.setTextureViewCallback(callback);
            Rapture::GE_INFO("ImGuiLayer: Texture view callback set successfully");
        } else {
            Rapture::GE_ERROR("ImGuiLayer: Failed to create texture view callback");
        }
    } catch (const std::exception& e) {
        Rapture::GE_ERROR("ImGuiLayer: Failed to set texture view callback: {}", e.what());
    } catch (...) {
        Rapture::GE_ERROR("ImGuiLayer: Unknown error setting texture view callback");
    }
}

void ImGuiLayer::onDetach()
{
    // Clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (m_SettingsPanel) {
        delete m_SettingsPanel;
        m_SettingsPanel = nullptr;
    }
    
    Rapture::GE_INFO("ImGui Layer Detached");
}

void ImGuiLayer::begin()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

}

void ImGuiLayer::end()
{
    // Rendering
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void ImGuiLayer::onUpdate(float ts)
{
    // Start ImGui frame
    begin();
    RAPTURE_PROFILE_GPU_SCOPE("ImGui Layer");
    RAPTURE_PROFILE_SCOPE("ImGui Layer");
    
    // Store TestLayer pointer for later use throughout the function
    TestLayer* testLayer = nullptr;
    
    // Use SceneManager instead of looking for TestLayer directly
    auto activeScene = Rapture::SceneManager::getInstance().getActiveScene();
    if (activeScene) {
        // Get reference to TestLayer for its framebuffer and other functionality
        testLayer = dynamic_cast<TestLayer*>(
            Rapture::Application::getInstance().getLayerByName("Test Layer"));
            
        if (testLayer) {
            // Set the ViewportPanel reference in TestLayer
            testLayer->setViewportPanel(&m_ViewportPanel);
            
            // Set up callbacks for ImGuizmo functionality
            
            // 1. Camera matrices callback: TestLayer -> ViewportPanel
            testLayer->setCameraMatricesCallback([this](const glm::mat4& view, const glm::mat4& projection) {
                m_ViewportPanel.setCameraMatrices(view, projection);
            });
            
            // 2. Entity transform callback: ViewportPanel -> Entity
            m_ViewportPanel.setEntityTransformCallback([](std::shared_ptr<Rapture::Entity> entity, glm::mat4& outMatrix) {
                if (!entity || !entity->isValid()) 
                    return false;
                
                auto* transformComponent = entity->tryGetComponent<Rapture::TransformComponent>();
                if (!transformComponent)
                    return false;
                
                // Get the transform matrix from the transforms struct
                outMatrix = transformComponent->transforms.getTransform();
                return true;
            });
            
            // 3. Entity selection callback: TestLayer -> ImGuiLayer
            testLayer->setEntitySelectedCallback([this](std::shared_ptr<Rapture::Entity> entity) {
                if (entity) {
                    // Update our selected entity when TestLayer selects something via raycast
                    m_SelectedEntity = entity;
                }
            });
        }
    }

    // ImGuizmo keyboard shortcuts
    if (m_SelectedEntity && m_SelectedEntity->isValid()) {
        // Only handle shortcuts if we have a selected entity
        ImGuiIO& io = ImGui::GetIO();
        
        // Don't process shortcuts if ImGui is capturing keyboard
        if (!io.WantCaptureKeyboard) {
            // 1 key for Translate
            if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                m_ViewportPanel.setGizmoOperation(ImGuizmo::TRANSLATE);
                Rapture::GE_INFO("Gizmo mode set to Translate");
            }
            // 2 key for Rotate
            else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                m_ViewportPanel.setGizmoOperation(ImGuizmo::ROTATE);
                Rapture::GE_INFO("Gizmo mode set to Rotate");
            }
            // 3 key for Scale
            else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
                m_ViewportPanel.setGizmoOperation(ImGuizmo::SCALE);
                Rapture::GE_INFO("Gizmo mode set to Scale");
            }
            // Space to toggle between local and world mode
            else if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
                m_ViewportPanel.toggleGizmoMode();
                const char* modeName = (m_ViewportPanel.getCurrentGizmoMode() == ImGuizmo::LOCAL) ? "Local" : "World";
                Rapture::GE_INFO("Gizmo coordinate system set to {0}", modeName);
            }
        }
    }
    
    // Setup docking space
    static bool dockspaceOpen = true;
    static bool opt_fullscreen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each other.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    
    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;
    
    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
    ImGui::PopStyleVar();
    
    if (opt_fullscreen)
        ImGui::PopStyleVar(2);
    
    // Submit the DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit")) { /* Handle exit */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Scene Viewport", nullptr, true)) { /* Toggle Scene Viewport */ }
            if (ImGui::MenuItem("Properties", nullptr, true)) { /* Toggle Properties Panel */ }
            if (ImGui::MenuItem("Settings", nullptr, true)) { /* Toggle Settings Panel */ }
            if (ImGui::MenuItem("GBuffer Debug View", nullptr, &m_showGBufferDebug)) {
                m_DebugViewPanel.setEnabled(m_showGBufferDebug);
            }
            
            // Simple state sync for texture viewer
            bool textureViewerEnabled = m_TextureViewPanel.isEnabled();
            if (ImGui::MenuItem("Texture Viewer", nullptr, &textureViewerEnabled)) {
                m_TextureViewPanel.setEnabled(textureViewerEnabled);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Render all panels using our panel classes
    if (testLayer) {
        m_ViewportPanel.renderSceneViewport(testLayer);
        m_ViewportPanel.renderDepthBufferViewport(testLayer);
        //m_MaterialViewerPanel.render(testLayer->getMaterialFramebuffer(), testLayer->getMaterialViewerSphere());
    }
    
    //m_StatsPanel.render(ts);
    
    m_EntityBrowserPanel.render(activeScene.get(), 
        [this, testLayer](std::shared_ptr<Rapture::Entity> entity) {
            if (entity) {
                m_SelectedEntity = entity;
                
                // Keep TestLayer's selection in sync
                if (testLayer) { 
                    testLayer->setSelectedEntity(entity);
                }
            } else {
                Rapture::GE_WARN("No valid entity selected");
            }
        });
    
    m_PropertiesPanel.render(m_SelectedEntity);
    
    m_LogPanel.render();
    m_AssetsPanel.render(testLayer);
    
    // Render the GBuffer debug panel if enabled
    if (m_showGBufferDebug) {
        m_DebugViewPanel.render();
    }
    
    // Render texture viewer if enabled
    if (m_TextureViewPanel.isEnabled()) {
        m_TextureViewPanel.render();
    }
    
    // Render the settings panel
    if (m_SettingsPanel) {
        m_SettingsPanel->setActiveScene(activeScene);
        m_SettingsPanel->render();
    }
    
    ImGui::End(); // End DockSpace Demo
    
    // Finish ImGui frame
    end();
}


void ImGuiLayer::onEvent(Rapture::Event& event)
{
    // ImGui handles events through GLFW callbacks set up in ImGui_ImplGlfw_InitForOpenGL
}

// Update the showTexture method with better validation and error logging
void ImGuiLayer::showTexture(std::shared_ptr<Rapture::Texture2D> texture)
{
    // Detailed validation with specific error messages
    if (!texture) {
        Rapture::GE_ERROR("ImGuiLayer: Attempt to show null texture");
        return;
    }
    
    try {
        // Validate texture properties before setting
        uint32_t rendererID = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        
        try {
            rendererID = texture->getRendererID();
            width = texture->getWidth();
            height = texture->getHeight();
        } catch (const std::exception& e) {
            Rapture::GE_ERROR("ImGuiLayer: Failed to get texture properties: {}", e.what());
            return;
        } catch (...) {
            Rapture::GE_ERROR("ImGuiLayer: Unknown error getting texture properties");
            return;
        }
        
        if (rendererID == 0) {
            Rapture::GE_ERROR("ImGuiLayer: Cannot show texture with invalid ID (0)");
            return;
        }
        
        if (width == 0 || height == 0) {
            Rapture::GE_WARN("ImGuiLayer: Texture has suspicious dimensions ({}x{})", width, height);
        }
        
        // Log successful validation
        Rapture::GE_INFO("ImGuiLayer: Texture validated (ID: {}, Size: {}x{})", rendererID, width, height);
        
        // Set the texture in the viewer
        try {
            m_TextureViewPanel.setTexture(texture);
            Rapture::GE_INFO("ImGuiLayer: Texture set in viewer");
        } catch (const std::exception& e) {
            Rapture::GE_ERROR("ImGuiLayer: Exception while setting texture in viewer: {}", e.what());
            return;
        } catch (...) {
            Rapture::GE_ERROR("ImGuiLayer: Unknown exception while setting texture in viewer");
            return;
        }
        
        // Ensure panel is enabled/visible
        try {
            bool wasEnabled = m_TextureViewPanel.isEnabled();
            m_TextureViewPanel.setEnabled(true);
            
            if (!wasEnabled) {
                Rapture::GE_INFO("ImGuiLayer: Texture viewer panel was not enabled, enabling now");
            }
        } catch (...) {
            Rapture::GE_ERROR("ImGuiLayer: Failed to enable texture viewer panel");
            return;
        }
        
        Rapture::GE_INFO("ImGuiLayer: Successfully prepared texture for viewing (ID: {})", rendererID);
    }
    catch (const std::exception& e) {
        Rapture::GE_ERROR("ImGuiLayer: Exception in showTexture: {}", e.what());
    }
    catch (...) {
        Rapture::GE_ERROR("ImGuiLayer: Unknown exception in showTexture");
    }
}

