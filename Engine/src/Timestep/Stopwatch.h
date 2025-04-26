#include <ctime>
#include <chrono>

namespace Rapture {

    class Stopwatch {
        public:
            Stopwatch();

            void start();
            void stop();
            void reset();
            double getElapsedTime() const;
            double getElapsedTimeNano() const;
        private:
            std::chrono::high_resolution_clock::time_point m_startTime;
            std::chrono::high_resolution_clock::time_point m_endTime;
            bool m_isRunning;
    };
}
