#include "moxingyi/jishiqi.h"

//#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <thread>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#if defined(_M_64) | defined(_M_IX86)
#include <emmintrin.h>
#endif

void 计时器::Begin(unsigned radNewTime) {
    End();

    auto new_time = rad_时长(radNewTime);
    rad_已经过 = new_time - rad_现在几点;
    rad_time = new_time;

    frame_start = 时钟::now();
}
void 计时器::结束() {
    auto b = std::chrono::duration_cast<时钟::duration>(target_duration);
    auto work_end = 时钟::now();


    if (cap_fps)
        等待();
}
void 计时器::等待() const {
    while (target_time - 时钟::now() > std::chrono::milliseconds(2))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

#if defined(_M_X64) || defined(_M_IX86) || defined(_M_64) || defined(_M_IX86)
    while (时钟::now() < target_time) _mm_pause();
#endif
}
void 计时器::SetTargetFps(double fps) {
    if (fps > 0.)
        target_duration = 时长(1000. / fps);
}
