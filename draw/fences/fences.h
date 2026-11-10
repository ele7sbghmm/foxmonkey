#pragma once

struct IDirect3DDevice8;
class WorldScene;

namespace srr2
{
namespace draw
{
namespace fences
{

void Init(IDirect3DDevice8* device);
void Shutdown();

void OnPreReset();
void OnPostReset();

// Rebuilds the batched D3D8 vertex/index buffers from all currently loaded
// FenceEntityDSGs in the WorldScene and broadcasts the updated fence list to WebSocket clients.
void Rebuild(WorldScene* worldScene);
void Clear();

// Draws all batched fences in a single D3D8 DrawIndexedPrimitive call.
void Render();

bool IsEnabled();
void SetEnabled(bool enabled);
bool* GetEnabledPtr();
int GetFenceCount();

} // namespace fences
} // namespace draw
} // namespace srr2
