#pragma once

#include <string>
#include <array>
#include <vector>
#include <winsock2.h>

static constexpr size_t 摘要长度 = 20; // zhāiyào chángdù

class 服务器 { // fuwuqi
public:
	static std::array<uint8_t, 摘要长度> Sha1(const std::string&);
	static std::string Base64(const std::array<uint8_t, 摘要长度>);
	static void 握手(SOCKET, const std::string&); // wòshǒu
	static void 处理客户端(SOCKET); // chuli kehuduan
	static void 运行(); // yunxing
};
