#include "WindowContext/Application.h"
#include "Logger/Log.h"
#include "AppEntryPoint.h"

// The main entry point of the application
int main(int argc, char** argv) {
    // Initialize logging first (only once)
    Rapture::Log::Init();
    
    // Create the editor application
    auto* app = Rapture::CreateWindow();
    
    if (app) {
        // Simple log without format string
        Rapture::GE_INFO("Starting application");
        
        // Run the application
        app->Run();
        
        // Cleanup
        delete app;
    }
    
    return 0;
}

