#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d8.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include <p3d/utility.hpp>
#include <p3d/context.hpp>
#include <pddi/dx8/display.hpp>
#include <worldsim/avatarmanager.h>
#include <worldsim/avatar.h>
#include <render/Culling/WorldScene.h>
#include <render/Culling/SpatialTreeIter.h>
#include <render/Culling/SpatialTree.h>
#include <render/Culling/SpatialNode.h>
#include <render/DSG/FenceEntityDSG.h>

#include "draw.h"
#include "server.h"

namespace Draw {

enum Axis : char { LEAF = -1, X, Y, Z };

constexpr DWORD FVF_3D = D3DFVF_XYZ | D3DFVF_DIFFUSE;
constexpr DWORD FVF_2D = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

static IDirect3DDevice8* s_device = nullptr;
static IDirect3DDevice8* s_stateBlockDevice = nullptr;
static DWORD             s_stateBlock = 0;

static std::vector<Vertex3D> s_linesDepth;
static std::vector<Vertex3D> s_linesOverlay;
static std::vector<Vertex2D> s_lines2D;
static std::vector<Vertex2D> s_tris2D;

static constexpr float kPi = 3.14159265358979323846f;

void Init(IDirect3DDevice8* device) {
    s_device = device;
}

//------------------------------------------------------------------------------
// Vector helpers
//------------------------------------------------------------------------------
static float Dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 Cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float LengthSq(Vec3 v) {
    return Dot(v, v);
}

static Vec3 Normalize(Vec3 v) {
    float lenSq = LengthSq(v);
    if (lenSq <= 1e-12f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    float invLen = 1.0f / std::sqrt(lenSq);
    return v * invLen;
}

// Builds two unit axes (u, v) spanning the plane perpendicular to unit normal n.
static void OrthonormalBasis(Vec3 n, Vec3& u, Vec3& v) {
    Vec3 ref = (std::fabs(n.y) < 0.99f) ? Vec3{ 0.0f, 1.0f, 0.0f } : Vec3{ 1.0f, 0.0f, 0.0f };
    u = Normalize(Cross(n, ref));
    v = Cross(n, u);
}

static D3DMATRIX IdentityMatrix() {
    D3DMATRIX m{};
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

static int ClampSegments(int segments) {
    if (segments < 3) return 3;
    if (segments > 4096) return 4096;
    return segments;
}

//------------------------------------------------------------------------------
// 3D Primitives (all decompose into AddLine)
//------------------------------------------------------------------------------
void AddLine(const Vec3& a, const Vec3& b, uint32_t color, bool depthTest) {
    auto& batch = depthTest ? s_linesDepth : s_linesOverlay;
    auto col = static_cast<DWORD>(color);
    batch.push_back({ { a.x, a.y, a.z }, col });
    batch.push_back({ { b.x, b.y, b.z }, col });
}

void AddRay(const Vec3& origin, const Vec3& dir, float length, uint32_t color, bool depthTest) {
    Vec3 n = Normalize(dir);
    if (LengthSq(n) <= 1e-12f) {
        return;
    }
    AddLine(origin, origin + n * length, color, depthTest);
}

void AddArrow(const Vec3& a, const Vec3& b, float headSize, uint32_t color, bool depthTest) {
    AddLine(a, b, color, depthTest);

    Vec3 delta = b - a;
    float lenSq = LengthSq(delta);
    if (lenSq <= 1e-10f) {
        return;
    }
    float len = std::sqrt(lenSq);
    Vec3 dir = delta * (1.0f / len);

    // Clamp arrowhead so very short arrows don't invert past the tail.
    float hs = (headSize > len * 0.5f) ? (len * 0.5f) : headSize;
    if (hs <= 0.0f) {
        return;
    }

    Vec3 u, v;
    OrthonormalBasis(dir, u, v);

    Vec3 base = b - dir * hs;
    float w = hs * 0.4f;
    AddLine(b, base + u * w, color, depthTest);
    AddLine(b, base - u * w, color, depthTest);
    AddLine(b, base + v * w, color, depthTest);
    AddLine(b, base - v * w, color, depthTest);
}

void AddPlus(const Vec3& center, float y, float size, uint32_t color, bool depthTest) {
    AddLine(Vec3{ center.x - size, y, center.z }, Vec3{ center.x + size, y, center.z }, color, depthTest);
    AddLine(Vec3{ center.x, y, center.z - size }, Vec3{ center.x, y, center.z + size }, color, depthTest);
}

void AddCross(const Vec3& center, float size, uint32_t color, bool depthTest) {
    AddLine(center - Vec3{ size, 0.0f, 0.0f }, center + Vec3{ size, 0.0f, 0.0f }, color, depthTest);
    AddLine(center - Vec3{ 0.0f, size, 0.0f }, center + Vec3{ 0.0f, size, 0.0f }, color, depthTest);
    AddLine(center - Vec3{ 0.0f, 0.0f, size }, center + Vec3{ 0.0f, 0.0f, size }, color, depthTest);
}

void AddAxes(const Vec3& origin, const Vec3& right, const Vec3& up, const Vec3& forward, float scale, bool depthTest) {
    AddLine(origin, origin + right   * scale, Color::Red,   depthTest);
    AddLine(origin, origin + up      * scale, Color::Green, depthTest);
    AddLine(origin, origin + forward * scale, Color::Blue,  depthTest);
}

void AddCircle(const Vec3& center, const Vec3& normal, float radius, uint32_t color, int segments, bool depthTest) {
    if (radius <= 0.0f) {
        return;
    }
    segments = ClampSegments(segments);

    Vec3 n = Normalize(normal);
    if (LengthSq(n) <= 1e-12f) {
        n = { 0.0f, 1.0f, 0.0f };
    }

    Vec3 u, v;
    OrthonormalBasis(n, u, v);

    const float step = (2.0f * kPi) / static_cast<float>(segments);
    Vec3 prev = center + u * radius;
    for (int i = 1; i <= segments; ++i) {
        float angle = (i == segments) ? 0.0f : (step * static_cast<float>(i));
        Vec3 curr = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
        AddLine(prev, curr, color, depthTest);
        prev = curr;
    }
}

void AddCircle(const Vec3& center, float radius, uint32_t color, int segments, bool depthTest) {
    AddCircle(center, Vec3{ 0.0f, 1.0f, 0.0f }, radius, color, segments, depthTest);
}

void AddSphere(const Vec3& center, float radius, uint32_t color, int segments, bool depthTest) {
    AddCircle(center, Vec3{ 1.0f, 0.0f, 0.0f }, radius, color, segments, depthTest);
    AddCircle(center, Vec3{ 0.0f, 1.0f, 0.0f }, radius, color, segments, depthTest);
    AddCircle(center, Vec3{ 0.0f, 0.0f, 1.0f }, radius, color, segments, depthTest);
}

void AddAABB(const Vec3& min, const Vec3& max, uint32_t color, bool depthTest) {
    Vec3 c[8] = {
        { min.x, min.y, min.z },
        { max.x, min.y, min.z },
        { max.x, min.y, max.z },
        { min.x, min.y, max.z },
        { min.x, max.y, min.z },
        { max.x, max.y, min.z },
        { max.x, max.y, max.z },
        { min.x, max.y, max.z },
    };

    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) & 3;
        AddLine(c[i],     c[next],     color, depthTest); // Bottom face
        AddLine(c[i + 4], c[next + 4], color, depthTest); // Top face
        AddLine(c[i],     c[i + 4],    color, depthTest); // Vertical edges
    }
}

void AddBox(const Vec3& center, const Vec3& halfExtents, uint32_t color, bool depthTest) {
    AddAABB(center - halfExtents, center + halfExtents, color, depthTest);
}

void AddRectXZ(float minX, float minZ, float maxX, float maxZ, float y, uint32_t color, bool depthTest) {
    Vec3 p0{ minX, y, minZ };
    Vec3 p1{ maxX, y, minZ };
    Vec3 p2{ maxX, y, maxZ };
    Vec3 p3{ minX, y, maxZ };
    AddLine(p0, p1, color, depthTest);
    AddLine(p1, p2, color, depthTest);
    AddLine(p2, p3, color, depthTest);
    AddLine(p3, p0, color, depthTest);
}

//------------------------------------------------------------------------------
// 2D Screen-space Primitives
//------------------------------------------------------------------------------
void AddLine2D(float x0, float y0, float x1, float y1, uint32_t color) {
    DWORD col = static_cast<DWORD>(color);
    s_lines2D.push_back({ x0, y0, 0.0f, 1.0f, col });
    s_lines2D.push_back({ x1, y1, 0.0f, 1.0f, col });
}

void AddRect2D(float x, float y, float w, float h, uint32_t color, bool filled) {
    if (!filled) {
        AddLine2D(x,     y,     x + w, y,     color);
        AddLine2D(x + w, y,     x + w, y + h, color);
        AddLine2D(x + w, y + h, x,     y + h, color);
        AddLine2D(x,     y + h, x,     y,     color);
        return;
    }

    DWORD col = static_cast<DWORD>(color);
    Vertex2D tl{ x,     y,     0.0f, 1.0f, col };
    Vertex2D tr{ x + w, y,     0.0f, 1.0f, col };
    Vertex2D br{ x + w, y + h, 0.0f, 1.0f, col };
    Vertex2D bl{ x,     y + h, 0.0f, 1.0f, col };

    s_tris2D.push_back(tl); s_tris2D.push_back(tr); s_tris2D.push_back(br);
    s_tris2D.push_back(tl); s_tris2D.push_back(br); s_tris2D.push_back(bl);
}

void AddCircle2D(float cx, float cy, float radius, uint32_t color, int segments) {
    if (radius <= 0.0f) {
        return;
    }
    segments = ClampSegments(segments);

    const float step = (2.0f * kPi) / static_cast<float>(segments);
    float px = cx + radius;
    float py = cy;
    for (int i = 1; i <= segments; ++i) {
        float angle = (i == segments) ? 0.0f : (step * static_cast<float>(i));
        float nx = cx + std::cos(angle) * radius;
        float ny = cy + std::sin(angle) * radius;
        AddLine2D(px, py, nx, ny, color);
        px = nx;
        py = ny;
    }
}

//------------------------------------------------------------------------------
// Flush: save Pure3D's D3D8 state, render batches via DrawPrimitiveUP, restore
//------------------------------------------------------------------------------
static void ClearBatches() {
    s_linesDepth.clear();
    s_linesOverlay.clear();
}

static void ClearBatchesRHW() {
    s_lines2D.clear();
    s_tris2D.clear();
}

static IDirect3DDevice8* ResolveDevice() {
    if (s_device) return s_device;
    if (p3d::display) {
        return static_cast<d3dDisplay*>(p3d::display)->GetD3DDevice();
    }
    return nullptr;
}

static bool ValidateStateBlock(IDirect3DDevice8* dev) {
    if (!dev) return false;

    if (s_stateBlockDevice != dev) {
        if (s_stateBlock && s_stateBlockDevice) {
            s_stateBlockDevice->DeleteStateBlock(s_stateBlock);
        }
        s_stateBlock = 0;
        s_stateBlockDevice = dev;
    }
    if (!s_stateBlock) {
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &s_stateBlock))) {
            return false;
        }
    } else if (FAILED(dev->CaptureStateBlock(s_stateBlock))) {
        dev->DeleteStateBlock(s_stateBlock);
        s_stateBlock = 0;
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &s_stateBlock))) {
            return false;
        }
    }
    return true;
}

static void SetupDebugRenderState(IDirect3DDevice8* dev) {
    // Configure clean fixed-function vertex-color rendering without modifying
    // Pure3D's shadowed d3dState (ApplyStateBlock restores everything).
    dev->SetPixelShader(0);
    dev->SetTexture(0, nullptr);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_COLORVERTEX, TRUE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
}

void Flush() {
    if (s_linesDepth.empty() && s_linesOverlay.empty()) return;

    SendVertices(s_linesOverlay);

    IDirect3DDevice8* dev = ResolveDevice();
    if (!dev || !p3d::context || !ValidateStateBlock(dev)) {
        ClearBatches();
        return;
    }

    SetupDebugRenderState(dev);

    // Pure3D leaves D3DTS_VIEW as Identity and multiplies World*View into
    // D3DTS_WORLD, while keeping the active perspective D3DTS_PROJECTION.
    D3DMATRIX world = IdentityMatrix();
    dev->SetTransform(D3DTS_WORLD, &world);
    dev->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(p3d::context->GetViewMatrix()));
    dev->SetVertexShader(FVF_3D);

    if (s_linesDepth.size() >= 2) {
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->DrawPrimitiveUP(D3DPT_LINELIST,
            static_cast<UINT>(s_linesDepth.size() / 2),
            s_linesDepth.data(), sizeof(Vertex3D));
    }

    if (s_linesOverlay.size() >= 2) {
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        dev->DrawPrimitiveUP(D3DPT_LINELIST,
            static_cast<UINT>(s_linesOverlay.size() / 2),
            s_linesOverlay.data(), sizeof(Vertex3D));
    }

    dev->ApplyStateBlock(s_stateBlock);
    ClearBatches();
}

void FlushRHW() {
    if (s_lines2D.empty() && s_tris2D.empty()) return;

    IDirect3DDevice8* dev = ResolveDevice();
    if (!dev || !p3d::context || !ValidateStateBlock(dev)) {
        ClearBatchesRHW();
        return;
    }

    SetupDebugRenderState(dev);

    // 2D screen-space batches: XYZRHW ignores World/View/Projection matrices.
    dev->SetVertexShader(FVF_2D);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    if (s_tris2D.size() >= 3) {
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,
            static_cast<UINT>(s_tris2D.size() / 3),
            s_tris2D.data(), sizeof(Vertex2D));
    }

    if (s_lines2D.size() >= 2) {
        dev->DrawPrimitiveUP(D3DPT_LINELIST,
            static_cast<UINT>(s_lines2D.size() / 2),
            s_lines2D.data(), sizeof(Vertex2D));
    }

    dev->ApplyStateBlock(s_stateBlock);
    ClearBatchesRHW();
}

//------------------------------------------------------------------------------
// Spatial / Vision Node Visualization (WorldScene debugging)
//------------------------------------------------------------------------------
static int CountNodeRenderables(const SpatialNode& node) {
    return node.mSEntityElems.mUseSize
         + node.mAnimCollElems.mUseSize
         + node.mAnimElems.mUseSize
         + node.mDPhysElems.mUseSize
         + node.mSPhysElems.mUseSize;
}

void AddFencesSpatialTree(const SpatialTree* tree) {
    if (!tree || !tree->mTreeNodes.mpData) return;

    for (int i = 0; i < tree->mTreeNodes.mSize; ++i) {
        const SpatialNode& node = tree->mTreeNodes.mpData[i].mData;
        for (int j = node.mFenceElems.mUseSize; j-- > 0;) {
            if (const FenceEntityDSG* fence = node.mFenceElems.mpData[j]) {
                AddFence(fence, true, false);
            }
        }
    }
}

void AddFence(const FenceEntityDSG* fence, bool active, bool overflow) {
    if (!fence) return;

    uint32_t color = overflow ? Color::Green : active ? Color::Red : Color::White;
    Vec3 start{ fence->mStartPoint.x, 0.f, fence->mStartPoint.z };
    Vec3 end{ fence->mEndPoint.x, 0.f, fence->mEndPoint.z };
    AddLine(start, end, color, false);
}

void AddNodeAxis(const SpatialNode* node, bool isCurrentLeaf, bool intersect) {
    if (!node) return;

    auto axis = static_cast<Axis>(node->mSubDivPlane.mAxis);
    const auto& min = reinterpret_cast<const Vec3&>(node->mBBox.mBounds.mMin);
    const auto& max = reinterpret_cast<const Vec3&>(node->mBBox.mBounds.mMax);
    const float pos = node->mSubDivPlane.mPosn;

    uint32_t color = intersect ? Color::Green : (isCurrentLeaf ? Color::Red : Color::White);
    if (axis == X) {
        AddLine(Vec3{ pos, 0.f, min.z }, Vec3{ pos, 0.f, max.z }, color, false);
    } else if (axis == Z) {
        AddLine(Vec3{ min.x, 0.f, pos }, Vec3{ max.x, 0.f, pos }, color, false);
    }
}

void DrawNode(const SpatialNode* node, bool isCurrentLeaf, bool intersect) {
    if (!node) return;
    AddNodeAxis(node, isCurrentLeaf, intersect);
}

void DrawAllNodes(const WorldScene* ws) {
    if (!ws || !ws->mpStaticTree || !ws->mpStaticTree->mTreeNodes.mpData ||
        !ws->mStaticTreeWalker.mNodeMarks.mpData || !ws->mStaticTreeWalker.mpRootNode) {
        return;
    }

    const auto& binNodes = ws->mpStaticTree->mTreeNodes;
    const int nodeCount = (binNodes.mSize < ws->mStaticTreeWalker.mNodeMarks.mSize)
        ? binNodes.mSize
        : ws->mStaticTreeWalker.mNodeMarks.mSize;

    // A. Camera culling volumes: View Sphere vs. Vision Cone.
    const float drawDist = (ws->mDrawDist > 0.0f) ? ws->mDrawDist : 0.0f;
    const Vec3 camPos{ ws->mViewPosn.x, ws->mViewPosn.y, ws->mViewPosn.z };
    const Vec3 viewDir{ ws->mViewVector.x, ws->mViewVector.y, ws->mViewVector.z };
    const float viewSphereRadius = drawDist * 0.5f;
    const Vec3 viewSphereCenter = camPos + viewDir * viewSphereRadius;

    // Camera view direction ray.
    AddLine(camPos, camPos + viewDir * drawDist, Color::Yellow, false);

    // Vision cone (SetVisCone / IsSphereInCone) projected onto y = 0.
    if (ws->mViewCosSqr > 0.0f && ws->mViewCosSqr <= 1.0f && drawDist > 0.0f) {
        const float xzLenSq = viewDir.x * viewDir.x + viewDir.z * viewDir.z;
        if (xzLenSq > 1e-8f) {
            const float invXzLen = 1.0f / std::sqrt(xzLenSq);
            const float vx = viewDir.x * invXzLen;
            const float vz = viewDir.z * invXzLen;

            const float cosA = std::sqrt(ws->mViewCosSqr);
            const float sinSq = 1.0f - ws->mViewCosSqr;
            const float sinA = std::sqrt(sinSq > 0.0f ? sinSq : 0.0f);

            const Vec3 originXZ{ camPos.x, 0.f, camPos.z };
            const Vec3 leftDir{ vx * cosA - vz * sinA, 0.f, vz * cosA + vx * sinA };
            const Vec3 rightDir{ vx * cosA + vz * sinA, 0.f, vz * cosA - vx * sinA };
            const Vec3 leftEnd = originXZ + leftDir * drawDist;
            const Vec3 rightEnd = originXZ + rightDir * drawDist;

            AddLine(originXZ, leftEnd, Color::Orange, false);
            AddLine(originXZ, rightEnd, Color::Orange, false);
            AddLine(leftEnd, rightEnd, Color::Orange, false);
        }

        // SMALL_THING_CULL_DIST_SQR = 4000.0f cutoff ring for small objects (radius < 0.85f).
        AddCircle(Vec3{ camPos.x, 0.f, camPos.z }, std::sqrt(4000.0f), Color::Orange, 16, false);
    }

    // B. Classify and draw all spatial tree nodes in pre-order.
    int occludedSubtreeEnd = -1;
    Vector3f sphereCenter(viewSphereCenter.x, viewSphereCenter.y, viewSphereCenter.z);

    for (int i = 0; i < nodeCount; ++i) {
        auto& binNode = binNodes.mpData[i];
        const tMark mark = ws->mStaticTreeWalker.mNodeMarks.mpData[i];
        const SpatialNode& node = binNode.mData;

        const bool isOccludedRoot = (mark & SpatialTreeIter::msFilterInvisible) != 0;
        if (isOccludedRoot) {
            int subEnd = i + binNode.GetSubTreeSize();
            if (subEnd >= nodeCount) subEnd = nodeCount - 1;
            if (subEnd > occludedSubtreeEnd) occludedSubtreeEnd = subEnd;
        }
        const bool isInOccludedSubtree = (i <= occludedSubtreeEnd);
        const bool isAlwaysVis = (mark & SpatialTreeIter::msFilterVisible) != 0;
        const bool isCamMarked = (mark & WorldScene::msVisible0) != 0;

        Bounds3f bounds = node.mBBox.mBounds;
        const bool intersectsViewSphere = bounds.IntersectsSphere(sphereCenter, viewSphereRadius);
        const int numRenderables = CountNodeRenderables(node);

        uint32_t color = Color::DarkGrey;
        if (isOccludedRoot)
            color = Color::Magenta;
        else if (isInOccludedSubtree)
            color = Color::Red;
        else if (isAlwaysVis)
            color = Color::Cyan;
        else if (isCamMarked && !intersectsViewSphere)
            color = Color::Orange;
        else if (isCamMarked && numRenderables > 0)
            color = Color::Green;
        else if (isCamMarked)
            color = Color::White;
        else if (numRenderables > 0)
            color = Color::LightGrey;

        const float pos = node.mSubDivPlane.mPosn;
        const Axis axis = static_cast<Axis>(node.mSubDivPlane.mAxis);
        const auto& min = reinterpret_cast<const Vec3&>(node.mBBox.mBounds.mMin);
        const auto& max = reinterpret_cast<const Vec3&>(node.mBBox.mBounds.mMax);

        if (axis == X) {
            AddLine(Vec3{ pos, 0.f, min.z }, Vec3{ pos, 0.f, max.z }, color, false);
        } else if (axis == Z) {
            AddLine(Vec3{ min.x, 0.f, pos }, Vec3{ max.x, 0.f, pos }, color, false);
        } else if (axis == LEAF) {
            Vec3 mid = (min + max) * 0.5f;
            mid.y = 0.f;
            AddPlus(mid, 0.f, 2.f, color, false);

            if ((isCamMarked || isAlwaysVis) && numRenderables > 0) {
                const float inset = 0.5f;
                if (max.x - min.x > inset * 2.f && max.z - min.z > inset * 2.f) {
                    AddRectXZ(min.x + inset, min.z + inset, max.x - inset, max.z - inset, 0.f, color, false);
                }
            }
            if (isInOccludedSubtree) {
                AddLine(Vec3{ mid.x - 1.5f, 0.f, mid.z - 1.5f }, Vec3{ mid.x + 1.5f, 0.f, mid.z + 1.5f }, Color::Red, false);
                AddLine(Vec3{ mid.x - 1.5f, 0.f, mid.z + 1.5f }, Vec3{ mid.x + 1.5f, 0.f, mid.z - 1.5f }, Color::Red, false);
            }
        }

        // Highlight Root Node (Node 0) if overflow elements fell back into it.
        if (i == 0 && numRenderables > 0) {
            AddRectXZ(min.x, min.z, max.x, max.z, 0.f, Color::Purple, false);
        }
    }

    // C. Highlight Player 0 position and the spatial leaf containing the player.
    if (GetAvatarManager()) {
        if (Avatar* av = GetAvatarManager()->GetAvatarForPlayer(0)) {
            if (av->IsInCar() ? (av->GetVehicle() != nullptr) : (av->GetCharacter() != nullptr)) {
                rmt::Vector p;
                av->GetPosition(p);
                Vec3 playerGround{ p.x, 0.f, p.z };
                AddPlus(playerGround, 0.f, 2.5f, Color::Magenta, false);
                AddCircle(playerGround, 3.0f, Color::Magenta, 16, false);

                Vector3f seekPt(p.x, p.y, p.z);
                const SpatialNode& playerLeaf = const_cast<WorldScene*>(ws)->mStaticTreeWalker.rSeekLeaf(seekPt);
                const auto& lmin = reinterpret_cast<const Vec3&>(playerLeaf.mBBox.mBounds.mMin);
                const auto& lmax = reinterpret_cast<const Vec3&>(playerLeaf.mBBox.mBounds.mMax);
                const float inset = 0.75f;
                if (lmax.x - lmin.x > inset * 2.f && lmax.z - lmin.z > inset * 2.f) {
                    AddRectXZ(lmin.x + inset, lmin.z + inset, lmax.x - inset, lmax.z - inset, 0.f, Color::Magenta, false);
                } else {
                    AddRectXZ(lmin.x, lmin.z, lmax.x, lmax.z, 0.f, Color::Magenta, false);
                }
            }
        }
    }
}

}
