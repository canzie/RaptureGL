#include "Stopwatch.h"

namespace Rapture {

    Stopwatch::Stopwatch() : m_isRunning(false) {}


    void Stopwatch::start() {
        m_startTime = std::chrono::high_resolution_clock::now();
        m_isRunning = true;
    }

    void Stopwatch::stop() {
        m_endTime = std::chrono::high_resolution_clock::now();
        m_isRunning = false;
    }

    void Stopwatch::reset() {
        m_startTime = std::chrono::high_resolution_clock::now();
        m_endTime = std::chrono::high_resolution_clock::now();
        m_isRunning = false;
    }

    double Stopwatch::getElapsedTime() const {
        return std::chrono::duration_cast<std::chrono::duration<double>>(m_endTime - m_startTime).count();
    }

    double Stopwatch::getElapsedTimeNano() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(m_endTime - m_startTime).count();
    }

}
