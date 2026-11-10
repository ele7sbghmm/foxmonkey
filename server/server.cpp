#pragma warning(push)
#pragma warning(disable : 4005)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#pragma warning(pop)

#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace srr2
{

namespace
{

static const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const int kMaxClients = 64;

struct ServerState
{
    CRITICAL_SECTION lock;
    bool lockInitialized;
    volatile LONG running;
    SOCKET listenSocket;
    SOCKET clientSockets[kMaxClients];
    int clientCount;
    HANDLE serverThread;
    char wwwRoot[MAX_PATH];
    WebSocketMessageCallback onMessage;
    void* userData;
    ServerConfig asyncConfig;
    char asyncWwwRoot[MAX_PATH];
    uint8_t* fencePacket;
    size_t fencePacketSize;
    bool hasFencePacket;
};

static ServerState g_State = {
    {},
    false,
    0,
    INVALID_SOCKET,
    {},
    0,
    NULL,
    {0},
    NULL,
    NULL,
    {8080, NULL, NULL, NULL},
    {0},
    NULL,
    0,
    false
};

static void EnsureLockInitialized()
{
    if (!g_State.lockInitialized)
    {
        InitializeCriticalSection(&g_State.lock);
        for (int i = 0; i < kMaxClients; ++i)
        {
            g_State.clientSockets[i] = INVALID_SOCKET;
        }
        g_State.lockInitialized = true;
    }
}

static void AddClientSocket(SOCKET s)
{
    EnsureLockInitialized();
    EnterCriticalSection(&g_State.lock);
    for (int i = 0; i < kMaxClients; ++i)
    {
        if (g_State.clientSockets[i] == INVALID_SOCKET)
        {
            g_State.clientSockets[i] = s;
            g_State.clientCount++;
            break;
        }
    }
    LeaveCriticalSection(&g_State.lock);
}

static void RemoveClientSocket(SOCKET s)
{
    if (!g_State.lockInitialized)
    {
        return;
    }
    EnterCriticalSection(&g_State.lock);
    for (int i = 0; i < kMaxClients; ++i)
    {
        if (g_State.clientSockets[i] == s)
        {
            g_State.clientSockets[i] = INVALID_SOCKET;
            g_State.clientCount--;
            break;
        }
    }
    LeaveCriticalSection(&g_State.lock);
}

static bool FileExists(const char* path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void ResolveWwwRoot(const char* requestedRoot, char* outPath, size_t outSize)
{
    if (requestedRoot && requestedRoot[0] != '\0')
    {
        strncpy_s(outPath, outSize, requestedRoot, _TRUNCATE);
        return;
    }

    const char* candidates[] = {
        "www",
        "..\\www",
        "..\\..\\www",
        "..\\..\\..\\www"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        char testIndex[MAX_PATH];
        snprintf(testIndex, sizeof(testIndex), "%s\\index.html", candidates[i]);
        if (FileExists(testIndex))
        {
            GetFullPathNameA(candidates[i], (DWORD)outSize, outPath, NULL);
            return;
        }
    }

    // Also try relative to the executable directory (e.g. srr2/cd/pc/Simpsons-Debug.exe -> srr2/www)
    char exePath[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0)
    {
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash)
        {
            *lastSlash = '\0';
            for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
            {
                char candidateDir[MAX_PATH];
                char testIndex[MAX_PATH];
                snprintf(candidateDir, sizeof(candidateDir), "%s\\%s", exePath, candidates[i]);
                snprintf(testIndex, sizeof(testIndex), "%s\\index.html", candidateDir);
                if (FileExists(testIndex))
                {
                    GetFullPathNameA(candidateDir, (DWORD)outSize, outPath, NULL);
                    return;
                }
            }
        }
    }

    strncpy_s(outPath, outSize, "www", _TRUNCATE);
}

static void Base64Encode(const uint8_t* data, size_t len, char* out, size_t outSize)
{
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t j = 0;
    for (size_t i = 0; i < len && j + 4 < outSize; i += 3)
    {
        uint32_t octetA = i < len ? data[i] : 0;
        uint32_t octetB = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t octetC = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;

        out[j++] = kTable[(triple >> 18) & 0x3F];
        out[j++] = kTable[(triple >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? kTable[(triple >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? kTable[triple & 0x3F] : '=';
    }
    if (j < outSize)
    {
        out[j] = '\0';
    }
}

// Uses Windows CNG (bcrypt.h) to compute SHA-1(Sec-WebSocket-Key + GUID) and Base64-encode it
static bool ComputeWebSocketAcceptKeyBCrypt(
    const char* clientKey,
    char* outAccept,
    size_t outAcceptSize)
{
    char combined[256];
    int combinedLen = snprintf(combined, sizeof(combined), "%s%s", clientKey, kWebSocketGuid);
    if (combinedLen <= 0 || combinedLen >= (int)sizeof(combined))
    {
        return false;
    }

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    PUCHAR hashObject = NULL;
    DWORD hashObjectSize = 0;
    DWORD cbData = 0;
    uint8_t sha1Digest[20] = {0};
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0) >= 0)
    {
        if (BCryptGetProperty(
                hAlg,
                BCRYPT_OBJECT_LENGTH,
                (PUCHAR)&hashObjectSize,
                sizeof(DWORD),
                &cbData,
                0) >= 0)
        {
            hashObject = (PUCHAR)malloc(hashObjectSize);
            if (hashObject != NULL)
            {
                if (BCryptCreateHash(
                        hAlg,
                        &hHash,
                        hashObject,
                        hashObjectSize,
                        NULL,
                        0,
                        0) >= 0)
                {
                    if (BCryptHashData(
                            hHash,
                            (PUCHAR)combined,
                            (ULONG)combinedLen,
                            0) >= 0 &&
                        BCryptFinishHash(
                            hHash,
                            sha1Digest,
                            sizeof(sha1Digest),
                            0) >= 0)
                    {
                        Base64Encode(sha1Digest, sizeof(sha1Digest), outAccept, outAcceptSize);
                        ok = true;
                    }
                    BCryptDestroyHash(hHash);
                }
                free(hashObject);
            }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    return ok;
}

static bool SendAll(SOCKET s, const void* buf, size_t len)
{
    const char* ptr = (const char*)buf;
    size_t remaining = len;
    while (remaining > 0)
    {
        int sent = send(s, ptr, (int)remaining, 0);
        if (sent <= 0)
        {
            return false;
        }
        ptr += sent;
        remaining -= (size_t)sent;
    }
    return true;
}

static bool RecvAll(SOCKET s, void* buf, size_t len)
{
    char* ptr = (char*)buf;
    size_t remaining = len;
    while (remaining > 0)
    {
        int received = recv(s, ptr, (int)remaining, 0);
        if (received <= 0)
        {
            return false;
        }
        ptr += received;
        remaining -= (size_t)received;
    }
    return true;
}

static bool SendWebSocketFrame(SOCKET s, uint8_t opcode, const void* payload, size_t length)
{
    uint8_t header[10];
    size_t headerLen = 0;

    header[0] = (uint8_t)(0x80 | (opcode & 0x0F)); // FIN = 1
    if (length <= 125)
    {
        header[1] = (uint8_t)length;
        headerLen = 2;
    }
    else if (length <= 0xFFFF)
    {
        header[1] = 126;
        header[2] = (uint8_t)((length >> 8) & 0xFF);
        header[3] = (uint8_t)(length & 0xFF);
        headerLen = 4;
    }
    else
    {
        uint64_t len64 = (uint64_t)length;
        header[1] = 127;
        for (int i = 0; i < 8; ++i)
        {
            header[2 + i] = (uint8_t)((len64 >> ((7 - i) * 8)) & 0xFF);
        }
        headerLen = 10;
    }

    if (!SendAll(s, header, headerLen))
    {
        return false;
    }
    if (length > 0 && payload != NULL)
    {
        if (!SendAll(s, payload, length))
        {
            return false;
        }
    }
    return true;
}

static bool ExtractHeaderValue(
    const char* request,
    const char* headerName,
    char* outValue,
    size_t outSize)
{
    size_t nameLen = strlen(headerName);
    const char* line = request;
    while (line && *line)
    {
        const char* lineEnd = strstr(line, "\r\n");
        if (!lineEnd)
        {
            break;
        }
        if (_strnicmp(line, headerName, nameLen) == 0 && line[nameLen] == ':')
        {
            const char* valStart = line + nameLen + 1;
            while (valStart < lineEnd && (*valStart == ' ' || *valStart == '\t'))
            {
                valStart++;
            }
            size_t valLen = (size_t)(lineEnd - valStart);
            if (valLen >= outSize)
            {
                valLen = outSize - 1;
            }
            memcpy(outValue, valStart, valLen);
            outValue[valLen] = '\0';
            return true;
        }
        line = lineEnd + 2;
    }
    return false;
}

static const char* GetMimeType(const char* path)
{
    const char* ext = strrchr(path, '.');
    if (!ext)
    {
        return "application/octet-stream";
    }
    if (_stricmp(ext, ".html") == 0 || _stricmp(ext, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (_stricmp(ext, ".js") == 0 || _stricmp(ext, ".mjs") == 0)
        return "application/javascript; charset=utf-8";
    if (_stricmp(ext, ".css") == 0)
        return "text/css; charset=utf-8";
    if (_stricmp(ext, ".json") == 0 || _stricmp(ext, ".map") == 0)
        return "application/json; charset=utf-8";
    if (_stricmp(ext, ".ts") == 0 || _stricmp(ext, ".wgsl") == 0)
        return "text/plain; charset=utf-8";
    if (_stricmp(ext, ".wasm") == 0)
        return "application/wasm";
    if (_stricmp(ext, ".png") == 0)
        return "image/png";
    if (_stricmp(ext, ".ico") == 0)
        return "image/x-icon";
    return "application/octet-stream";
}

static void ServeStaticFile(SOCKET client, const char* wwwRoot, const char* urlPath)
{
    // Strip query string if present
    char cleanUrl[256] = {0};
    strncpy_s(cleanUrl, sizeof(cleanUrl), urlPath, _TRUNCATE);
    char* query = strchr(cleanUrl, '?');
    if (query)
    {
        *query = '\0';
    }

    if (strstr(cleanUrl, "..") != NULL)
    {
        const char* forbidden =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n\r\nForbidden";
        SendAll(client, forbidden, strlen(forbidden));
        return;
    }

    const char* relativePath = cleanUrl;
    if (strcmp(cleanUrl, "/") == 0)
    {
        relativePath = "/index.html";
    }

    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s%s", wwwRoot, relativePath);
    for (char* p = filePath; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\\';
        }
    }

    // Convenience fallback: if /main.js is requested and sits in /dist/main.js
    if (!FileExists(filePath) && _stricmp(cleanUrl, "/main.js") == 0)
    {
        snprintf(filePath, sizeof(filePath), "%s\\dist\\main.js", wwwRoot);
    }

    FILE* fp = NULL;
    if (fopen_s(&fp, filePath, "rb") != 0 || !fp)
    {
        const char* notFound =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n\r\nNot Found";
        SendAll(client, notFound, strlen(notFound));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize < 0)
    {
        fclose(fp);
        return;
    }

    char* fileData = (char*)malloc((size_t)fileSize > 0 ? (size_t)fileSize : 1);
    if (!fileData)
    {
        fclose(fp);
        return;
    }

    size_t bytesRead = fread(fileData, 1, (size_t)fileSize, fp);
    fclose(fp);

    char header[512];
    int headerLen = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",
        GetMimeType(filePath),
        bytesRead);

    if (headerLen > 0)
    {
        SendAll(client, header, (size_t)headerLen);
        if (bytesRead > 0)
        {
            SendAll(client, fileData, bytesRead);
        }
    }

    free(fileData);
}

static void HandleWebSocketSession(SOCKET client, const char* wsKey)
{
    char acceptKey[64] = {0};
    if (!ComputeWebSocketAcceptKeyBCrypt(wsKey, acceptKey, sizeof(acceptKey)))
    {
        return;
    }

    char handshakeResp[512];
    int respLen = snprintf(
        handshakeResp,
        sizeof(handshakeResp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        acceptKey);

    if (respLen <= 0 || !SendAll(client, handshakeResp, (size_t)respLen))
    {
        return;
    }

    const char* welcome = "srr2-server: websocket handshake ok (bcrypt sha1)";
    SendWebSocketFrame(client, 0x1, welcome, strlen(welcome));

    EnsureLockInitialized();
    EnterCriticalSection(&g_State.lock);
    for (int i = 0; i < kMaxClients; ++i)
    {
        if (g_State.clientSockets[i] == INVALID_SOCKET)
        {
            g_State.clientSockets[i] = client;
            g_State.clientCount++;
            break;
        }
    }
    if (g_State.hasFencePacket && g_State.fencePacket != NULL && g_State.fencePacketSize > 0)
    {
        SendWebSocketFrame(client, 0x2, g_State.fencePacket, g_State.fencePacketSize);
    }
    LeaveCriticalSection(&g_State.lock);

    while (InterlockedCompareExchange(&g_State.running, 0, 0) != 0)
    {
        uint8_t hdr[2];
        if (!RecvAll(client, hdr, 2))
        {
            break;
        }

        uint8_t opcode = hdr[0] & 0x0F;
        bool masked = (hdr[1] & 0x80) != 0;
        uint64_t payloadLen = hdr[1] & 0x7F;

        if (payloadLen == 126)
        {
            uint8_t ext[2];
            if (!RecvAll(client, ext, 2))
            {
                break;
            }
            payloadLen = ((uint64_t)ext[0] << 8) | (uint64_t)ext[1];
        }
        else if (payloadLen == 127)
        {
            uint8_t ext[8];
            if (!RecvAll(client, ext, 8))
            {
                break;
            }
            payloadLen = 0;
            for (int i = 0; i < 8; ++i)
            {
                payloadLen = (payloadLen << 8) | ext[i];
            }
        }

        // Guard against excessively large frames (16 MB limit)
        if (payloadLen > 16 * 1024 * 1024)
        {
            break;
        }

        uint8_t maskKey[4] = {0};
        if (masked)
        {
            if (!RecvAll(client, maskKey, 4))
            {
                break;
            }
        }

        uint8_t* payload = (uint8_t*)malloc((size_t)payloadLen + 1);
        if (!payload)
        {
            break;
        }

        if (payloadLen > 0)
        {
            if (!RecvAll(client, payload, (size_t)payloadLen))
            {
                free(payload);
                break;
            }
            if (masked)
            {
                for (size_t i = 0; i < (size_t)payloadLen; ++i)
                {
                    payload[i] ^= maskKey[i & 3];
                }
            }
        }
        payload[payloadLen] = '\0';

        if (opcode == 0x8) // Close frame
        {
            SendWebSocketFrame(client, 0x8, NULL, 0);
            free(payload);
            break;
        }
        else if (opcode == 0x9) // Ping -> Pong
        {
            SendWebSocketFrame(client, 0xA, payload, (size_t)payloadLen);
        }
        else if (opcode == 0x1 || opcode == 0x2) // Text or Binary
        {
            WebSocketMessageCallback cb = g_State.onMessage;
            void* ud = g_State.userData;
            if (cb)
            {
                cb((uintptr_t)client, payload, (size_t)payloadLen, opcode == 0x2, ud);
            }
            else if (opcode == 0x1)
            {
                char echoBuf[512];
                int n = snprintf(echoBuf, sizeof(echoBuf), "ack: %s", (const char*)payload);
                if (n > 0)
                {
                    SendWebSocketFrame(client, 0x1, echoBuf, (size_t)n);
                }
            }
        }

        free(payload);
    }

    RemoveClientSocket(client);
}

static DWORD WINAPI ClientWorkerProc(LPVOID param)
{
    SOCKET client = (SOCKET)(uintptr_t)param;

    char reqBuf[4096];
    int received = recv(client, reqBuf, sizeof(reqBuf) - 1, 0);
    if (received > 0)
    {
        reqBuf[received] = '\0';

        char method[16] = {0};
        char urlPath[256] = {0};
        sscanf_s(reqBuf, "%15s %255s", method, (unsigned)sizeof(method), urlPath, (unsigned)sizeof(urlPath));

        char upgradeHdr[64] = {0};
        char wsKey[128] = {0};
        bool hasUpgrade = ExtractHeaderValue(reqBuf, "Upgrade", upgradeHdr, sizeof(upgradeHdr));
        bool hasWsKey = ExtractHeaderValue(reqBuf, "Sec-WebSocket-Key", wsKey, sizeof(wsKey));

        if (hasUpgrade && _stricmp(upgradeHdr, "websocket") == 0 && hasWsKey)
        {
            HandleWebSocketSession(client, wsKey);
        }
        else if (_stricmp(method, "GET") == 0)
        {
            ServeStaticFile(client, g_State.wwwRoot, urlPath);
        }
    }

    shutdown(client, SD_BOTH);
    closesocket(client);
    return 0;
}

} // anonymous namespace

bool RunServerWithConfig(const ServerConfig& config)
{
    EnsureLockInitialized();

    if (InterlockedCompareExchange(&g_State.running, 1, 0) != 0)
    {
        return false; // Server is already running
    }

    ResolveWwwRoot(config.wwwRoot, g_State.wwwRoot, sizeof(g_State.wwwRoot));
    if (config.onMessage)
    {
        g_State.onMessage = config.onMessage;
        g_State.userData = config.userData;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        InterlockedExchange(&g_State.running, 0);
        return false;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        WSACleanup();
        InterlockedExchange(&g_State.running, 0);
        return false;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config.port ? config.port : 8080);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listenSock);
        WSACleanup();
        InterlockedExchange(&g_State.running, 0);
        return false;
    }

    g_State.listenSocket = listenSock;

    char logMsg[512];
    snprintf(
        logMsg,
        sizeof(logMsg),
        "[SRR2 Server] Listening on http://localhost:%u (www: %s)\n",
        config.port ? config.port : 8080,
        g_State.wwwRoot);
    OutputDebugStringA(logMsg);

    while (InterlockedCompareExchange(&g_State.running, 0, 0) != 0)
    {
        SOCKET client = accept(listenSock, NULL, NULL);
        if (client == INVALID_SOCKET)
        {
            if (InterlockedCompareExchange(&g_State.running, 0, 0) == 0)
            {
                break;
            }
            continue;
        }

        HANDLE hClientThread = CreateThread(
            NULL,
            0,
            ClientWorkerProc,
            (LPVOID)(uintptr_t)client,
            0,
            NULL);
        if (hClientThread)
        {
            CloseHandle(hClientThread);
        }
        else
        {
            closesocket(client);
        }
    }

    if (g_State.listenSocket != INVALID_SOCKET)
    {
        closesocket(g_State.listenSocket);
        g_State.listenSocket = INVALID_SOCKET;
    }

    WSACleanup();
    return true;
}

bool RunServer(unsigned short port, const char* wwwRoot)
{
    ServerConfig cfg;
    cfg.port = port;
    cfg.wwwRoot = wwwRoot;
    cfg.onMessage = g_State.onMessage;
    cfg.userData = g_State.userData;
    return RunServerWithConfig(cfg);
}

DWORD WINAPI ServerThreadProc(LPVOID lpParam)
{
    if (lpParam != NULL)
    {
        const ServerConfig* cfg = (const ServerConfig*)lpParam;
        RunServerWithConfig(*cfg);
    }
    else
    {
        RunServer(8080, NULL);
    }
    return 0;
}

bool StartServerThread(unsigned short port, const char* wwwRoot)
{
    EnsureLockInitialized();

    g_State.asyncConfig.port = port ? port : 8080;
    if (wwwRoot && wwwRoot[0] != '\0')
    {
        strncpy_s(g_State.asyncWwwRoot, sizeof(g_State.asyncWwwRoot), wwwRoot, _TRUNCATE);
        g_State.asyncConfig.wwwRoot = g_State.asyncWwwRoot;
    }
    else
    {
        g_State.asyncConfig.wwwRoot = NULL;
    }
    g_State.asyncConfig.onMessage = g_State.onMessage;
    g_State.asyncConfig.userData = g_State.userData;

    g_State.serverThread = CreateThread(
        NULL,
        0,
        ServerThreadProc,
        &g_State.asyncConfig,
        0,
        NULL);
    return g_State.serverThread != NULL;
}

void StopServer()
{
    InterlockedExchange(&g_State.running, 0);

    if (g_State.listenSocket != INVALID_SOCKET)
    {
        closesocket(g_State.listenSocket);
        g_State.listenSocket = INVALID_SOCKET;
    }

    if (g_State.lockInitialized)
    {
        EnterCriticalSection(&g_State.lock);
        for (int i = 0; i < kMaxClients; ++i)
        {
            if (g_State.clientSockets[i] != INVALID_SOCKET)
            {
                shutdown(g_State.clientSockets[i], SD_BOTH);
                closesocket(g_State.clientSockets[i]);
                g_State.clientSockets[i] = INVALID_SOCKET;
            }
        }
        g_State.clientCount = 0;
        LeaveCriticalSection(&g_State.lock);
    }

    if (g_State.serverThread != NULL)
    {
        WaitForSingleObject(g_State.serverThread, 2000);
        CloseHandle(g_State.serverThread);
        g_State.serverThread = NULL;
    }
}

void BroadcastText(const char* text)
{
    if (!text || !g_State.lockInitialized)
    {
        return;
    }
    size_t len = strlen(text);
    EnterCriticalSection(&g_State.lock);
    for (int i = 0; i < kMaxClients; ++i)
    {
        if (g_State.clientSockets[i] != INVALID_SOCKET)
        {
            SendWebSocketFrame(g_State.clientSockets[i], 0x1, text, len);
        }
    }
    LeaveCriticalSection(&g_State.lock);
}

void BroadcastBinary(const void* data, size_t length)
{
    if (!data || !g_State.lockInitialized)
    {
        return;
    }
    EnterCriticalSection(&g_State.lock);
    for (int i = 0; i < kMaxClients; ++i)
    {
        if (g_State.clientSockets[i] != INVALID_SOCKET)
        {
            SendWebSocketFrame(g_State.clientSockets[i], 0x2, data, length);
        }
    }
    LeaveCriticalSection(&g_State.lock);
}

void UpdateFences(const float* fenceSegmentsXZ, size_t fenceCount)
{
    EnsureLockInitialized();

    size_t floatBytes = (fenceSegmentsXZ != NULL) ? (fenceCount * 4 * sizeof(float)) : 0;
    size_t packetSize = 8 + floatBytes;
    uint8_t* packet = (uint8_t*)malloc(packetSize);
    if (!packet)
    {
        return;
    }

    uint32_t header[2];
    header[0] = 0x45434E46u; // 'FNCE' in little-endian
    header[1] = (fenceSegmentsXZ != NULL) ? (uint32_t)fenceCount : 0u;
    memcpy(packet, header, sizeof(header));
    if (floatBytes > 0 && fenceSegmentsXZ != NULL)
    {
        memcpy(packet + 8, fenceSegmentsXZ, floatBytes);
    }

    EnterCriticalSection(&g_State.lock);
    if (g_State.fencePacket != NULL)
    {
        free(g_State.fencePacket);
    }
    g_State.fencePacket = packet;
    g_State.fencePacketSize = packetSize;
    g_State.hasFencePacket = true;

    for (int i = 0; i < kMaxClients; ++i)
    {
        if (g_State.clientSockets[i] != INVALID_SOCKET)
        {
            SendWebSocketFrame(g_State.clientSockets[i], 0x2, g_State.fencePacket, g_State.fencePacketSize);
        }
    }
    LeaveCriticalSection(&g_State.lock);
}

bool SendText(uintptr_t clientSocket, const char* text)
{
    if (!text || (SOCKET)clientSocket == INVALID_SOCKET)
    {
        return false;
    }
    return SendWebSocketFrame((SOCKET)clientSocket, 0x1, text, strlen(text));
}

bool SendBinary(uintptr_t clientSocket, const void* data, size_t length)
{
    if (!data || (SOCKET)clientSocket == INVALID_SOCKET)
    {
        return false;
    }
    return SendWebSocketFrame((SOCKET)clientSocket, 0x2, data, length);
}

void SetMessageCallback(WebSocketMessageCallback callback, void* userData)
{
    g_State.onMessage = callback;
    g_State.userData = userData;
}

} // namespace srr2
