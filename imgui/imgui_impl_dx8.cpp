// dear imgui: Renderer Backend for DirectX8
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'LPDIRECT3DTEXTURE8' as texture identifier. Read the FAQ about ImTextureID/ImTextureRef!
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).
//  [X] Renderer: IMGUI_USE_BGRA_PACKED_COLOR support, as this is the optimal color encoding for DirectX8.

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_dx8.h"

// DirectX 8
#if defined(__has_include) && __has_include(<d3d8.h>)
#include <d3d8.h>
#else
#include <sdks/dx8/d3d8.h>
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"         // warning: use of old-style cast                            // yes, they are more terse.
#pragma clang diagnostic ignored "-Wsign-conversion"        // warning: implicit conversion changes signedness
#endif

// DirectX data
struct ImGui_ImplDX8_Data
{
    LPDIRECT3DDEVICE8           pd3dDevice;
    LPDIRECT3DVERTEXBUFFER8     pVB;
    LPDIRECT3DINDEXBUFFER8      pIB;
    int                         VertexBufferSize;
    int                         IndexBufferSize;

    ImGui_ImplDX8_Data()        { memset((void*)this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

struct CUSTOMVERTEX
{
    float    pos[3];
    D3DCOLOR col;
    float    uv[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_DX8_ARGB(_COL)     (_COL)
#else
#define IMGUI_COL_TO_DX8_ARGB(_COL)     (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplDX8_Data* ImGui_ImplDX8_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX8_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static void ImGui_ImplDX8_SetClipViewportAndProjection(LPDIRECT3DDEVICE8 device, ImDrawData* draw_data, LONG left, LONG top, LONG right, LONG bottom)
{
    D3DVIEWPORT8 vp;
    vp.X = (DWORD)left;
    vp.Y = (DWORD)top;
    vp.Width = (DWORD)(right - left);
    vp.Height = (DWORD)(bottom - top);
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    device->SetViewport(&vp);

    float L = draw_data->DisplayPos.x + (float)left + 0.5f;
    float R = draw_data->DisplayPos.x + (float)right + 0.5f;
    float T = draw_data->DisplayPos.y + (float)top + 0.5f;
    float B = draw_data->DisplayPos.y + (float)bottom + 0.5f;
    D3DMATRIX mat_projection =
    { { {
        2.0f/(R-L),   0.0f,         0.0f,  0.0f,
        0.0f,         2.0f/(T-B),   0.0f,  0.0f,
        0.0f,         0.0f,         0.5f,  0.0f,
        (L+R)/(L-R),  (T+B)/(B-T),  0.5f,  1.0f
    } } };
    device->SetTransform(D3DTS_PROJECTION, &mat_projection);
}

// Functions
static void ImGui_ImplDX8_SetupRenderState(ImDrawData* draw_data)
{
    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();
    LPDIRECT3DDEVICE8 device = bd->pd3dDevice;

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing, shade mode (for gradient), bilinear sampling.
    device->SetPixelShader(0);
    device->SetVertexShader(D3DFVF_CUSTOMVERTEX);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPING, TRUE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    // Setup world/view identity and initial full-display viewport + orthographic projection
    {
        D3DMATRIX mat_identity = { { { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f } } };
        device->SetTransform(D3DTS_WORLD, &mat_identity);
        device->SetTransform(D3DTS_VIEW, &mat_identity);
        ImGui_ImplDX8_SetClipViewportAndProjection(device, draw_data, 0, 0, (LONG)draw_data->DisplaySize.x, (LONG)draw_data->DisplaySize.y);
    }
}

// Draw callbacks
static void ImGui_ImplDX8_DrawCallback_ResetRenderState(const ImDrawList*, const ImDrawCmd*)    {} // Intentionally empty. Used as an identifier for rendering loop to call its code. Simpler to implement this way.
static void ImGui_ImplDX8_DrawCallback_SetSamplerLinear(const ImDrawList*, const ImDrawCmd*)    { ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData(); bd->pd3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR); bd->pd3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR); }
static void ImGui_ImplDX8_DrawCallback_SetSamplerNearest(const ImDrawList*, const ImDrawCmd*)   { ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData(); bd->pd3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT); bd->pd3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT); }

// Render function.
void ImGui_ImplDX8_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();
    LPDIRECT3DDEVICE8 device = bd->pd3dDevice;

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplDX8_UpdateTexture(tex);

    // Create and grow buffers if needed
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        if (device->CreateVertexBuffer((UINT)bd->VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &bd->pVB) < 0)
            return;
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        if (device->CreateIndexBuffer((UINT)bd->IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &bd->pIB) < 0)
            return;
    }

    // Backup the DX8 state
    DWORD state_block = 0;
    if (device->CreateStateBlock(D3DSBT_ALL, &state_block) < 0)
        return;
    if (device->CaptureStateBlock(state_block) < 0)
    {
        device->DeleteStateBlock(state_block);
        return;
    }

    // Backup the DX8 transform and viewport
    D3DMATRIX last_world, last_view, last_projection;
    D3DVIEWPORT8 last_viewport;
    device->GetTransform(D3DTS_WORLD, &last_world);
    device->GetTransform(D3DTS_VIEW, &last_view);
    device->GetTransform(D3DTS_PROJECTION, &last_projection);
    device->GetViewport(&last_viewport);

    // Allocate buffers
    CUSTOMVERTEX* vtx_dst;
    ImDrawIdx* idx_dst;
    if (bd->pVB->Lock(0, (UINT)(draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX)), (BYTE**)&vtx_dst, D3DLOCK_DISCARD) < 0)
    {
        device->DeleteStateBlock(state_block);
        return;
    }
    if (bd->pIB->Lock(0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (BYTE**)&idx_dst, D3DLOCK_DISCARD) < 0)
    {
        bd->pVB->Unlock();
        device->DeleteStateBlock(state_block);
        return;
    }

    // Copy and convert all vertices into a single contiguous buffer, convert colors to DX8 default format.
    // FIXME-OPT: This is a minor waste of resource, the ideal is to use imconfig.h and
    //  1) to avoid repacking colors:   #define IMGUI_USE_BGRA_PACKED_COLOR
    //  2) to avoid repacking vertices: #define IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT struct ImDrawVert { ImVec2 pos; float z; ImU32 col; ImVec2 uv; }
    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        const ImDrawVert* vtx_src = draw_list->VtxBuffer.Data;
        for (int i = 0; i < draw_list->VtxBuffer.Size; i++)
        {
            vtx_dst->pos[0] = vtx_src->pos.x;
            vtx_dst->pos[1] = vtx_src->pos.y;
            vtx_dst->pos[2] = 0.0f;
            vtx_dst->col = IMGUI_COL_TO_DX8_ARGB(vtx_src->col);
            vtx_dst->uv[0] = vtx_src->uv.x;
            vtx_dst->uv[1] = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }
        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += draw_list->IdxBuffer.Size;
    }
    bd->pVB->Unlock();
    bd->pIB->Unlock();
    device->SetStreamSource(0, bd->pVB, sizeof(CUSTOMVERTEX));
    device->SetIndices(bd->pIB, 0);

    // Setup desired DX state
    ImGui_ImplDX8_SetupRenderState(draw_data);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    float fb_width = draw_data->DisplaySize.x;
    float fb_height = draw_data->DisplaySize.y;
    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                if (pcmd->UserCallback == ImGui_ImplDX8_DrawCallback_ResetRenderState)
                    ImGui_ImplDX8_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                LONG r_left   = (LONG)(clip_min.x < 0.0f ? 0.0f : clip_min.x);
                LONG r_top    = (LONG)(clip_min.y < 0.0f ? 0.0f : clip_min.y);
                LONG r_right  = (LONG)(clip_max.x > fb_width ? fb_width : clip_max.x);
                LONG r_bottom = (LONG)(clip_max.y > fb_height ? fb_height : clip_max.y);
                if (r_right <= r_left || r_bottom <= r_top)
                    continue;

                // Apply clipping rectangle via D3D8 viewport + matching projection
                ImGui_ImplDX8_SetClipViewportAndProjection(device, draw_data, r_left, r_top, r_right, r_bottom);

                // Bind texture, Draw
                const LPDIRECT3DTEXTURE8 texture = (LPDIRECT3DTEXTURE8)pcmd->GetTexID();
                device->SetTexture(0, texture);
                device->SetIndices(bd->pIB, pcmd->VtxOffset + global_vtx_offset);
                device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, (UINT)draw_list->VtxBuffer.Size, pcmd->IdxOffset + global_idx_offset, pcmd->ElemCount / 3);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }

    // Restore the DX8 transform and viewport
    device->SetTransform(D3DTS_WORLD, &last_world);
    device->SetTransform(D3DTS_VIEW, &last_view);
    device->SetTransform(D3DTS_PROJECTION, &last_projection);
    device->SetViewport(&last_viewport);

    // Restore the DX8 state
    device->ApplyStateBlock(state_block);
    device->DeleteStateBlock(state_block);
}

// Convert RGBA32 to BGRA32 (D3DFMT_A8R8G8B8 in DX8)
static void ImGui_ImplDX8_CopyTextureRegion(bool tex_use_colors, const ImU32* src, int src_pitch, ImU32* dst, int dst_pitch, int w, int h)
{
#ifndef IMGUI_USE_BGRA_PACKED_COLOR
    const bool convert_rgba_to_bgra = tex_use_colors;
#else
    const bool convert_rgba_to_bgra = false;
    IM_UNUSED(tex_use_colors);
#endif
    for (int y = 0; y < h; y++)
    {
        const ImU32* src_p = (const ImU32*)(const void*)((const unsigned char*)src + src_pitch * y);
        ImU32* dst_p = (ImU32*)(void*)((unsigned char*)dst + dst_pitch * y);
        if (convert_rgba_to_bgra)
            for (int x = w; x > 0; x--, src_p++, dst_p++) // Convert copy
                *dst_p = IMGUI_COL_TO_DX8_ARGB(*src_p);
        else
            memcpy(dst_p, src_p, w * 4); // Raw copy
    }
}

void ImGui_ImplDX8_UpdateTexture(ImTextureData* tex)
{
    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        LPDIRECT3DTEXTURE8 dx_tex = nullptr;
        HRESULT hr = bd->pd3dDevice->CreateTexture(tex->Width, tex->Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dx_tex);
        if (hr < 0)
            hr = bd->pd3dDevice->CreateTexture(tex->Width, tex->Height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dx_tex);
        if (hr < 0)
        {
            IM_ASSERT(hr >= 0 && "Backend failed to create texture!");
            return;
        }

        D3DLOCKED_RECT locked_rect;
        if (dx_tex->LockRect(0, &locked_rect, nullptr, 0) == D3D_OK)
        {
            ImGui_ImplDX8_CopyTextureRegion(tex->UseColors, (ImU32*)tex->GetPixels(), tex->Width * 4, (ImU32*)locked_rect.pBits, (ImU32)locked_rect.Pitch, tex->Width, tex->Height);
            dx_tex->UnlockRect(0);
        }

        // Store identifiers
        tex->SetTexID((ImTextureID)(intptr_t)dx_tex);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->Updates[] but you can use tex->UpdateRect to upload a single region.
        LPDIRECT3DTEXTURE8 backend_tex = (LPDIRECT3DTEXTURE8)(intptr_t)tex->TexID;
        RECT update_rect = { (LONG)tex->UpdateRect.x, (LONG)tex->UpdateRect.y, (LONG)(tex->UpdateRect.x + tex->UpdateRect.w), (LONG)(tex->UpdateRect.y + tex->UpdateRect.h) };
        D3DLOCKED_RECT locked_rect;
        if (backend_tex->LockRect(0, &locked_rect, &update_rect, 0) == D3D_OK)
            for (ImTextureRect& r : tex->Updates)
                ImGui_ImplDX8_CopyTextureRegion(tex->UseColors, (ImU32*)tex->GetPixelsAt(r.x, r.y), tex->Width * 4,
                    (ImU32*)locked_rect.pBits + (r.x - update_rect.left) + (r.y - update_rect.top) * (locked_rect.Pitch / 4), (int)locked_rect.Pitch, r.w, r.h);
        backend_tex->UnlockRect(0);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        if (tex->TexID != ImTextureID_Invalid)
            if (LPDIRECT3DTEXTURE8 backend_tex = (LPDIRECT3DTEXTURE8)tex->TexID)
            {
                IM_ASSERT(tex->TexID == (ImTextureID)(intptr_t)backend_tex);
                backend_tex->Release();

                // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
                tex->SetTexID(ImTextureID_Invalid);
            }
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

bool ImGui_ImplDX8_CreateDeviceObjects()
{
    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return false;
    return true;
}

void ImGui_ImplDX8_InvalidateDeviceObjects()
{
    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return;

    // Destroy all textures
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplDX8_UpdateTexture(tex);
        }
    if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }
}

void ImGui_ImplDX8_NewFrame()
{
    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplDX8_Init()?");
    IM_UNUSED(bd);
}

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplDX8_Data* bd = IM_NEW(ImGui_ImplDX8_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx8";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = 4096;
    platform_io.DrawCallback_ResetRenderState = ImGui_ImplDX8_DrawCallback_ResetRenderState;
    platform_io.DrawCallback_SetSamplerLinear = ImGui_ImplDX8_DrawCallback_SetSamplerLinear;
    platform_io.DrawCallback_SetSamplerNearest = ImGui_ImplDX8_DrawCallback_SetSamplerNearest;

    bd->pd3dDevice = device;
    bd->pd3dDevice->AddRef();

    return true;
}

void ImGui_ImplDX8_Shutdown()
{
    ImGui_ImplDX8_Data* bd = ImGui_ImplDX8_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGui_ImplDX8_InvalidateDeviceObjects();
    if (bd->pd3dDevice) { bd->pd3dDevice->Release(); }

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    platform_io.ClearRendererHandlers();
    IM_DELETE(bd);
}

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
