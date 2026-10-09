#include "thingamajig/globals.h"

FrameLimiter g_frameLimiter{ g_fps };
Gui g_gui{};

double g_fps = 1000.;
double g_fps_gameplay = 60.;
