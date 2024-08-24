#pragma once

#include <iostream>

namespace Nothofagus
{

/** Convenience class to measure simple performance metrics */
class PerformanceMonitor
{
private:
    float _currentTime;
    float _timer;
    float _period;
    int _framesCounter;
    float _framesPerSecond;
    float _milisecondsPerFrame;

public:
    /** Set the first reference time and the period of time over to compute the average frames per second */
    PerformanceMonitor(float currentTime, float period):
        _currentTime(currentTime),
        _timer(0.0f),
        _period(period),
        _framesCounter(0),
        _framesPerSecond(0.0f),
        _milisecondsPerFrame(0.0f)
    {}

    /** It must be called once per frame to update the internal metrics */
    void update(float currentTime);

    inline float getFPS() const { return _framesPerSecond; }

    inline float getMS() const { return _milisecondsPerFrame; }
};

std::ostream& operator<<(std::ostream& os, const PerformanceMonitor& perfMonitor);

}