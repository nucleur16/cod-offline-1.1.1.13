/* Local verified-SDK -> hosted VM handoff. No network, cache or loose-source fallback.
 * The SDK authenticates/decrypts the release; the host DLL clones the immutable source blob.
 * This local process boundary is not remote attestation of an untrusted player's client. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../production_profile.h"

#ifndef P2PVS_TIMEOUT_MS
#define P2PVS_TIMEOUT_MS 30000u
#endif

typedef int (__cdecl *P2PVSInstallFn)(const void *, unsigned);

/* Zero is absent/off, one is explicitly selected, minus one is malformed. */
static int P2PVerifiedScriptsMode(void)
{
    char value[4];
    DWORD size = GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED", value, sizeof(value));
    if (!size || (size == 1 && value[0] == '0')) return 0;
    return size == 1 && value[0] == '1' ? 1 : -1;
}

static int P2PVerifiedScriptsAllowsMode(int inprocess)
{
    int selected = P2PVerifiedScriptsMode();
    return selected == 1 && CODOL_LOCAL_TESTING && inprocess;
}

#if CODOL_LOCAL_TESTING
/* Same ceiling as the service's encrypted payload. The host parser applies
 * independent source-total (64 MiB), per-file and count limits. */
#define P2PVS_MAX_BLOB (96u * 1024u * 1024u)

static int P2PVSIdentifier(const char *s, unsigned maximum)
{
    unsigned n = 0;
    if (!s || !*s) return 0;
    for (; *s; ++s) {
        if (++n > maximum || !((*s >= 'a' && *s <= 'z') ||
            (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    }
    return 1;
}

static int P2PVSPipeName(const char *name)
{
    static const char prefix[] = "\\\\.\\pipe\\CoDOLVerifiedScripts-";
    unsigned n;
    if (strncmp(name, prefix, sizeof(prefix)-1) || strlen(name) != sizeof(prefix)-1+32) return 0;
    for (n = sizeof(prefix)-1; name[n]; ++n)
        if (!((name[n] >= '0' && name[n] <= '9') || (name[n] >= 'a' && name[n] <= 'f'))) return 0;
    return 1;
}

static int P2PVSPid(const char *text, ULONG *pid)
{
    unsigned long value = 0;
    const char *s = text;
    if (!*s || *s == '0') return 0;
    for (; *s; ++s) {
        unsigned digit;
        if (*s < '0' || *s > '9') return 0;
        digit = (unsigned)(*s-'0');
        if (value > (0xfffffffful-digit)/10ul) return 0;
        value = value*10ul+digit;
    }
    *pid = value;
    return value != 0;
}

/* Every transfer shares one deadline. Cancellation is drained before the
 * OVERLAPPED/event/buffer are released, preventing a late kernel write. */
static int P2PVSTransfer(HANDLE pipe, void *buffer, DWORD size, int write, DWORD started)
{
    BYTE *cursor = (BYTE *)buffer;
    while (size) {
        OVERLAPPED operation;
        DWORD count = 0, elapsed = GetTickCount()-started;
        BOOL done;
        if (elapsed >= P2PVS_TIMEOUT_MS) return 0;
        ZeroMemory(&operation, sizeof(operation));
        operation.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!operation.hEvent) return 0;
        done = write ? WriteFile(pipe, cursor, size, &count, &operation) :
                       ReadFile(pipe, cursor, size, &count, &operation);
        if (!done && GetLastError() == ERROR_IO_PENDING) {
            elapsed = GetTickCount()-started;
            if (elapsed >= P2PVS_TIMEOUT_MS ||
                WaitForSingleObject(operation.hEvent, P2PVS_TIMEOUT_MS-elapsed) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &operation);
                /* Cancellation completes independent of a pipe server's reply. */
                GetOverlappedResult(pipe, &operation, &count, TRUE);
                CloseHandle(operation.hEvent);
                return 0;
            }
            done = GetOverlappedResult(pipe, &operation, &count, FALSE);
        }
        CloseHandle(operation.hEvent);
        if (!done || !count || count > size) return 0;
        cursor += count;
        size -= count;
    }
    return GetTickCount()-started < P2PVS_TIMEOUT_MS;
}
#endif

static void P2PVerifiedScriptsFree(void *blob, unsigned size)
{
    if (blob) { SecureZeroMemory(blob, size); HeapFree(GetProcessHeap(), 0, blob); }
}

/* Called on P2PInprocRun's worker before the server DLL is even loaded.
 * An absent service must not silently select local loose script sources. */
static int P2PVerifiedScriptsFetch(const char *map, const char *gametype, const char *category,
                                  void **output, unsigned *output_size, char *error, unsigned error_size)
{
    int setting = P2PVerifiedScriptsMode();
    const char *failure = "verified script handoff unavailable";
    *output = NULL; *output_size = 0;
    if (!setting) failure = "verified script service is required for P2P hosting";
    else if (setting != 1) failure = "invalid verified script opt-in";
#if !CODOL_LOCAL_TESTING
    else failure = "verified SDK bridge requires a local-testing client build";
    (void)map; (void)gametype; (void)category;
#else
    else {
        typedef BOOL (WINAPI *ServerPidFn)(HANDLE, PULONG);
        char name[128], parent[16], request[320];
        DWORD name_size, parent_size, size = 0, started = GetTickCount();
        ULONG expected_pid, actual_pid = 0;
        HANDLE pipe = INVALID_HANDLE_VALUE;
        BYTE *blob = NULL;
        int result = 0, length;
        ServerPidFn server_pid = (ServerPidFn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNamedPipeServerProcessId");
        name_size = GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE", name, sizeof(name));
        parent_size = GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID", parent, sizeof(parent));
        if (!server_pid) { failure = "local script channel identity check unavailable"; goto end; }
        if (!name_size || name_size >= sizeof(name) || !P2PVSPipeName(name) ||
            !parent_size || parent_size >= sizeof(parent) || !P2PVSPid(parent, &expected_pid)) {
            failure = "invalid local verified script channel"; goto end;
        }
        if (!P2PVSIdentifier(map, 63) || !P2PVSIdentifier(gametype, 31) || !P2PVSIdentifier(category, 15)) {
            failure = "invalid script match selection"; goto end;
        }
        length = _snprintf(request, sizeof(request),
            "{\"operation\":\"prepare\",\"map\":\"%s\",\"gametype\":\"%s\",\"type\":\"%s\"}", map, gametype, category);
        if (length <= 0 || (unsigned)length >= sizeof(request)) goto end;
        if (!WaitNamedPipeA(name, P2PVS_TIMEOUT_MS)) goto end;
        pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL);
        if (pipe == INVALID_HANDLE_VALUE) goto end;
        if (!server_pid(pipe, &actual_pid) || actual_pid != expected_pid) {
            failure = "verified script channel belongs to a different process"; goto end;
        }
        size = (DWORD)length;
        if (!P2PVSTransfer(pipe, &size, 4, 1, started) ||
            !P2PVSTransfer(pipe, request, size, 1, started) ||
            !P2PVSTransfer(pipe, &size, 4, 0, started)) goto end;
        if (size < 48 || size > P2PVS_MAX_BLOB) { failure = "verified script response refused or exceeds limits"; goto end; }
        blob = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size);
        if (!blob) { failure = "not enough memory for verified scripts"; goto end; }
        if (!P2PVSTransfer(pipe, blob, size, 0, started)) goto end;
        if (memcmp(blob, "CVSPv1\0\0", 8)) { failure = "invalid verified script response format"; goto end; }
        *output = blob; *output_size = size; blob = NULL;
        result = 1;
    end:
        P2PVerifiedScriptsFree(blob, size);
        if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe);
        SecureZeroMemory(request, sizeof(request));
        if (result) return 1;
    }
#endif
    if (error && error_size) {
        strncpy(error, failure, error_size-1);
        error[error_size-1] = 0;
    }
    return 0;
}

/* The server export clones the fetched buffer before returning. The caller
 * erases its transient IPC copy on success and failure. No Init has run yet. */
static int P2PVerifiedScriptsInstall(HMODULE module, const void *blob, unsigned size,
                                    char *error, unsigned error_size)
{
    const char *failure;
    if (!blob || !size) {
        failure = "verified script inventory is required for P2P hosting";
        goto refused;
    }
#if CODOL_LOCAL_TESTING
    P2PVSInstallFn install;
    install = (P2PVSInstallFn)(void *)GetProcAddress(module, "Sh1xHost_UseVerifiedScripts");
    failure = !install ? "host lacks the verified script handoff API" :
                        "host rejected the verified script inventory";
    if (install && install(blob, size) == 1) return 1;
#else
    (void)module;
    failure = "verified SDK bridge requires a local-testing client build";
#endif
refused:
    if (error && error_size) {
        strncpy(error, failure, error_size-1); error[error_size-1] = 0;
    }
    return 0;
}

/* New duplex startup is an explicit, isolated protocol. Legacy preparation
 * above remains unchanged; it cannot silently accept a principal envelope. */
#include "p2p_startup_bridge.c"
