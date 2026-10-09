#include "玩意儿/服务器.h"

std::array<uint8_t, 摘要长度> 服务器::Sha1(const std::string& input) {}
std::string 服务器::Base64(std::array<uint8_t, 摘要长度> digest) {}
void 服务器::握手(SOCKET 客户端, const std::string& request) {}
void 服务器::处理客户端(SOCKET 客户端) {
	char buffer[1024]{};
	int len = recv(客户端, buffer, sizeof(buffer), 0);


}
void 服务器::运行() { // fuwuqi::yunxing
	WSADATA wsa数据结构{}; // shuju jiegou
	WSAStartup(MAKEWORD(2, 2), &wsa数据结构);

	SOCKET 套接字 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // taojiezi
	int opt = 1;
	setsockopt(套接字, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

	sockaddr_in 地址{}; // dizhi
	地址.sin_port = htons(9000);
	地址.sin_family = AF_INET;
	地址.sin_addr.s_addr = INADDR_ANY;
	bind(套接字, reinterpret_cast<sockaddr*>(&地址), sizeof(地址));
	listen(套接字, 10);

	while (true) {
		SOCKET 客户端 = accept(套接字, nullptr, nullptr); // kehuduan
		if (客户端 == INVALID_SOCKET)
			continue;

		处理客户端(客户端);
	}

	WSACleanup();
}