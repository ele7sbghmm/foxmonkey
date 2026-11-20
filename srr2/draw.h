#pragma once

#include <cstdint>
#include <d3dx8.h>

class WorldScene;
class SpatialTreeIter;
class SpatialTree;
class SpatialNode;
class FenceEntityDSG;

namespace Draw {

//------------------------------------------------------------------------------
// Vertex formats & per-frame batches
//------------------------------------------------------------------------------
// Using two separate 3D line batches (depth-tested vs. X-ray overlay) keeps the
// GPU draw cost down to at most 2 DrawPrimitiveUP calls for all 3D wireframes.
struct Vertex3D {
    D3DXVECTOR3 xyz;
    DWORD color;
};

// XYZRHW bypasses the World/View/Projection matrices: (x, y) are pixel coords.
struct Vertex2D {
    float x, y, z, rhw;
    DWORD color;
};

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
    constexpr uint32_t Grey    = RGBA(128, 128, 128);
    constexpr uint32_t Black   = RGBA(  0,   0,   0);
    constexpr uint32_t Orange  = RGBA(255, 128,   0);
}

void Init(IDirect3DDevice8* device);

// 3D world-space primitives (batched until Flush()).
void AddLine(const Vec3& a, const Vec3& b, uint32_t color = Color::White, bool depthTest = true);
void AddRay(const Vec3& origin, const Vec3& dir, float length, uint32_t color = Color::Yellow, bool depthTest = true);
void AddArrow(const Vec3& a, const Vec3& b, float headSize = 0.25f, uint32_t color = Color::Yellow, bool depthTest = true);
void AddPlus(const Vec3& center, float y, float size, uint32_t color, bool depthTest);
void AddCross(const Vec3& center, float size = 0.25f, uint32_t color = Color::White, bool depthTest = true);
void AddAxes(const Vec3& origin, const Vec3& right, const Vec3& up, const Vec3& forward, float scale = 1.0f, bool depthTest = true);
void AddCircle(const Vec3& center, const Vec3& normal, float radius, uint32_t color = Color::Green, int segments = 24, bool depthTest = true);
void AddCircle(const Vec3& center, float radius, uint32_t color = Color::Green, int segments = 24, bool depthTest = true);
void AddSphere(const Vec3& center, float radius, uint32_t color = Color::Cyan, int segments = 24, bool depthTest = true);
void AddAABB(const Vec3& min, const Vec3& max, uint32_t color = Color::Red, bool depthTest = true);
void AddBox(const Vec3& center, const Vec3& halfExtents, uint32_t color = Color::Red, bool depthTest = true);

// 2D screen-space primitives (pixel coordinates, origin at top-left).
void AddLine2D(float x0, float y0, float x1, float y1, uint32_t color = Color::White);
void AddRect2D(float x, float y, float w, float h, uint32_t color = Color::White, bool filled = false);
void AddCircle2D(float cx, float cy, float radius, uint32_t color = Color::White, int segments = 24);

// Draws all queued 3D and 2D primitives and clears the frame batches.
void Flush();
void FlushRHW();

void AddFencesSpatialTree(const SpatialTree* tree);
void AddFence(const FenceEntityDSG* fence, bool active, bool overflow);
void AddNodeAxis(const SpatialNode* node, bool isCurrentLeaf, bool intersect);
void DrawNode(const SpatialNode* node, bool isCurrentLeaf, bool intersect);
void DrawAllNodes(const WorldScene* tree);

}
