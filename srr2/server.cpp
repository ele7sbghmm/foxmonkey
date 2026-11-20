#include <ws2tcpip.h>
#include <mswsock.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

#include <string>
#include <vector>
#include <array>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "server.h"

static SOCKET s_lastClient = ~0;

static bool SendAll(SOCKET client, const void* buf, int len) {
	if (s_lastClient == INVALID_SOCKET) return false;
	client = s_lastClient;

	const char* ptr = (const char*)buf;
	int rem = len;
	while (rem > 0) {
		int sent = send(client, ptr, rem, 0);
		if (sent <= 0) return false;
		ptr += sent;
		rem -= sent;
	}
	return true;
}
void SendFrame(SOCKET client, u8 opcode, size_t len) {
	size_t hlen = 0;
	u8 header[10]{};
	header[0] = 0x80 | (opcode & 0xf);

	if (len <= 125) {
		header[1] = (u8)(len & 0xff);
		hlen = 2;
	}
	else if (len <= 0xffff) {
		header[1] = 126;
		header[2] = (len >> 8) & 0xff;
		header[3] = len & 0xff;
		hlen = 4;
	}
	else {
		u64 len64 = (u64)len;
		header[1] = 127;
		for (int i = 0; i < 8; ++i)
			header[2 + i] = len64 >> (7 - i) * 8 & 0xff;
		hlen = 10;
	}

	SendAll(client, (void*)header, hlen);
}
static bool RecvAll(SOCKET s, void* buf, int len) {
	char* ptr = (char*)buf;
	int rem = len;
	while (rem > 0) {
		int rec = recv(s, ptr, rem, 0);
		if (rec <= 0) return false;
		ptr += rec;
		rem -= rec;
	}
	return true;
}
static void Sha1(std::string_view in, u8 out[20]) {
	BCRYPT_ALG_HANDLE alg = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
	BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
	BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(in.data())), (ULONG)in.size(), 0);
	BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(out), 20, 0);
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(alg, 0);
}
static std::string Base64(const u8* data, int len) {
	static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";	

	std::string out;
	out.reserve(len / 3 * 4);
	for (int i = 0; i < len; i += 3) {
		u32 a = i < len ? data[i] : 0;
		u32 b = (i + 1) < len ? data[(i + 1)] : 0;
		u32 c = (i + 2) < len ? data[(i + 2)] : 0;
		u32 d = (a << 16) | (b << 8) | c;
		out += kTable[(d >> 18) & 0x3f];
		out += kTable[(d >> 12) & 0x3f];
		out += (i + 1) < len ? kTable[(d >> 6) & 0x3f] : '=';
		out += (i + 2) < len ? kTable[(d) & 0x3f] : '=';
	}
	return out;
}
static void Upgrade(SOCKET client, std::string_view req) {
	std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	std::string needle = "Sec-WebSocket-Key: ";

	size_t start = req.find(needle);
	if (start == std::string::npos) return;
	start += needle.size();
	size_t end = req.find("\r\n", start);

	std::string key(req.substr(start, end - start));
	u8 digest[20]{};
	Sha1(key + guid, digest);
	std::string accept_key = Base64(digest, sizeof(digest));

	std::string http = "HTTP/1.1 101 Switching Protocols\r\n"
					   "Upgrade: websocket\r\n"
					   "Connection: Upgrade\r\n"
  	                   "Sec-Websocket-Accept: " + accept_key + "\r\n\r\n";
	send(client, http.c_str(), http.size(), 0);
}
static std::string Http(std::string_view mime, int len) {
	return "HTTP/1.1 200 OK\r\n"
		   "Content-Type: " + std::string(mime) + "\r\n"
		   "Content-Length: " + std::to_string(len) + "\r\n" +
		   "Connection: close\r\n\r\n";
}
struct Frame {
	bool masked = false;
	bool fin = false;
	u8 mkey[4] = {0};
	size_t hsize = 0;
	u64 plen = 0;
	u8 op = 0;
};
static Frame ReadFrame(const u8* buf, size_t len) {
	if (len < 2) return {0};
	Frame frame{}; 

	u8 b0 = buf[0]; frame.hsize++;
	frame.fin = (b0 & 0x80) != 0;
	u8 op = b0 & 0xf;

	u8 b1 = buf[1]; frame.hsize++;
	frame.masked = (b1 & 0x80) != 0;
	frame.plen = b1 & 0x7f;

	if (frame.plen == 126) {
		if (len < frame.hsize + 2) return {0};
		frame.plen = ((u64)buf[2] << 8) | buf[3];
		frame.hsize += 2;
	} else if (frame.plen == 127) {
		if (len < 10) return {0};
		frame.plen = 0;
		for (int i = 0; i < 8; ++i) {
			frame.plen |= frame.plen << 8 | buf[2 + i];
			frame.hsize++;
		}
	}

	if (frame.plen > 0xff'ffff) return {0};

	if (frame.masked) {
		if (len < frame.hsize + 4) return {0};
			for (int i = 0; i < 4; ++i)
				frame.mkey[i] = buf[frame.hsize++];
	}
	if (len < frame.hsize + frame.plen) return {0};

	return frame;
}
static void SendFile(SOCKET client, std::string_view name, std::string_view mime) {
	const std::string path = "..\\..\\www\\" + std::string(name);

	char cwdBuffer[MAX_PATH];
	DWORD len = GetCurrentDirectoryA(MAX_PATH, cwdBuffer);

	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return;

	LARGE_INTEGER size;
	GetFileSizeEx(hFile, &size);

	std::string http = Http(mime, size.QuadPart);

	TRANSMIT_FILE_BUFFERS tfb = {0};
	tfb.Head = (PVOID)http.c_str();
	tfb.HeadLength = (DWORD)http.size();

	BOOL result = TransmitFile(client, hFile, 0, 0, NULL, &tfb, 0);

	CloseHandle(hFile);
}

static DWORD WINAPI ClientWorker(LPVOID param) {
	SOCKET client = (SOCKET)(uintptr_t)param;
	char req[4096];
	int rec = recv(client, req, sizeof(req) - 1, 0);
	if (rec > 0) {
		req[rec] = '\0';

		const std::string s{req, (size_t)rec};
		if (s.find("Sec-WebSocket-Key") != std::string::npos) {
			Upgrade(client, s);
			s_lastClient = client;
			return 0;
		}
		else if (s.find("GET /index.js") != std::string::npos)
			SendFile(client, "index.js", "application/javascript");
		else if (s.find("GET / HTTP/1.1") != std::string::npos)
			SendFile(client, "index.html", "text/html");
	}

	shutdown(client, SD_BOTH);
	closesocket(client);
	return 0;
}
DWORD WINAPI RunListener(LPVOID param) {
	WSADATA wsa{};
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int opt = 1;
	setsockopt(listener, SOL_SOCKET, NULL, (const char*)&opt, (int)sizeof(opt));

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9001);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(listener, (sockaddr*)&addr, sizeof(addr));
	listen(listener, SOMAXCONN);

	while (true) {
		SOCKET client = accept(listener, nullptr, nullptr);
		if (client == INVALID_SOCKET) continue;

		HANDLE thread = CreateThread(NULL, 0, ClientWorker, (LPVOID)(uintptr_t)client, 0, NULL);
		if (thread) CloseHandle(thread);
		else closesocket(client);
	}
	WSACleanup();
	return 0;
}
void SendVertices(const std::vector<Draw::Vertex3D>& vertices) {
	const size_t count = vertices.size();
	const size_t len = count * sizeof(Draw::Vertex3D);
	SendFrame(NULL, 2, len + sizeof(size_t));
	SendAll(NULL, (void*)&count, sizeof(size_t));
	SendAll(NULL, (void*)vertices.data(), len);
}