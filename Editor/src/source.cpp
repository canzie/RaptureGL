#include "WindowContext/Application.h"
#include "Logger/Log.h"
#include "TestLayer.h"
#include "ImGuiLayer.h"
#include "AppEntryPoint.h"

// The main Editor application class
class EditorApp : public Rapture::Application {
public:
    EditorApp() {
        m_debugName = "LiDAR Editor";
        
        // Log startup message
        Rapture::GE_INFO("LiDAR Editor starting up...");
        
        // Push main editor layer
        pushLayer(new TestLayer());
        
        // Push ImGui layer as an overlay so it renders on top
        pushOverlay(new Rapture::ImGuiLayer());
    }
    
    ~EditorApp() {
        Rapture::GE_INFO("LiDAR Editor shutting down...");
    }
};

// Implementation of the function declared in AppEntryPoint.h
Rapture::Application* Rapture::CreateWindow() {
    return new EditorApp();
}