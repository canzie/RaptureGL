#include "ImGuiLayer.h"
#include "WindowContext/Application.h"
#include "TestLayer.h"
#include "Logger/Log.h"
#include "Scenes/Components/Components.h"
#include "Scenes/Entity.h"
#include <glm/gtc/type_ptr.hpp>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

namespace Rapture {

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
    
    // Enable keyboard controls, docking, and viewports
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport

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
    GLFWwindow* window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
}

void ImGuiLayer::onDetach()
{
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    Rapture::GE_INFO("ImGui Layer Detached");
}

void ImGuiLayer::begin()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
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
    
    // Get access to the TestLayer to retrieve its framebuffer
    TestLayer* testLayer = nullptr;
    for (Layer* layer : Application::getInstance().getLayerStack())
    {
        if (TestLayer* tl = dynamic_cast<TestLayer*>(layer))
        {
            testLayer = tl;
            break;
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
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Scene Viewport Panel - displays the scene framebuffer
    ImGui::Begin("Scene Viewport");
    {
        // Get the size of the ImGui window viewport
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        
        if (testLayer)
        {
            // Get the framebuffer from the test layer
            unsigned int textureID = testLayer->getFramebuffer()->getColorAttachmentRendererID();
            
            // Check if viewport size changed and resize the framebuffer if needed
            static ImVec2 lastSize = ImVec2(0, 0);
            static bool firstTime = true;
            if (viewportPanelSize.x != lastSize.x || viewportPanelSize.y != lastSize.y || firstTime)
            {
                
                if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0)
                {
                    // Update framebuffer size to match viewport
                    testLayer->getFramebuffer()->resize(
                        static_cast<unsigned int>(viewportPanelSize.x), 
                        static_cast<unsigned int>(viewportPanelSize.y));
                }
                lastSize = viewportPanelSize;
                firstTime = false;
            }
            
            // Display the framebuffer texture in ImGui
            // ImGui::Image uses void* to store the texture ID, so we need to cast it
            ImTextureID texID = (ImTextureID)(intptr_t)textureID;
            ImGui::Image(texID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
        }
        else
        {
            ImGui::Text("Scene View not available");
        }
    }
    ImGui::End();
    
    // Depth Buffer Viewport - directly displays the depth buffer
    ImGui::Begin("Depth Buffer Viewport");
    {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        
        if (testLayer)
        {
            // Get the depth texture directly from the framebuffer
            unsigned int depthTexID = testLayer->getFramebuffer()->getDepthAttachmentRendererID();
            
            if (depthTexID)
            {
                // Display the depth texture (inverted Y coordinates to match OpenGL)
                ImTextureID texID = (ImTextureID)(intptr_t)depthTexID;
                ImGui::Image(texID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
                
                ImGui::Text("Raw depth buffer - may appear mostly black");
                ImGui::Text("The z-buffer stores non-linear depth values");
            }
            else
            {
                ImGui::Text("No depth attachment available");
            }
        }
        else
        {
            ImGui::Text("Depth buffer view not available");
        }
    }
    ImGui::End();
    
    // Stats Panel
    ImGui::Begin("Statistics");
    ImGui::Text("FPS: %.1f", 1.0f / ts);
    ImGui::Text("Frame Time: %.3f ms", ts * 1000.0f);
    ImGui::End();
    
    // Properties Panel (for selecting and editing objects)
    ImGui::Begin("Properties");
    
    // Access the scene registry from the TestLayer
    if (testLayer)
    {
        auto& registry = testLayer->getActiveScene()->getRegistry();
        
        // Entity selector
        static int selectedEntityIndex = 0;
        
        // Get all entities with transform components
        auto view = registry.view<Rapture::TransformComponent>();
        
        if (view.size() > 0)
        {
            // Create a vector to store entity handles for UI selection
            std::vector<entt::entity> entities;
            for (auto entity : view)
            {
                entities.push_back(entity);
            }
            
            // Cap the selected index to valid range
            if (selectedEntityIndex >= entities.size())
                selectedEntityIndex = entities.size() - 1;
            
            // Dropdown to select an entity
            if (ImGui::BeginCombo("Entity", std::to_string((uint32_t)entities[selectedEntityIndex]).c_str()))
            {
                for (int i = 0; i < entities.size(); i++)
                {
                    bool isSelected = (selectedEntityIndex == i);
                    if (ImGui::Selectable(std::to_string((uint32_t)entities[i]).c_str(), isSelected))
                        selectedEntityIndex = i;
                    
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            
            // Get the selected entity
            entt::entity selectedEntity = entities[selectedEntityIndex];
            Rapture::Entity entity(selectedEntity, testLayer->getActiveScene().get());
            
            // Edit Transform component
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& transform = entity.getComponent<Rapture::TransformComponent>();
                
                // Position with lock option
                static bool positionLocked = false;
                ImGui::BeginGroup();
                if (ImGui::DragFloat3("Position", glm::value_ptr(transform.translation), 0.1f))
                {
                    // Position changed by UI
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("##posLock", &positionLocked))
                {
                    // Lock state changed
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock position axes");
                ImGui::EndGroup();
                
                // Rotation with lock option
                static bool rotationLocked = false;
                ImGui::BeginGroup();
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(transform.rotation), 0.1f))
                {
                    // Rotation changed by UI
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("##rotLock", &rotationLocked))
                {
                    // Lock state changed
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock rotation axes");
                ImGui::EndGroup();
                
                // Scale with lock option (maintain aspect ratio)
                static bool scaleLocked = false;
                static glm::vec3 lastScale = transform.scale;
                
                ImGui::BeginGroup();
                
                // Store original scale
                glm::vec3 originalScale = transform.scale;
                
                if (ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f))
                {
                    // Scale changed by UI
                    if (scaleLocked)
                    {
                        // Find which component changed
                        float ratioX = originalScale.x != 0.0f ? transform.scale.x / originalScale.x : 1.0f;
                        float ratioY = originalScale.y != 0.0f ? transform.scale.y / originalScale.y : 1.0f;
                        float ratioZ = originalScale.z != 0.0f ? transform.scale.z / originalScale.z : 1.0f;
                        
                        // Determine which component changed the most
                        float ratio = 1.0f;
                        if (abs(ratioX - 1.0f) > abs(ratioY - 1.0f) && abs(ratioX - 1.0f) > abs(ratioZ - 1.0f))
                            ratio = ratioX;
                        else if (abs(ratioY - 1.0f) > abs(ratioZ - 1.0f))
                            ratio = ratioY;
                        else
                            ratio = ratioZ;
                        
                        // Update all components with the same ratio
                        transform.scale = originalScale * ratio;
                    }
                    
                    lastScale = transform.scale;
                }
                
                ImGui::SameLine();
                if (ImGui::Checkbox("##scaleLock", &scaleLocked))
                {
                    // Lock state changed
                }
                
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Lock scale (maintain aspect ratio)");
                
                ImGui::EndGroup();
            }
            

        }
        else
        {
            ImGui::Text("No entities with transform components found.");
        }
    }
    else
    {
        ImGui::Text("No active scene available.");
    }
    
    ImGui::End();
    
    ImGui::End(); // End DockSpace Demo
    
    // Finish ImGui frame
    end();
}

void ImGuiLayer::onEvent(Event& event)
{
    // ImGui handles events through GLFW callbacks set up in ImGui_ImplGlfw_InitForOpenGL
}

} 