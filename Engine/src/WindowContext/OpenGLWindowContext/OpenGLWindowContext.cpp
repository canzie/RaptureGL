#include "OpenGLWindowContext.h"

#include "../../logger/Log.h"


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
		glfwSwapInterval(0);
		GE_CORE_INFO("VSync disabled - uncapped framerate");

		setGLFWCallbacks();

	}

	void OpenGLWindowContext::closeWindow()
	{
		glfwDestroyWindow(m_window);
	}

	void OpenGLWindowContext::onUpdate()
	{
		glfwPollEvents();
		glfwSwapBuffers(m_window);
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
}

