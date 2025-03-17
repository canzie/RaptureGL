// main.cpp : Defines the entry point for the application.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Logger/Log.h"

#include "WindowContext/Application.h"

/*
int main(void)
{
    
    Rapture::Log::Init();

    //auto app = Rapture::Application();
    auto app = Rapture::CreateWindow();
    Rapture::GE_INFO(app->getDebugName());
    app->Run();
    

    delete app;
    

    return 0;
}

*/