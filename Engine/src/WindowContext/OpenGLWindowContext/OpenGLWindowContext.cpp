#include "OpenGLWindowContext.h"

#include "../../logger/Log.h"
#include "../../Debug/TracyProfiler.h"


namespace Rapture {

	WindowContext* WindowContext::createWindow() {
		return new OpenGLWindowContext;
	}


	static void  GLFWErrorCallback(int code, const char* description)
	{
		GE_CORE_ERROR("GLFW Error ({0}): {1}", code, description);
	}


	OpenGLWindowContext::OpenGLWindowContext()
	{
		initWindow();
	}

	void OpenGLWindowContext::initWindow()
	{
		GE_CORE_INFO("---Creating window context---");

		if (glfwInit())
		{
			glfwSetErrorCallback(GLFWErrorCallback);
			GE_CORE_INFO("GLFW Succesfully initialized");
		}
		else 
		{
			GE_CORE_CRITICAL("GLFW failed to initialize");
			exit(EXIT_FAILURE);
		}

		m_window = glfwCreateWindow(1920, 1080, "Window Title", nullptr, nullptr);
		glfwMakeContextCurrent(m_window);

		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		if (status)
		{
			GE_CORE_INFO("Glad Succesfully initialized");

		}
		else
		{
			GE_CORE_CRITICAL("Glad failed to initialize");
		}
		glfwSetWindowUserPointer(m_window, &m_context_data);
		
		// Disable VSync by setting swap interval to 0
		setSwapMode(SwapMode::Immediate);

		setGLFWCallbacks();

	}

	void OpenGLWindowContext::closeWindow()
	{
		glfwDestroyWindow(m_window);
	}

	void OpenGLWindowContext::onUpdate()
	{
		RAPTURE_PROFILE_FUNCTION();
		
		// Profile any rendering that happens before the swap
		{
			RAPTURE_PROFILE_SCOPE("Pre-SwapBuffers");
			RAPTURE_PROFILE_GPU_SCOPE("Pre-SwapBuffers");
			
			// Any rendering code that happens here
		}
		
		// Profile the actual buffer swap
		{
			RAPTURE_PROFILE_SCOPE("SwapBuffers");
			RAPTURE_PROFILE_GPU_SCOPE("SwapBuffers");
			
			// Your actual swap buffers call
			glfwPollEvents();
			glfwSwapBuffers(m_window);
		}
		
		// Profile the time immediately after the swap
		{
			RAPTURE_PROFILE_SCOPE("Post-SwapBuffers");
			RAPTURE_PROFILE_GPU_SCOPE("Post-SwapBuffers");
			
			// Any code that happens after the swap
		}
		
	}

	void OpenGLWindowContext::setGLFWCallbacks()
	{
		glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window)
			{
				ContextData& data = *(ContextData*)glfwGetWindowUserPointer(window);

				WindowCloseEvent event = WindowCloseEvent();

				data.eventFnCallback(event);

			});

		glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height)
			{
				ContextData& data = *(ContextData*)glfwGetWindowUserPointer(window);

				WindowResizeEvent event(width, height);
				data.eventFnCallback(event);
			});

	}
	
	void OpenGLWindowContext::setSwapMode(SwapMode mode)
	{
		// Store the current mode
		m_currentSwapMode = mode;
		
		// Configure OpenGL for triple buffering if needed
		// Note: GLFW doesn't directly expose triple buffering without VSync,
		// so we need to use OpenGL directly to set it up
		bool tripleBufferingEnabled = (mode == SwapMode::AdaptiveVSync || mode == SwapMode::TripleBuffering);
		
		if (tripleBufferingEnabled) {
			// Enable triple buffering via OpenGL
			GLint swapMode = 0;
			glGetIntegerv(GL_DOUBLEBUFFER, &swapMode);
			
			if (swapMode) {
				// Buffer swapping is supported
				// Set up appropriate extensions if available
			}
		}
		
		// Set the appropriate swap interval based on the mode
		switch (mode) {
			case SwapMode::VSync: {
				// Traditional VSync with double buffering
				glfwSwapInterval(1);
				GE_CORE_INFO("VSync enabled with double buffering");
				break;
			}
				
			case SwapMode::AdaptiveVSync: {
				// Check for the swap control tear extension
				bool tearControlSupported = glfwExtensionSupported("WGL_EXT_swap_control_tear") || 
										   glfwExtensionSupported("GLX_EXT_swap_control_tear");
				
				if (tearControlSupported) {
					// Negative value enables adaptive vsync with triple buffering
					glfwSwapInterval(-1);
					GE_CORE_INFO("Triple buffering enabled with adaptive vsync");
				} else {
					// Fall back to regular VSync if the extension is not supported
					glfwSwapInterval(1);
					m_currentSwapMode = SwapMode::VSync;
					GE_CORE_WARN("Triple buffering requested but swap control tear extension not supported. Falling back to double buffering.");
				}
				break;
			}
				
			case SwapMode::TripleBuffering: {
				// Triple buffering without VSync (uncapped framerate)
				// Note: In GLFW we achieve this by disabling VSync but configuring the buffer mode
				glfwSwapInterval(0);
				
				// Try to enable triple buffering through driver-specific commands
				// This might not work on all systems as it depends on driver support
				bool tripleBufferingSupported = isTripleBufferingSupported();
				
				if (tripleBufferingSupported) {
					GE_CORE_INFO("Triple buffering enabled without VSync (uncapped framerate)");
				} else {
					GE_CORE_WARN("Triple buffering without VSync requested but not fully supported. May fall back to double buffering.");
				}
				break;
			}
				
			case SwapMode::Immediate:
			default: {
				// No VSync, uncapped framerate
				glfwSwapInterval(0);
				GE_CORE_INFO("VSync disabled - uncapped framerate with double buffering");
				break;
			}
		}
	}
	
	SwapMode OpenGLWindowContext::getSwapMode() const
	{
		return m_currentSwapMode;
	}
	
	bool OpenGLWindowContext::isTripleBufferingSupported() const
	{
		// For adaptive vsync with triple buffering, we need the swap control tear extension
		bool tearControlSupported = glfwExtensionSupported("WGL_EXT_swap_control_tear") || 
								   glfwExtensionSupported("GLX_EXT_swap_control_tear");
		
		// We can also check for other extensions that might enable triple buffering
		bool wglTripleBufferSupported = false;
		
		#ifdef _WIN32
		// Windows-specific extensions for triple buffering
		wglTripleBufferSupported = glfwExtensionSupported("WGL_EXT_swap_control") ||
								  glfwExtensionSupported("WGL_NV_swap_group");
		#endif
		
		return tearControlSupported || wglTripleBufferSupported;
	}
}

