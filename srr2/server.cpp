#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "mswsock.lib")

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "server.h"

static std::atomic<SOCKET> s_lastClient{ INVALID_SOCKET };

static void ReplaceActiveClient(SOCKET newClient) {
	SOCKET old = s_lastClient.exchange(newClient, std::memory_order_acq_rel);
	if (old != INVALID_SOCKET && old != newClient) {
		shutdown(old, SD_BOTH);
		closesocket(old);
	}
}

static void InvalidateClient(SOCKET failedClient) {
	if (failedClient == INVALID_SOCKET) return;
	if (s_lastClient.compare_exchange_strong(failedClient, INVALID_SOCKET, std::memory_order_acq_rel)) {
		shutdown(failedClient, SD_BOTH);
		closesocket(failedClient);
	}
}

static bool SendAll(SOCKET client, const void* buf, int len) {
	if (client == INVALID_SOCKET || !buf || len < 0) return false;

	const char* ptr = static_cast<const char*>(buf);
	int rem = len;
	while (rem > 0) {
		int sent = send(client, ptr, rem, 0);
		if (sent <= 0) return false;
		ptr += sent;
		rem -= sent;
	}
	return true;
}

static size_t BuildFrameHeader(u8 header[10], u8 opcode, size_t len) {
	header[0] = 0x80 | (opcode & 0x0f);

	if (len <= 125) {
		header[1] = static_cast<u8>(len);
		return 2;
	}
	if (len <= 0xffff) {
		header[1] = 126;
		header[2] = static_cast<u8>((len >> 8) & 0xff);
		header[3] = static_cast<u8>(len & 0xff);
		return 4;
	}
	u64 len64 = static_cast<u64>(len);
	header[1] = 127;
	for (int i = 0; i < 8; ++i)
		header[2 + i] = static_cast<u8>((len64 >> ((7 - i) * 8)) & 0xff);
	return 10;
}

static bool Sha1(std::string_view in, u8 out[20]) {
	BCRYPT_ALG_HANDLE alg = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	bool ok = false;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) >= 0) {
		if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
			if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(in.data())), static_cast<ULONG>(in.size()), 0) >= 0 &&
				BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(out), 20, 0) >= 0) {
				ok = true;
			}
			BCryptDestroyHash(hash);
		}
		BCryptCloseAlgorithmProvider(alg, 0);
	}
	return ok;
}

static std::string Base64(const u8* data, int len) {
	static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	if (!data || len <= 0) return {};

	std::string out;
	out.reserve(static_cast<size_t>(((len + 2) / 3) * 4));
	for (int i = 0; i < len; i += 3) {
		u32 a = i < len ? data[i] : 0;
		u32 b = (i + 1) < len ? data[i + 1] : 0;
		u32 c = (i + 2) < len ? data[i + 2] : 0;
		u32 d = (a << 16) | (b << 8) | c;
		out += kTable[(d >> 18) & 0x3f];
		out += kTable[(d >> 12) & 0x3f];
		out += (i + 1) < len ? kTable[(d >> 6) & 0x3f] : '=';
		out += (i + 2) < len ? kTable[d & 0x3f] : '=';
	}
	return out;
}

static bool Upgrade(SOCKET client, std::string_view req) {
	static constexpr std::string_view kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	static constexpr std::string_view kNeedle = "Sec-WebSocket-Key: ";

	size_t start = req.find(kNeedle);
	if (start == std::string_view::npos) return false;
	start += kNeedle.size();
	size_t end = req.find("\r\n", start);
	if (end == std::string_view::npos) return false;

	std::string key(req.substr(start, end - start));
	u8 digest[20]{};
	if (!Sha1(key + std::string(kGuid), digest)) return false;
	std::string accept_key = Base64(digest, sizeof(digest));

	std::string http = "HTTP/1.1 101 Switching Protocols\r\n"
					   "Upgrade: websocket\r\n"
					   "Connection: Upgrade\r\n"
					   "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";
	return SendAll(client, http.data(), static_cast<int>(http.size()));
}

static std::string Http(std::string_view mime, u64 len) {
	return "HTTP/1.1 200 OK\r\n"
		   "Content-Type: " + std::string(mime) + "\r\n"
		   "Content-Length: " + std::to_string(len) + "\r\n"
		   "Connection: close\r\n\r\n";
}

static void SendHttp404(SOCKET client) {
	static constexpr char kNotFound[] =
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n";
	SendAll(client, kNotFound, static_cast<int>(sizeof(kNotFound) - 1));
}

struct Frame {
	bool valid = false;
	bool masked = false;
	bool fin = false;
	u8 mkey[4] = { 0 };
	size_t hsize = 0;
	u64 plen = 0;
	u8 op = 0;
};

static Frame ReadFrame(const u8* buf, size_t len) {
	if (!buf || len < 2) return {};
	Frame frame{};

	u8 b0 = buf[0];
	frame.hsize++;
	frame.fin = (b0 & 0x80) != 0;
	frame.op = b0 & 0x0f;

	u8 b1 = buf[1];
	frame.hsize++;
	frame.masked = (b1 & 0x80) != 0;
	frame.plen = b1 & 0x7f;

	if (frame.plen == 126) {
		if (len < frame.hsize + 2) return {};
		frame.plen = (static_cast<u64>(buf[2]) << 8) | buf[3];
		frame.hsize += 2;
	}
	else if (frame.plen == 127) {
		if (len < 10) return {};
		frame.plen = 0;
		for (int i = 0; i < 8; ++i) {
			frame.plen = (frame.plen << 8) | buf[2 + i];
			frame.hsize++;
		}
	}

	if (frame.plen > 0xff'ffff) return {};

	if (frame.masked) {
		if (len < frame.hsize + 4) return {};
		for (int i = 0; i < 4; ++i)
			frame.mkey[i] = buf[frame.hsize++];
	}
	if (len < frame.hsize + frame.plen) return {};

	frame.valid = true;
	return frame;
}

static void SendFile(SOCKET client, std::string_view name, std::string_view mime) {
	if (name.find("..") != std::string_view::npos ||
		name.find('/') != std::string_view::npos ||
		name.find('\\') != std::string_view::npos) {
		SendHttp404(client);
		return;
	}

	const std::string path = "..\\..\\www\\" + std::string(name);
	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		SendHttp404(client);
		return;
	}

	LARGE_INTEGER size{};
	if (!GetFileSizeEx(hFile, &size) || size.QuadPart < 0) {
		CloseHandle(hFile);
		SendHttp404(client);
		return;
	}

	std::string http = Http(mime, static_cast<u64>(size.QuadPart));

	TRANSMIT_FILE_BUFFERS tfb{};
	tfb.Head = const_cast<char*>(http.data());
	tfb.HeadLength = static_cast<DWORD>(http.size());

	TransmitFile(client, hFile, 0, 0, NULL, &tfb, 0);
	CloseHandle(hFile);
}

static DWORD WINAPI ClientWorker(LPVOID param) {
	SOCKET client = static_cast<SOCKET>(reinterpret_cast<uintptr_t>(param));
	char req[4096];
	int rec = recv(client, req, sizeof(req) - 1, 0);
	if (rec > 0) {
		req[rec] = '\0';

		const std::string_view s{ req, static_cast<size_t>(rec) };
		if (s.find("Sec-WebSocket-Key") != std::string_view::npos) {
			if (Upgrade(client, s)) {
				ReplaceActiveClient(client);

				// Keep the WebSocket worker alive to detect client close/disconnect.
				u8 buf[4096];
				while (true) {
					int n = recv(client, reinterpret_cast<char*>(buf), sizeof(buf), 0);
					if (n <= 0) break;
					Frame f = ReadFrame(buf, static_cast<size_t>(n));
					if (f.valid && f.op == 0x8) break;
				}
				InvalidateClient(client);
				return 0;
			}
		}
		else if (s.find("GET /index.js ") != std::string_view::npos)
			SendFile(client, "index.js", "application/javascript");
		else if (s.find("GET / ") != std::string_view::npos)
			SendFile(client, "index.html", "text/html");
		else
			SendHttp404(client);
	}

	shutdown(client, SD_BOTH);
	closesocket(client);
	return 0;
}

DWORD WINAPI RunListener(LPVOID param) {
	(void)param;
	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET) {
		WSACleanup();
		return 1;
	}

	int opt = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9001);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
		listen(listener, SOMAXCONN) == SOCKET_ERROR) {
		closesocket(listener);
		WSACleanup();
		return 1;
	}

	while (true) {
		SOCKET client = accept(listener, nullptr, nullptr);
		if (client == INVALID_SOCKET) continue;

		HANDLE thread = CreateThread(NULL, 0, ClientWorker, reinterpret_cast<LPVOID>(static_cast<uintptr_t>(client)), 0, NULL);
		if (thread) CloseHandle(thread);
		else closesocket(client);
	}

	closesocket(listener);
	WSACleanup();
	return 0;
}

void SendVertices(const std::vector<Draw::Vertex3D>& vertices) {
	SOCKET client = s_lastClient.load(std::memory_order_acquire);
	if (client == INVALID_SOCKET) return;

	const u32 count = static_cast<u32>(vertices.size());
	const size_t payloadBytes = static_cast<size_t>(count) * sizeof(Draw::Vertex3D);
	const size_t totalPayload = sizeof(u32) + payloadBytes;

	u8 header[10]{};
	const size_t hlen = BuildFrameHeader(header, 0x2, totalPayload);

	WSABUF bufs[3]{};
	DWORD bufCount = 2;
	bufs[0].buf = reinterpret_cast<CHAR*>(header);
	bufs[0].len = static_cast<ULONG>(hlen);
	bufs[1].buf = reinterpret_cast<CHAR*>(const_cast<u32*>(&count));
	bufs[1].len = static_cast<ULONG>(sizeof(u32));
	if (payloadBytes > 0) {
		bufs[2].buf = reinterpret_cast<CHAR*>(const_cast<Draw::Vertex3D*>(vertices.data()));
		bufs[2].len = static_cast<ULONG>(payloadBytes);
		bufCount = 3;
	}

	DWORD idx = 0;
	while (idx < bufCount) {
		DWORD sent = 0;
		if (WSASend(client, &bufs[idx], bufCount - idx, &sent, 0, nullptr, nullptr) == SOCKET_ERROR || sent == 0) {
			InvalidateClient(client);
			return;
		}
		DWORD rem = sent;
		while (idx < bufCount && rem >= bufs[idx].len) {
			rem -= bufs[idx].len;
			++idx;
		}
		if (idx < bufCount && rem > 0) {
			bufs[idx].buf += rem;
			bufs[idx].len -= rem;
		}
	}
}
