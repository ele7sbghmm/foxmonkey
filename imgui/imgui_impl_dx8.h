// dear imgui: Renderer Backend for DirectX8
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'LPDIRECT3DTEXTURE8' as texture identifier. Read the FAQ about ImTextureID/ImTextureRef!
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).
//  [X] Renderer: IMGUI_USE_BGRA_PACKED_COLOR support, as this is the optimal color encoding for DirectX8.

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct IDirect3DDevice8;

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool     ImGui_ImplDX8_Init(IDirect3DDevice8* device);
IMGUI_IMPL_API void     ImGui_ImplDX8_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplDX8_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplDX8_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplDX8_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplDX8_InvalidateDeviceObjects();

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = nullptr to handle this manually.
IMGUI_IMPL_API void     ImGui_ImplDX8_UpdateTexture(ImTextureData* tex);

#endif // #ifndef IMGUI_DISABLE
