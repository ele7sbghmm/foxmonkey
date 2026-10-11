#pragma once

#include <chrono>
#include <thread>
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#if defined(_M_X64) || defined(_M_IX86)
#include <emmintrin.h>
#endif

class FPS {
public:
    FPS() = default;
    void SetFPS(float fps);
    void Start();
    void End();
    void Wait() const;

    bool use_fake_elapsed = false;
    unsigned fake_elapsed = 8;
    float target_fps = 200.;
    bool cap_fps = false;

    std::chrono::duration<float, std::milli> target_duration
        = std::chrono::duration<float, std::milli>(1'000. / 100.);
    std::chrono::steady_clock::time_point target_time{};
    std::chrono::steady_clock::time_point frame_start{};
    std::chrono::steady_clock::time_point frame_end{};

    long long second{};
    uint32_t frames_this_second = 0;
    uint32_t total_frames_last_second = 0;
};

inline FPS fps{};
