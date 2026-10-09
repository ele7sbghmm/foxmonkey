#include "thingamajig/server.h"
#include "thingamajig/utility.h"

#include <winsock2.h>
#include <thread>
#include <mswsock.h>
#include <vector>

#include <ws2tcpip.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

#include "d3d8.h"

std::mutex Server::mtx;
std::vector<SOCKET> Server::clients{};

auto Server::Sha1(const std::string& input) -> std::array<uint8_t, DIGEST_LEN> {
	BCRYPT_ALG_HANDLE alg = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	std::array<uint8_t, DIGEST_LEN> digest{};
	BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
	BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
	BCryptHashData(hash,
		reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
		static_cast<ULONG>(input.size()), 0);
	BCryptFinishHash(hash, digest.data(), DIGEST_LEN, 0);
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(alg, 0);
	return digest;
}
std::string Server::Base64(const std::array<uint8_t, DIGEST_LEN> digest) {
	static constexpr char BASE64[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out{};

	for (int i = 0; i < DIGEST_LEN; i += 3) {
		uint8_t b0 = digest[i];
		uint8_t b1 = (i + 1) < DIGEST_LEN ? digest[i + 1] : 0;
		uint8_t b2 = (i + 2) < DIGEST_LEN ? digest[i + 2] : 0;
		out += BASE64[b0 >> 2];
		out += BASE64[((b0 & 3) << 4) | (b1 >> 4)];
		out += (i + 1) < DIGEST_LEN ? BASE64[((b1 & 0xf) << 2) | (b2 >> 6)] : '=';
		out += (i + 2) < DIGEST_LEN ? BASE64[b2 & 0x3f] : '=';
	}
	return out;
}
bool Server::SendFile(SOCKET client, const std::string& name, const std::string& mime) {
	std::string path = "..\\..\\thingamajig\\web\\" + name;

	char buf[MAX_PATH]{};
	DWORD len = GetCurrentDirectory(MAX_PATH, buf);

	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER size{};
	GetFileSizeEx(hFile, &size);

	std::string http =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: " + mime + "\r\n"
		"Content-Length: " + std::to_string(size.QuadPart) + "\r\n"
		"Connection: close\r\n\r\n";

	TRANSMIT_FILE_BUFFERS tfb = { 0 };
	tfb.Head = reinterpret_cast<PVOID>(const_cast<char*>(http.c_str()));
	tfb.HeadLength = static_cast<DWORD>(http.size());
	BOOL result = TransmitFile(client, hFile, 0, 0, NULL, &tfb, 0);

	CloseHandle(hFile);
	return result == true;
}
void Server::WebsocketLoop(SOCKET client, const std::string& request) {
	std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	std::string needle = "Sec-WebSocket-Key: ";

	size_t start = request.find(needle);
	size_t end = request.find("\r\n", start);
	if (start == std::string::npos)
		return;

	start += needle.size();
	std::string key = request.substr(start, end - start);
	std::array<uint8_t, DIGEST_LEN> digest = Sha1(key + guid);
	std::string accept_key = Base64(digest);
	std::string response =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";

	send(client, response.c_str(), response.size(), 0);

	std::vector<uint8_t> buffer{};
	buffer.reserve(0x10000);
	uint8_t chunk[8192]{};

	{
		std::lock_guard<std::mutex> lock(mtx);
		clients.push_back(client);
	}
	while (true) {
		int len = recv(client, reinterpret_cast<char*>(chunk), static_cast<int>(sizeof(chunk)), 0);
		if (len <= 0)
			break;

		buffer.insert(buffer.end(), chunk, chunk + len);
		size_t offset = 0;

		while (true) {
			const uint8_t* current_ptr = buffer.data() + offset;
			size_t remaining = buffer.size() - offset;
		
			WebsocketFrame frame = ParseFrame(current_ptr, remaining);
			if (!frame.complete)
				break;
			if (frame.payload_len > 10 * 1024 * 1024)
				return;

			const uint8_t* payload_ptr = current_ptr + frame.header_size;
			std::vector<uint8_t> payload(payload_ptr, payload_ptr + frame.payload_len);
			
			if (frame.masked)
				for (int i = 0; i < payload.size(); ++i)
					payload[i] ^= frame.mask_key[i % 4];

			switch (frame.opcode) {
			case 1: // text
			case 2: // binary
			case 8: // close
			case 9: // ping
			default: break;
			}

			offset += frame.header_size + frame.payload_len;
		}

		if (offset > 0)
			buffer.erase(buffer.begin(), buffer.begin() + offset);
	}
	{
		std::lock_guard<std::mutex> lock(mtx);
		clients.erase(std::remove(clients.begin(), clients.end(), client));
	}
}
void Server::Run() {
	std::thread([]() {
		WSADATA wsa{};
		WSAStartup(MAKEWORD(2, 2), &wsa);

		SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		int opt = 1;
		setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(9000);
		addr.sin_addr.s_addr = INADDR_ANY;

		bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
		listen(server, 10);

		while (true) {
			SOCKET client = accept(server, nullptr, nullptr);
			if (client == INVALID_SOCKET) continue;

			std::thread([client]() {
				char buf[4096]{};
				int len = recv(client, reinterpret_cast<char*>(buf), sizeof(buf) - 1, 0);
				if (len > 0) {
					std::string request(buf, len);
					if (request.find("GET /ws ") != std::string::npos)
						WebsocketLoop(client, request);
					else if (request.find("GET /style.css") != std::string::npos)
						SendFile(client, "style.css", "text/css");
					else if (request.find("GET /index.js") != std::string::npos)
						SendFile(client, "index.js", "application/javascript");
					else if (request.find("GET /wgpu.js") != std::string::npos)
						SendFile(client, "wgpu.js", "application/javascript");
					else if (request.find("GET /wsock.js") != std::string::npos)
						SendFile(client, "wsock.js", "application/javascript");
					else if (request.find("GET /") != std::string::npos)
						SendFile(client, "index.html", "text/html");
				}

				shutdown(client, SD_BOTH);
				closesocket(client);
				}).detach();
		}

		closesocket(server);
		WSACleanup();
		}).detach();
}
WebsocketFrame Server::ParseFrame(const uint8_t* buffer, size_t size) {
	if (size < 2)
		return {};

	WebsocketFrame frame{};

	uint8_t b0 = buffer[0];
	frame.fin = (b0 & 0x80) != 0;
	frame.opcode = b0 & 0xf;
	uint8_t b1 = buffer[1];
	frame.masked = (b1 & 0x80) != 0;
	frame.payload_len = b1 & 0x7f;
	frame.header_size = 2;

	if (frame.payload_len == 126) {
		if (size < frame.header_size + 2)
			return {};

		frame.payload_len = (static_cast<uint64_t>(buffer[2]) << 8) | buffer[3];
		frame.header_size += 2;
	}
	else if (frame.payload_len == 127) {
		if (size < frame.header_size + 8)
			return {};

		frame.payload_len = 0;
		for (int i = 0; i < 8; ++i)
			frame.payload_len = (frame.payload_len << 8) | buffer[frame.header_size + i];

		frame.header_size += 8;
	}

	if (frame.masked) {
		if (size < frame.header_size + 4)
			return {};

		for (int i = 0; i < 4; ++i)
			frame.mask_key[i] = buffer[frame.header_size + i];

		frame.header_size += 4;
	}

	if (size < frame.header_size + frame.payload_len)
		return {};

	frame.complete = true;

	return frame;
}
auto Server::BuildFrame(const uint8_t* data, size_t len)->std::vector<uint8_t> {
	std::vector<uint8_t> frame{};
	frame.push_back(130);

	if (len <= 125)
		frame.push_back(static_cast<uint8_t>(len));
	else if (len <= 65535) {
		frame.push_back(126);
		frame.push_back(static_cast<uint8_t>((len >> 8) & 255));
		frame.push_back(static_cast<uint8_t>(len & 255));
	}
	else {
		auto len64 = static_cast<uint64_t>(len);
		frame.push_back(127);
		for (int i = 8; 0 < i--;)
			frame.push_back(static_cast<uint8_t>(len64 >> (8 * i) & 255));
	}

	frame.insert(frame.end(), data, data + len);
	return frame;
}
int Server::SendAll(SOCKET client, const char* data, int len) {
	int total = 0;
	while (total < len) {
		int r = send(client, data + total, len - total, 0);
		if (r <= 0)
			return r;
		total += r;
	}
	return total;
}
void Server::SendFrameToAllClients(const std::vector<uint8_t>& frame) {
	if (frame.empty())
		return;

	std::vector<SOCKET> copy;
	{
		std::lock_guard<std::mutex> lock(mtx);
		copy = clients;
	}
	const char* data = reinterpret_cast<const char*>(frame.data());
	int len = static_cast<int>(frame.size());

	std::vector<SOCKET> disconnected;
	for (SOCKET client : copy)
		if (!SendAll(client, data, len))
			disconnected.push_back(client);

	if (!disconnected.empty()) {
		std::lock_guard<std::mutex> lock(mtx);
		for (SOCKET dead : disconnected) {
			clients.erase(std::remove(clients.begin(), clients.end(), dead), clients.end());
			shutdown(dead, SD_BOTH);
		}
	}
}
void Server::SendCapture() {
	return;
	IDirect3DDevice8* device = get_device();
	if (!device)
		return;

	IDirect3DSurface8* backbuffer = nullptr;
	device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);

	D3DSURFACE_DESC desc{};
	backbuffer->GetDesc(&desc);

	IDirect3DSurface8* offscreen = nullptr;
	device->CreateImageSurface(desc.Width, desc.Height, desc.Format, &offscreen);
	device->CopyRects(backbuffer, nullptr, 0, offscreen, nullptr);

	D3DLOCKED_RECT lock{};
	if (SUCCEEDED(offscreen->LockRect(&lock, nullptr, D3DLOCK_READONLY))) {
		const uint8_t* src = reinterpret_cast<const uint8_t*>(lock.pBits);
		size_t total = lock.Pitch * desc.Height;

		std::vector<uint8_t> frame = BuildFrame(src, total);
		SendFrameToAllClients(frame);

		offscreen->UnlockRect();
	}
	offscreen->Release();
	backbuffer->Release();

	return;
}