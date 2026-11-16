#pragma once

#include <vector>
#include <d3dx8.h>

#define D3DFVF_CUSTOMVERTEX 
struct Vtx {
	D3DXVECTOR3 xyz;
	D3DCOLOR diffuse;
	static constexpr DWORD FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;
};

class BLG {
public:
	IDirect3DDevice8* dev;
	IDirect3DVertexBuffer8* vb;
	DWORD sb;
	std::vector<Vtx> vtx;

	bool locked = false;

	void Init(IDirect3DDevice8* device) {
		dev = device;
		vtx.reserve(10'000);
	}
	void Line(rmt::Vector& a, rmt::Vector& b, D3DCOLOR c);
	void Line(D3DXVECTOR3& a, D3DXVECTOR3& b, D3DCOLOR c);
	void Draw();
};

extern BLG g_Blg;
