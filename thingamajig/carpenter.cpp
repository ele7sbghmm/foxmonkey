#include "thingamajig/carpenter.h"
#include "thingamajig/server.h"

Carpenter carpenter{};

void Carpenter::Init() {
	if (!device) {
		IDirect3DDevice8* d = get_device();
		assert(d);
		device = d;
	}
	if (!stateblock)
		device->CreateStateBlock(D3DSBT_ALL, &stateblock);

	lines.Init(device);
	tris.Init(device);
}
void Carpenter::Final() {
	Init();

	D3DXVECTOR3 a = { 1000.f, 12.f, 0.f };
	D3DXVECTOR3 b = { 0.f, 12.f, 0.f };
	lines.Line(a, b, 0xff00ff00);

	D3DXVECTOR3 c = { 0.f, 12.f, 0.f };
	D3DXVECTOR3 d = { 0.f, 10.f, -10.f };
	D3DXVECTOR3 e = { 0.f, 10.f, 10.f };
	tris.Tri(c, d, e, 0x400000ff);

	lines.Unlock();
	tris.Unlock();

	Flush();

	lines.Lock();
	tris.Lock();
}
void Carpenter::Flush() {
	device->CaptureStateBlock(stateblock);

	device->SetTexture(0, NULL);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	device->SetRenderState(D3DRS_ZENABLE, TRUE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	tris.Flush();

	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	lines.Flush();
	
	device->ApplyStateBlock(stateblock);
}

void Lines::Line(const rmt::Vector& a, const rmt::Vector& b, D3DCOLOR color) {
	Line(reinterpret_cast<const D3DXVECTOR3&>(a), reinterpret_cast<const D3DXVECTOR3&>(b), color);
}
void Lines::Line(const D3DXVECTOR3& a, const D3DXVECTOR3& b, D3DCOLOR color) {
	if (!lock) return;
	vtx[n++] = { a, color };
	vtx[n++] = { b, color };
}

void Tris::Tri(const rmt::Vector& a, const rmt::Vector& b, const rmt::Vector& c, D3DCOLOR color) {
	Tri(reinterpret_cast<const D3DXVECTOR3&>(a), reinterpret_cast<const D3DXVECTOR3&>(b),
		reinterpret_cast<const D3DXVECTOR3&>(c), color);
}
void Tris::Tri(const D3DXVECTOR3& a, const D3DXVECTOR3& b, const D3DXVECTOR3& c, D3DCOLOR color) {
	if (!lock) return;
	D3DXVECTOR3 norm{ -1.f, 0.f, 0.f };
	vtx[n++] = { a, color };
	vtx[n++] = { b, color };
	vtx[n++] = { c, color };
}