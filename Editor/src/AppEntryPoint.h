#pragma once

namespace Rapture {
    class Application;
    
    // This function is implemented by the client application
    // and should return a new instance of the application
    Application* CreateWindow();
} 