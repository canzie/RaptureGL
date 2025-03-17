#include <ctime>
#include <chrono>


namespace Rapture
{

	class Timestep
	{

	public:

		static std::chrono::seconds getSeconds();
		static std::chrono::milliseconds getMilliSeconds();
		static std::chrono::milliseconds deltaTimeMs();

		static void onUpdate();

	private:
		static std::chrono::milliseconds m_time;
		static std::chrono::milliseconds m_lastFrameTime;
	};

}
	
