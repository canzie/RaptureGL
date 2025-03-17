#include "Timestep.h"

namespace Rapture
{
	std::chrono::seconds Timestep::getSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(m_time);
	}

	std::chrono::milliseconds Timestep::getMilliSeconds()
	{
		return m_time;
	}

	std::chrono::milliseconds Timestep::deltaTimeMs()
	{
		return m_time-m_lastFrameTime;
	}

	void Timestep::onUpdate()
	{
		m_lastFrameTime = m_time;
		m_time = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch());
	}

	std::chrono::milliseconds Timestep::m_time = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());

	std::chrono::milliseconds Timestep::m_lastFrameTime = m_time;

}

/**




			{

		};
	*/