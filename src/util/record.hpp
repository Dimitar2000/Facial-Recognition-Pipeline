#pragma once

#include <chrono>
#include <cstdio>
#include <string>

class TimeRecorder
{
    public:
        using Clock = std::chrono::steady_clock;

        void start(std::string&& w)
        {
            work = w;
            clock_time_base = Clock::now();
        }

        void stop()
        {
            auto elapsed = Clock::now() - clock_time_base;
            auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);

            std::printf("%-15s :%10lld\n", work.c_str(), static_cast<long long>(elapsed_ns.count()));
        }

    private:
        std::string work;
        Clock::time_point clock_time_base;
   
};