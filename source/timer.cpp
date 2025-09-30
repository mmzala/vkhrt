#include "timer.hpp"

Timer::Timer()
{
    Reset();
}

DeltaMS Timer::GetElapsed() const
{
    return std::chrono::duration_cast<DeltaMS>(std::chrono::high_resolution_clock::now() - _start);
}

void Timer::Reset()
{
    _start = std::chrono::high_resolution_clock::now();
}
