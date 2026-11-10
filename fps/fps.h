#pragma once

namespace srr2
{
namespace fps
{

// Waits until the target frame duration has elapsed using std::chrono::steady_clock.
// Call once per frame in the main game loop.
void Limit();

bool IsEnabled();
void SetEnabled(bool enabled);
bool* GetEnabledPtr();

int GetTargetFps();
void SetTargetFps(int targetFps);
int* GetTargetFpsPtr();

float GetFrameTimeMs();
float GetCurrentFps();

} // namespace fps
} // namespace srr2
