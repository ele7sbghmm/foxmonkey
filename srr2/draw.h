#pragma once

#include <cstdint>
#include <d3dx8.h>

namespace Draw {

using Vec3 = D3DXVECTOR3;

// Packs 8-bit channels into D3DCOLOR (0xAARRGGBB).
constexpr uint32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

namespace Color {
    constexpr uint32_t Red     = RGBA(255,   0,   0);
    constexpr uint32_t Green   = RGBA(  0, 255,   0);
    constexpr uint32_t Blue    = RGBA(  0,   0, 255);
    constexpr uint32_t Yellow  = RGBA(255, 255,   0);
    constexpr uint32_t Cyan    = RGBA(  0, 255, 255);
    constexpr uint32_t Magenta = RGBA(255,   0, 255);
    constexpr uint32_t White   = RGBA(255, 255, 255);
    constexpr uint32_t Black   = RGBA(  0,   0,   0);
    constexpr uint32_t Orange  = RGBA(255, 128,   0);
}

void Init(IDirect3DDevice8* device);

// 3D world-space primitives (batched until Flush()).
void AddLine(Vec3 a, Vec3 b, uint32_t color = Color::White, bool depthTest = true);
void AddRay(Vec3 origin, Vec3 dir, float length, uint32_t color = Color::Yellow, bool depthTest = true);
void AddArrow(Vec3 a, Vec3 b, float headSize = 0.25f, uint32_t color = Color::Yellow, bool depthTest = true);
void AddCross(Vec3 center, float size = 0.25f, uint32_t color = Color::White, bool depthTest = true);
void AddAxes(Vec3 origin, Vec3 right = {1,0,0}, Vec3 up = {0,1,0}, Vec3 forward = {0,0,1}, float scale = 1.0f, bool depthTest = true);
void AddCircle(Vec3 center, Vec3 normal, float radius, uint32_t color = Color::Green, int segments = 24, bool depthTest = true);
void AddCircle(Vec3 center, float radius, uint32_t color = Color::Green, int segments = 24, bool depthTest = true);
void AddSphere(Vec3 center, float radius, uint32_t color = Color::Cyan, int segments = 24, bool depthTest = true);
void AddAABB(Vec3 min, Vec3 max, uint32_t color = Color::Red, bool depthTest = true);
void AddBox(Vec3 center, Vec3 halfExtents, uint32_t color = Color::Red, bool depthTest = true);

// 2D screen-space primitives (pixel coordinates, origin at top-left).
void AddLine2D(float x0, float y0, float x1, float y1, uint32_t color = Color::White);
void AddRect2D(float x, float y, float w, float h, uint32_t color = Color::White, bool filled = false);
void AddCircle2D(float cx, float cy, float radius, uint32_t color = Color::White, int segments = 24);

// Draws all queued 3D and 2D primitives and clears the frame batches.
void Flush();
void FlushRHW();

}
