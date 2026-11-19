#pragma once

#include <winsock2.h>
#include <cstdint>

#include "draw.h"

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

DWORD WINAPI RunListener(LPVOID param);
void SendVertices(const std::vector<Draw::Vertex3D>& vertices);
