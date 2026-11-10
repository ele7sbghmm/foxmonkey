#include "draw/fences/fences.h"

#pragma warning(push)
#pragma warning(disable : 4005)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d8.h>
#pragma warning(pop)

#include <stdlib.h>
#include <string.h>

#include <p3d/utility.hpp>
#include <pddi/dx8/display.hpp>
#include <render/Culling/WorldScene.h>
#include <render/DSG/FenceEntityDSG.h>

#include "server/server.h"

namespace srr2
{
namespace draw
{
namespace fences
{

namespace
{

struct FenceVertex
{
    float x, y, z;
    D3DCOLOR color;
};

static const DWORD kFenceFVF = (D3DFVF_XYZ | D3DFVF_DIFFUSE);
static const int kMaxBatchedFences = 16380; // 16380 * 4 = 65520 vertices (< 65536 for 16-bit indices)

static IDirect3DDevice8* g_Device = NULL;
static IDirect3DVertexBuffer8* g_VertexBuffer = NULL;
static IDirect3DIndexBuffer8* g_IndexBuffer = NULL;
static int g_FenceCount = 0;
static bool g_Enabled = false;

static IDirect3DDevice8* AcquireDevice()
{
    if (g_Device != NULL)
    {
        return g_Device;
    }
    if (p3d::display != NULL)
    {
        g_Device = static_cast<d3dDisplay*>(p3d::display)->GetD3DDevice();
    }
    return g_Device;
}

static void ReleaseBuffers()
{
    if (g_VertexBuffer != NULL)
    {
        g_VertexBuffer->Release();
        g_VertexBuffer = NULL;
    }
    if (g_IndexBuffer != NULL)
    {
        g_IndexBuffer->Release();
        g_IndexBuffer = NULL;
    }
    g_FenceCount = 0;
}

} // anonymous namespace

void Init(IDirect3DDevice8* device)
{
    g_Device = device;
}

void Shutdown()
{
    ReleaseBuffers();
    g_Device = NULL;
}

void OnPreReset()
{
    // Managed buffers survive D3D8 Reset, no action required.
}

void OnPostReset()
{
}

void Clear()
{
    ReleaseBuffers();
    srr2::UpdateFences(NULL, 0);
}

void Rebuild(WorldScene* worldScene)
{
    ReleaseBuffers();

    if (worldScene == NULL || !worldScene->mStaticTreeWalker.IsSetUp())
    {
        srr2::UpdateFences(NULL, 0);
        return;
    }

    int numNodes = worldScene->mStaticTreeWalker.NumNodes();
    int totalFences = 0;
    for (int i = 0; i < numNodes; ++i)
    {
        SpatialNode& rNode = worldScene->mStaticTreeWalker.rIthNode(i);
        for (int j = 0; j < rNode.mFenceElems.mUseSize; ++j)
        {
            if (rNode.mFenceElems[j] != NULL)
            {
                ++totalFences;
            }
        }
    }

    if (totalFences <= 0)
    {
        srr2::UpdateFences(NULL, 0);
        return;
    }

    // 1. Broadcast all loaded fence 2D XZ segments to WebSocket clients
    float* segmentsXZ = static_cast<float*>(malloc(static_cast<size_t>(totalFences) * 4 * sizeof(float)));
    if (segmentsXZ != NULL)
    {
        int segIdx = 0;
        for (int i = 0; i < numNodes; ++i)
        {
            SpatialNode& rNode = worldScene->mStaticTreeWalker.rIthNode(i);
            for (int j = 0; j < rNode.mFenceElems.mUseSize; ++j)
            {
                FenceEntityDSG* pFence = rNode.mFenceElems[j];
                if (pFence != NULL)
                {
                    segmentsXZ[segIdx++] = pFence->mStartPoint.x;
                    segmentsXZ[segIdx++] = pFence->mStartPoint.z;
                    segmentsXZ[segIdx++] = pFence->mEndPoint.x;
                    segmentsXZ[segIdx++] = pFence->mEndPoint.z;
                }
            }
        }
        srr2::UpdateFences(segmentsXZ, static_cast<size_t>(segIdx / 4));
        free(segmentsXZ);
    }

    // 2. Build batched Direct3D 8 Vertex Buffer and Index Buffer for in-game rendering
    IDirect3DDevice8* device = AcquireDevice();
    if (device == NULL)
    {
        return;
    }

    int batchedFences = (totalFences > kMaxBatchedFences) ? kMaxBatchedFences : totalFences;
    UINT vertexCount = static_cast<UINT>(batchedFences * 4);
    UINT indexCount = static_cast<UINT>(batchedFences * 6);
    UINT vbByteSize = vertexCount * sizeof(FenceVertex);
    UINT ibByteSize = indexCount * sizeof(WORD);

    if (FAILED(device->CreateVertexBuffer(
            vbByteSize,
            0,
            kFenceFVF,
            D3DPOOL_MANAGED,
            &g_VertexBuffer)))
    {
        ReleaseBuffers();
        return;
    }

    if (FAILED(device->CreateIndexBuffer(
            ibByteSize,
            0,
            D3DFMT_INDEX16,
            D3DPOOL_MANAGED,
            &g_IndexBuffer)))
    {
        ReleaseBuffers();
        return;
    }

    BYTE* vbBytes = NULL;
    if (FAILED(g_VertexBuffer->Lock(0, vbByteSize, &vbBytes, 0)) || vbBytes == NULL)
    {
        ReleaseBuffers();
        return;
    }

    BYTE* ibBytes = NULL;
    if (FAILED(g_IndexBuffer->Lock(0, ibByteSize, &ibBytes, 0)) || ibBytes == NULL)
    {
        g_VertexBuffer->Unlock();
        ReleaseBuffers();
        return;
    }

    FenceVertex* vertices = reinterpret_cast<FenceVertex*>(vbBytes);
    WORD* indices = reinterpret_cast<WORD*>(ibBytes);

    const D3DCOLOR solidRed = D3DCOLOR_ARGB(255, 255, 0, 0);
    int fenceIdx = 0;

    for (int i = 0; i < numNodes && fenceIdx < batchedFences; ++i)
    {
        SpatialNode& rNode = worldScene->mStaticTreeWalker.rIthNode(i);
        for (int j = 0; j < rNode.mFenceElems.mUseSize && fenceIdx < batchedFences; ++j)
        {
            FenceEntityDSG* pFence = rNode.mFenceElems[j];
            if (pFence == NULL)
            {
                continue;
            }

            const float x0 = pFence->mStartPoint.x;
            const float y0 = pFence->mStartPoint.y;
            const float z0 = pFence->mStartPoint.z;
            const float x1 = pFence->mEndPoint.x;
            const float y1 = pFence->mEndPoint.y;
            const float z1 = pFence->mEndPoint.z;

            WORD baseVertex = static_cast<WORD>(fenceIdx * 4);
            int vOffset = fenceIdx * 4;
            int iOffset = fenceIdx * 6;

            vertices[vOffset + 0].x = x0;
            vertices[vOffset + 0].y = y0;
            vertices[vOffset + 0].z = z0;
            vertices[vOffset + 0].color = solidRed;

            vertices[vOffset + 1].x = x1;
            vertices[vOffset + 1].y = y0;
            vertices[vOffset + 1].z = z1;
            vertices[vOffset + 1].color = solidRed;

            vertices[vOffset + 2].x = x1;
            vertices[vOffset + 2].y = y1;
            vertices[vOffset + 2].z = z1;
            vertices[vOffset + 2].color = solidRed;

            vertices[vOffset + 3].x = x0;
            vertices[vOffset + 3].y = y1;
            vertices[vOffset + 3].z = z0;
            vertices[vOffset + 3].color = solidRed;

            indices[iOffset + 0] = baseVertex + 0;
            indices[iOffset + 1] = baseVertex + 1;
            indices[iOffset + 2] = baseVertex + 2;
            indices[iOffset + 3] = baseVertex + 0;
            indices[iOffset + 4] = baseVertex + 2;
            indices[iOffset + 5] = baseVertex + 3;

            ++fenceIdx;
        }
    }

    g_IndexBuffer->Unlock();
    g_VertexBuffer->Unlock();
    g_FenceCount = fenceIdx;
}

void Render()
{
    if (!g_Enabled || g_FenceCount <= 0 || g_VertexBuffer == NULL || g_IndexBuffer == NULL)
    {
        return;
    }

    IDirect3DDevice8* device = AcquireDevice();
    if (device == NULL)
    {
        return;
    }

    DWORD stateBlock = 0;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock)))
    {
        return;
    }

    // Set camera world-to-view transform on D3DTS_WORLD and identity on D3DTS_VIEW
    D3DMATRIX identityMat;
    memset(&identityMat, 0, sizeof(identityMat));
    identityMat._11 = 1.0f;
    identityMat._22 = 1.0f;
    identityMat._33 = 1.0f;
    identityMat._44 = 1.0f;
    device->SetTransform(D3DTS_VIEW, &identityMat);

    if (p3d::context != NULL)
    {
        const rmt::Matrix* viewMat = p3d::context->GetViewMatrix();
        if (viewMat != NULL)
        {
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(viewMat));
        }
    }

    device->SetPixelShader(0);
    device->SetVertexShader(kFenceFVF);

    device->SetTexture(0, NULL);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

    device->SetStreamSource(0, g_VertexBuffer, sizeof(FenceVertex));
    device->SetIndices(g_IndexBuffer, 0);

    // Single batched indexed draw call for all loaded fences
    device->DrawIndexedPrimitive(
        D3DPT_TRIANGLELIST,
        0,
        static_cast<UINT>(g_FenceCount * 4),
        0,
        static_cast<UINT>(g_FenceCount * 2));

    device->ApplyStateBlock(stateBlock);
    device->DeleteStateBlock(stateBlock);
}

bool IsEnabled()
{
    return g_Enabled;
}

void SetEnabled(bool enabled)
{
    g_Enabled = enabled;
}

bool* GetEnabledPtr()
{
    return &g_Enabled;
}

int GetFenceCount()
{
    return g_FenceCount;
}

} // namespace fences
} // namespace draw
} // namespace srr2
