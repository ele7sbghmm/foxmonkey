#pragma once

#include <string>
#include <array>
#include <vector>
#include <mutex>

struct IDirect3DDevice8;

static constexpr size_t DIGEST_LEN = 20;
using SOCKET = unsigned int;

struct WebsocketFrame {
	bool complete = false;
	bool fin = false;
	uint8_t opcode = 0;
	bool masked = false;
	uint64_t payload_len = 0;
	uint8_t mask_key[4] = {};
	size_t header_size = 0;
};

class Server {
public:
	static std::mutex mtx;
	static std::vector<SOCKET> clients;
	static void* fuck_this;

	static auto Sha1(const std::string&) -> std::array<uint8_t, DIGEST_LEN>;
	static std::string Base64(const std::array<uint8_t, DIGEST_LEN>);
	static bool SendFile(SOCKET, const std::string&, const std::string&);
	static void WebsocketLoop(SOCKET, const std::string&);
	static void Run();
	static auto ParseFrame(const uint8_t*, size_t) -> WebsocketFrame;
	static auto BuildFrame(const uint8_t*, size_t) -> std::vector<uint8_t>;
	static int SendAll(SOCKET, const char*, int);
	static void SendFrameToAllClients(const std::vector<uint8_t>&);
	static void SendCapture();
};
