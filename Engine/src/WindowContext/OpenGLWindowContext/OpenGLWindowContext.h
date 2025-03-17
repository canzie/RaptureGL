#include "../WindowContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Rapture {

	class OpenGLWindowContext : public WindowContext {

	public:
		OpenGLWindowContext();
		~OpenGLWindowContext(){ closeWindow(); }

		virtual void initWindow(void) override;
		virtual void closeWindow(void) override;

		virtual void onUpdate(void) override;

		virtual void* getNativeWindowContext() override { return m_window; };


	private:
		void setGLFWCallbacks();

	private:
		GLFWwindow* m_window;


	};


}