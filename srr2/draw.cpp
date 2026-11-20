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
    s_stateBlockDevice = device;
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

//------------------------------------------------------------------------------
// Platform / thread setup
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// 3D Primitives (all decompose into AddLine)
//------------------------------------------------------------------------------
void AddLine(const rmt::Vector& a, const rmt::Vector& b, uint32_t color, bool depthTest) {
    AddLine(reinterpret_cast<const Vec3&>(a), reinterpret_cast<const Vec3&>(b), color, depthTest);
}
void AddLine(const Vec3& a, const Vec3& b, uint32_t color, bool depthTest) {
    auto& batch = depthTest ? s_linesDepth : s_linesOverlay;
    auto col = static_cast<DWORD>(color);
    batch.push_back({{ a.x, a.y, a.z }, col });
    batch.push_back({{ b.x, b.y, b.z }, col });
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
    AddLine(center - Vec3{ size, 0.f, 0.f }, center + Vec3{ size, 0.f, 0.f }, color, depthTest);
    AddLine(center - Vec3{ 0.f, 0.f, size }, center + Vec3{ 0.f, 0.f, size }, color, depthTest);
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
    if (segments < 3) {
        segments = 3;
    }
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
    if (segments < 3) {
        segments = 3;
    }
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
static bool ValidateStateBlock() {
    auto* dev = s_device;
    if (s_stateBlockDevice != dev) {
        if (s_stateBlock && s_stateBlockDevice) {
            s_stateBlockDevice->DeleteStateBlock(s_stateBlock);
        }
        s_stateBlock = 0;
        s_stateBlockDevice = dev;
    }
    if (!s_stateBlock) {
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &s_stateBlock))) {
            ClearBatches();
            ClearBatchesRHW();
            return false;
        }
    }
    else if (FAILED(dev->CaptureStateBlock(s_stateBlock))) {
        dev->DeleteStateBlock(s_stateBlock);
        s_stateBlock = 0;
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &s_stateBlock))) {
            ClearBatches();
            ClearBatchesRHW();
            return false;
        }
    }
    return true;
}
void Flush() {
    if (s_linesDepth.empty() && s_linesOverlay.empty()) return;

    SendVertices(s_linesOverlay);

    IDirect3DDevice8* dev = s_device;
    if (!dev || !p3d::context) {
        ClearBatches();
        return;
    }

    // Create or recreate the state block if the device changed or was reset.
    if (!ValidateStateBlock()) return;

    // Configure clean fixed-function vertex-color rendering without modifying
    // Pure3D's shadowed d3dState (ApplyStateBlock below restores everything).
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

    // 3D batches: Pure3D leaves D3DTS_VIEW as Identity and multiplies World*View
    // into D3DTS_WORLD, while keeping the active perspective D3DTS_PROJECTION.
    if (!s_linesDepth.empty() || !s_linesOverlay.empty()) {
        //D3DMATRIX world = IdentityMatrix();
        //dev->SetTransform(D3DTS_WORLD, &world);
        //dev->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(p3d::context->GetViewMatrix()));
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
    }

    dev->ApplyStateBlock(s_stateBlock);
    ClearBatches();
}
void FlushRHW() {
    if (s_lines2D.empty() && s_tris2D.empty()) return;

    IDirect3DDevice8* dev = s_device;
    if (!dev || !p3d::context) {
        ClearBatchesRHW();
        return;
    }

    // Create or recreate the state block if the device changed or was reset.
    if (!ValidateStateBlock()) return;

    // Configure clean fixed-function vertex-color rendering without modifying
    // Pure3D's shadowed d3dState (ApplyStateBlock below restores everything).
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

    // 2D screen-space batches: XYZRHW ignores World/View/Projection matrices.
    if (!s_tris2D.empty() || !s_lines2D.empty()) {
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
    }

    dev->ApplyStateBlock(s_stateBlock);
    ClearBatchesRHW();
}
void AddFencesSpatialTree(const SpatialTree* tree) {
    if (!tree) return;

    for (int i = 0; i < tree->mTreeNodes.mSize; ++i) {
        const SpatialNode& node = tree->mTreeNodes.mpData[i].mData;
        for (int i = node.mFenceElems.mUseSize; i-- > 0;)
            if (const FenceEntityDSG* fence = node.mFenceElems.mpData[i]; fence)
                AddFence(fence, true, false);
    }
}
void AddFence(const FenceEntityDSG* fence, bool active, bool overflow) {
    if (!fence) return;
    
    uint32_t color = overflow ? Color::Green : active ? Color::Red : Color::White;
    rmt::Vector start = fence->mStartPoint;
    rmt::Vector end   = fence->mEndPoint;
    start.y = 0.f;
    end.y = 0.f;
    AddLine(start, end, color, false);
}
void AddNodeAxis(const SpatialNode* node, bool isCurrentLeaf, bool intersect) {
    auto axis = static_cast<Axis>(node->mSubDivPlane.mAxis);
    auto& min = reinterpret_cast<const D3DXVECTOR3&>(node->mBBox.mBounds.mMin);
    auto& max = reinterpret_cast<const D3DXVECTOR3&>(node->mBBox.mBounds.mMax);
    float pos = node->mSubDivPlane.mPosn;

    if (false) {
        if (isCurrentLeaf) {
            D3DXVECTOR3 start = { min.x, 0.f, min.z };
            D3DXVECTOR3 end = { max.x, 0.f, max.z };
            AddLine(start, end, Color::Green, false);
        }

        switch (axis) {
        case LEAF: {
            D3DXVECTOR3 start = { min.x, 0.f, max.z };
            D3DXVECTOR3 end = { max.x, 0.f, min.z };
            AddLine(start, end, Color::Magenta, false);
            break;
        }
        case X: {
            D3DXVECTOR3 start = { pos, 0.f, min.z };
            D3DXVECTOR3 end = { pos, 0.f, max.z };
            AddLine(start, end, Color::White, false);
            break;
        }
        case Z: {
            D3DXVECTOR3 start = { min.x, 0.f, pos };
            D3DXVECTOR3 end = { max.x, 0.f, pos };
            AddLine(start, end, Color::White, false);
            break;
        }
        default: break;
        }
    }
    uint32_t color = intersect ? Color::Green : Color::White;
    D3DXVECTOR3 start{}, end{};
    if (axis == X) {
        start = { pos, 0.f, min.z };
        end = { pos, 0.f, max.z };
        AddLine(start, end, color, false);
    }
    else if (axis == Z) {
        start = { min.x, 0.f, pos };
        end = { max.x, 0.f, pos };
        AddLine(start, end, color, false);
    }
}
void DrawNode(const SpatialNode* node, bool isCurrentLeaf, bool intersect) {
    auto& min = reinterpret_cast<const Vec3&>(node->mBBox.mBounds.mMin);
    auto& max = reinterpret_cast<const Vec3&>(node->mBBox.mBounds.mMax);
    uint32_t color = Color::White;
    if (isCurrentLeaf)
        color = Color::Red;
    //AddAABB(min, max, color, false);
    AddNodeAxis(node, isCurrentLeaf, intersect);
}
void DrawAllNodes(const WorldScene* ws) {
    if (!ws) return;
    const auto& binNodes = ws->mpStaticTree->mTreeNodes;
    
    for (int i = 0; i < binNodes.mSize; ++i) {
        if (i >= binNodes.mSize) return;

        const tMark mark = ws->mStaticTreeWalker.mNodeMarks.mpData[i];
        const SpatialNode& node = binNodes.mpData[i].mData;
        const float pos = node.mSubDivPlane.mPosn;
        const Axis axis = static_cast<Axis>(node.mSubDivPlane.mAxis);
        const Vec3& min = reinterpret_cast<const Vec3&>(node.mBBox.mBounds.mMin);
        const Vec3& max = reinterpret_cast<const Vec3&>(node.mBBox.mBounds.mMax);
        uint32_t color = Color::Grey;
        if (mark & 1)
            color = Color::White;

        if (axis == X)
            AddLine(Vec3{ pos, 0.f, min.z }, Vec3{ pos, 0.f, max.z }, color, false);
        else if (axis == Z)
            AddLine(Vec3{ min.x, 0.f, pos }, Vec3{ max.x, 0.f, pos }, color, false);
        else if (axis == LEAF) {
            D3DXVECTOR3 mid = (min + max) / 2.f;
            mid.y = 0.f;
            AddPlus(mid, 0.f, 2.f, Color::White, false);
        }
    }
}

}
