#include "blg.h"

void BLG::Draw() {
	HRESULT hr = 0;
	if (!sb)
		hr = dev->CreateStateBlock(D3DSBT_ALL, &sb);
	if (!vb) {
		hr = dev->CreateVertexBuffer(sizeof(Vtx) * 10'000,
				                     D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
				                     Vtx::FVF,
				                     D3DPOOL_DEFAULT,
				                     &vb);
		return;
	}

	if (vtx.size() < 2) return;

	Vtx* data = nullptr;
	size_t size = vtx.size() * sizeof(Vtx);
	hr = (vb->Lock(0, size, (BYTE**)&data, D3DLOCK_DISCARD));
	assert(data);
	if (data != NULL)
		memcpy(data, vtx.data(), size);
	hr = vb->Unlock();

	hr = dev->CaptureStateBlock(sb);


	dev->SetTexture(0, NULL);
	dev->SetRenderState(D3DRS_LIGHTING, FALSE);
	dev->SetRenderState(D3DRS_COLORVERTEX, TRUE);
	dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	dev->SetRenderState(D3DRS_ZENABLE, FALSE);

	dev->SetVertexShader(Vtx::FVF);
	dev->SetStreamSource(0, vb, sizeof(Vtx));
	dev->DrawPrimitive(D3DPT_LINELIST, 0, vtx.size() / 2);
	vtx.clear();

	hr = dev->ApplyStateBlock(sb);
}
void BLG::Line(rmt::Vector& a, rmt::Vector& b, D3DCOLOR c) {
	Line(reinterpret_cast<D3DXVECTOR3&>(a), reinterpret_cast<D3DXVECTOR3&>(b), c);
}
void BLG::Line(D3DXVECTOR3& a, D3DXVECTOR3& b, D3DCOLOR c) {
	vtx.push_back({ a, c });
	vtx.push_back({ b, c });
}

BLG g_Blg{};
