#ifndef SRR2_SERVER_H
#define SRR2_SERVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4005)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#pragma warning(pop)
#endif

namespace srr2
{

typedef void (*WebSocketMessageCallback)(
    uintptr_t clientSocket,
    const uint8_t* payload,
    size_t length,
    bool isBinary,
    void* userData
);

struct ServerConfig
{
    unsigned short port;
    const char* wwwRoot; // Optional: if NULL, auto-detects srr2/www
    WebSocketMessageCallback onMessage;
    void* userData;
};

// Blocking server loop (call this from your own thread, or use StartServerThread)
bool RunServer(unsigned short port = 8080, const char* wwwRoot = NULL);
bool RunServerWithConfig(const ServerConfig& config);

// Win32 thread entry point compatible with CreateThread(..., &srr2::ServerThreadProc, ...)
// lpParam can be NULL (defaults to port 8080) or a pointer to a static/heap ServerConfig.
#ifdef _WIN32
DWORD WINAPI ServerThreadProc(LPVOID lpParam);
#endif

// Spawns a background Win32 thread running the HTTP + WebSocket server
bool StartServerThread(unsigned short port = 8080, const char* wwwRoot = NULL);

// Signals the server and all client connections to shut down
void StopServer();

// Broadcasts a UTF-8 text or binary WebSocket frame to all connected clients
void BroadcastText(const char* text);
void BroadcastBinary(const void* data, size_t length);

// Caches the latest loaded FenceEntityDSG segments (4 floats per fence: x1, z1, x2, z2)
// and broadcasts the binary payload to all connected WebSocket clients (and to new clients on connect).
void UpdateFences(const float* fenceSegmentsXZ, size_t fenceCount);

// Sends a UTF-8 text or binary WebSocket frame to a specific client socket
bool SendText(uintptr_t clientSocket, const char* text);
bool SendBinary(uintptr_t clientSocket, const void* data, size_t length);

// Sets a callback invoked whenever a WebSocket frame is received from a client
void SetMessageCallback(WebSocketMessageCallback callback, void* userData = NULL);

} // namespace srr2

#endif // SRR2_SERVER_H
