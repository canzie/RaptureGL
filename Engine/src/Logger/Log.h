#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>


namespace Rapture {

	class Log
	{
	public:
		static void Init();

		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

}

// Core log macros
#define GE_CORE_TRACE(...)    Log::GetCoreLogger()->trace(__VA_ARGS__)
#define GE_CORE_INFO(...)     Log::GetCoreLogger()->info(__VA_ARGS__)
#define GE_CORE_WARN(...)     Log::GetCoreLogger()->warn(__VA_ARGS__)
#define GE_CORE_ERROR(...)    Log::GetCoreLogger()->error(__VA_ARGS__)
#define GE_CORE_CRITICAL(...) Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define GE_TRACE(...)         Log::GetClientLogger()->trace(__VA_ARGS__)
#define GE_INFO(...)          Log::GetClientLogger()->info(__VA_ARGS__)
#define GE_WARN(...)          Log::GetClientLogger()->warn(__VA_ARGS__)
#define GE_ERROR(...)         Log::GetClientLogger()->error(__VA_ARGS__)
#define GE_CRITICAL(...)      Log::GetClientLogger()->critical(__VA_ARGS__)