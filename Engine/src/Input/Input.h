#include <utility>

namespace Rapture
{

	class Input
	{
	public:
		static bool isKeyPressed(int keycode);

		static bool isMouseBtnPressed(int btn);

		static void disableMouseCursor();
		static void enableMouseCursor();

		static std::pair<double, double> getMousePos();
	};

}