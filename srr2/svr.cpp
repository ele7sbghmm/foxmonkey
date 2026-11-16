#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>
#include <bcrypt.h>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

static const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static std::string Base64(const u8* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string out;
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        u32 a = i < len ? data[i] : 0;
        u32 b = (i + 1 < len ? data[i + 1] : 0);
        u32 c = (i + 2 < len ? data[i + 2] : 0);
        u32 d = (a << 16) | (b << 8) | c;

        out += table[(d >> 18) & 0x3f];
        out += table[(d >> 12) & 0x3f];
        out += (i + 1 < len) ? table[(d >> 6) & 0x3f] : '=';
        out += (i + 2 < len) ? table[d & 0x3] : '=';
    }

    return out;
}
static u8* Sha1(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    u8 digest[20] = {0};

    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
    BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hash, reinterpret_cast<BYTE*>(const_cast<char*>(data.data())), data.size(), 0);
    BCryptFinishHash(hash, digest, sizeof(digest), 0);

    return digest;
}
static std::string AcceptKey(const std::string& key) {
    const u8* digest = Sha1(key + kWebSocketGuid);
    return Base64(digest, 20);
}
static std::string ReadFrame(SOCKET client) {
    u8 header[2]{};
    if (RecvAll(client, reinterpret_cast<char*>(header), sizeof(header)) != 2) return {};
    u8 opcode = header[0] & 0xf;
    bool masked = (header[1] & 0x80) != 0;
    u64 payload_len = header[1] & 0x7f;

    if (payload_len == 126) {
        u8 ext[2];
        if (RecvAll(client, ext, sizeof(ext)) != sizeof(ext)) return {};
        payload_len = (static_cast<u64>(ext[0]) << 8) | ext[1];
    } else if (payload_len == 127) {
        u8 ext[8];
        if (RecvAll(client, ext, sizeof(ext)) != sizeof(ext)) return {};
        for (int i = 0; i < 8; ++i)
            payload_len = (payload_len << 8) | ext[i];
    }

    u8 mask[4];
    if (masked)
        if (RecvAll(client, mask, sizeof(mask)) != sizeof(mask)) return {};
    
    std::vector<u8> payload(static_cast<std::size_t>(payload_len));
    if (RecvAll(client, payload.data(), (int)payload_len) != payload_len) return {};
    if (masked)
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] ^= mask[i % 4];
    
    if (opcode == 8) return {};
    return std::string(payload.begin(), payload.end());
}
static void Handshake(const std::string& req) {

}