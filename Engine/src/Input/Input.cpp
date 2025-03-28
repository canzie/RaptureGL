#include "Input.h"

#include <GLFW/glfw3.h>

#include "../WindowContext/Application.h"
#include "../Logger/Log.h"

namespace Rapture
{



	bool Input::isKeyPressed(int keycode)
	{
		auto window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());

		int state = glfwGetKey(window, keycode);

		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::isMouseBtnPressed(int btn)
	{
		auto window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());

		auto state = glfwGetMouseButton(window, btn);

		return state == GLFW_PRESS;
	}

    bool Input::isMouseBtnReleased(int btn)
    {
        auto window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());

        auto state = glfwGetMouseButton(window, btn);

        return state == GLFW_RELEASE;
    }


	std::pair<double, double> Input::getMousePos()
	{
		auto window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return { xpos, ypos };
	}

	void Input::disableMouseCursor()
	{
		auto window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	void Input::enableMouseCursor()
	{
		auto window = static_cast<GLFWwindow*>(Application::getInstance().getWindowContext().getNativeWindowContext());
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

}