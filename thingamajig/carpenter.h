#pragma once

#include "d3dx8.h"
#include "p3d/utility.hpp"
#include "pddi/dx8/display.hpp"

#include <vector>

#include "thingamajig/utility.h"

static constexpr size_t MAX_N = 10000;

struct XyzDiffuse {
	D3DXVECTOR3 xyz;
	D3DCOLOR diffuse;
	static constexpr DWORD FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;
};

struct XyzNormalDiffuse {
	D3DXVECTOR3 xyz;
	D3DXVECTOR3 normal;
	D3DCOLOR diffuse;
	static constexpr DWORD FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE;
};

template <typename T> class Buffer {
public:
	using Vertex = T;

	Vertex* vtx = nullptr;
	size_t n = 0;
	IDirect3DDevice8* device = nullptr;
	IDirect3DVertexBuffer8* vb = nullptr;
	D3DPRIMITIVETYPE PRIMTYPE;
	size_t VTX_PER_PRIM;
	bool init = false;
	bool lock = false;

	Buffer(D3DPRIMITIVETYPE pt, size_t n) : PRIMTYPE(pt), VTX_PER_PRIM(n) {};
	void Init(IDirect3DDevice8* d) {
		if (!device)
			device = d;
		if (!vb)
			assert(SUCCEEDED(device->CreateVertexBuffer(
				MAX_N * sizeof(Vertex), D3DUSAGE_DYNAMIC, Vertex::FVF, D3DPOOL_DEFAULT, &vb)));
		init = true;
	}
	void Lock() {
		if (!init) return;
		vtx = nullptr;
		n = 0;
		assert(vb);
		assert(SUCCEEDED(vb->Lock(0, 0, (BYTE**)&vtx, D3DLOCK_DISCARD)));
		lock = true;
	}
	void Unlock() {
		if (!init) return;
		assert(SUCCEEDED(vb->Unlock()));
		lock = false;
	}
	void Flush() {
		device->SetVertexShader(Vertex::FVF);
		device->SetStreamSource(0, vb, sizeof(Vertex));
		device->DrawPrimitive(PRIMTYPE, 0, n / VTX_PER_PRIM);
	}
};

class Lines : public Buffer<XyzDiffuse> {
public:
	Lines() : Buffer(D3DPT_LINELIST, 2) { }
	void Line(const rmt::Vector&, const rmt::Vector&, D3DCOLOR color);
	void Line(const D3DXVECTOR3&, const D3DXVECTOR3&, D3DCOLOR color);
};

class Tris : public Buffer<XyzDiffuse> {
public:
	Tris() : Buffer(D3DPT_TRIANGLELIST, 3) { }
	void Tri(const rmt::Vector&, const rmt::Vector&, const rmt::Vector&, D3DCOLOR);
	void Tri(const D3DXVECTOR3&, const D3DXVECTOR3&, const D3DXVECTOR3&, D3DCOLOR);
};

class Carpenter {
public:
	IDirect3DDevice8* device = nullptr;
	DWORD stateblock = 0;

	Lines lines{};
	Tris tris{};

	bool lock = false;
	bool init = false;

	Carpenter() = default;
	void Init();
	void Final();
	void Flush();
};

extern Carpenter carpenter;
