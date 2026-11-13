/* Actual local named-pipe I/O and sink ABI in an isolated x86 process. No game. */
#define _WIN32_WINNT 0x0600
#define P2PVS_TIMEOUT_MS 250u
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/p2p/p2p_verified_scripts.c"

static BYTE packet[128], copied[128];
static DWORD packet_size;
static int sink_calls, sink_refuse;
static const char *scenario;

__declspec(dllexport) int __cdecl Sh1xHost_UseVerifiedScripts(const void *data, unsigned size)
{
    ++sink_calls;
    assert(size == packet_size && !memcmp(data, packet, size));
    if (sink_refuse) return 0;
    memcpy(copied, data, size);
    return 1;
}

#if CODOL_LOCAL_TESTING
static DWORD WINAPI server(void *context)
{
    HANDLE pipe = context;
    DWORD size, count;
    char request[512];
    BOOL connected = ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;
    if (!connected) goto end;
    if (!ReadFile(pipe, &size, 4, &count, NULL) || count != 4) goto end;
    assert(size < sizeof(request));
    if (!ReadFile(pipe, request, size, &count, NULL) || count != size) goto end;
    request[size] = 0;
    assert(!strcmp(request,"{\"operation\":\"prepare\",\"map\":\"mp_killhouse_sh\",\"gametype\":\"war\",\"type\":\"mp\"}"));
    if (!strcmp(scenario, "timeout")) { Sleep(P2PVS_TIMEOUT_MS*2); goto end; }
    size = packet_size;
    if (!strcmp(scenario,"refusal")) size = 0;
    if (!strcmp(scenario,"oversize")) size = 0xffffffffu;
    if (!WriteFile(pipe,&size,4,&count,NULL) || count != 4) goto end;
    if (!strcmp(scenario,"refusal") || !strcmp(scenario,"oversize")) goto end;
    if (!strcmp(scenario,"truncated")) size = 2;
    if (!strcmp(scenario,"fragmented")) {
        DWORD n;
        for (n=0;n<size;++n)
            if (!WriteFile(pipe,packet+n,1,&count,NULL) || count!=1) goto end;
    } else WriteFile(pipe, packet, size, &count, NULL);
    FlushFileBuffers(pipe);
end:
    DisconnectNamedPipe(pipe);CloseHandle(pipe);return 0;
}

static void make_packet(void)
{
    static const char path[] = "maps/start.gsc", data[] = "main() {}\n";
    DWORD count = 1, total = sizeof(data)-1, name_size = sizeof(path)-1;
    memcpy(packet,"CVSPv1\0\0",8);
    memcpy(packet+8,&count,4);memcpy(packet+12,&total,4);
    memset(packet+16,0xa5,32);
    memcpy(packet+48,&name_size,4);memcpy(packet+52,&total,4);
    memcpy(packet+56,path,name_size);memcpy(packet+56+name_size,data,total);
    packet_size = 56+name_size+total;
}
#endif

int main(int argc, char **argv)
{
    void *blob = (void *)1;
    unsigned size = 1;
    char error[160] = {0};
    int result;
#if CODOL_LOCAL_TESTING
    DWORD started;
#endif
    assert(argc == 2);scenario = argv[1];
#if CODOL_LOCAL_TESTING
    if (!strcmp(scenario,"external")) {
        /* Interoperability test: the Python pipe server owns this child and
         * binds its actual PID/handle before releasing the stdin gate. */
        assert(getchar() == '\n');
        make_packet();
        assert(P2PVerifiedScriptsMode() == 1);
        assert(P2PVerifiedScriptsFetch("mp_killhouse_sh","war","mp",&blob,&size,error,sizeof(error)));
        assert(blob && size == packet_size && !memcmp(blob,packet,size));
        assert(P2PVerifiedScriptsInstall(GetModuleHandleA(NULL),blob,size,error,sizeof(error)));
        P2PVerifiedScriptsFree(blob,size);
        assert(sink_calls == 1 && !memcmp(copied,packet,packet_size));
        puts("PASS external: actual native consumer and Python SDK pipe interoperate");return 0;
    }
#endif
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED",NULL);
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",NULL);
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID",NULL);
    if (!strcmp(scenario,"off")) {
        assert(!P2PVerifiedScriptsAllowsMode(0) && !P2PVerifiedScriptsAllowsMode(1));
        assert(!P2PVerifiedScriptsFetch("mp_killhouse_sh","war","mp",&blob,&size,error,sizeof(error)));
        assert(!blob && !size && !sink_calls);
        assert(strstr(error,"required"));
        assert(!P2PVerifiedScriptsInstall(NULL,blob,size,error,sizeof(error)));
        puts("PASS off: P2P refuses unverified scripts");return 0;
    }
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED","1");
    assert(!P2PVerifiedScriptsAllowsMode(0));
    assert(P2PVerifiedScriptsAllowsMode(1) == CODOL_LOCAL_TESTING);
#if !CODOL_LOCAL_TESTING
    result = P2PVerifiedScriptsFetch("mp_killhouse_sh","war","mp",&blob,&size,error,sizeof(error));
    assert(!result && !blob && !size && !sink_calls);
    assert(strstr(error,"local-testing"));
    assert(!P2PVerifiedScriptsInstall(GetModuleHandleA(NULL),packet,48,error,sizeof(error)));
    assert(!sink_calls);
    puts("PASS shipping rejects selected SDK bridge");return 0;
#else
    {
        HANDLE pipe, thread;
        char name[128], pid[16];
        ULONG parent = GetCurrentProcessId();
        DWORD id, wait;
        _snprintf(name,sizeof(name),"\\\\.\\pipe\\CoDOLVerifiedScripts-%032lx",(unsigned long)parent);
        _snprintf(pid,sizeof(pid),"%lu",(unsigned long)parent);
        SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",name);
        SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID",pid);
        if (!strcmp(scenario,"missing")) SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",NULL);
        if (!strcmp(scenario,"badname")) SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE","\\\\remote\\pipe\\anything");
        if (!strcmp(scenario,"badpid")) SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID","-1");
        if (!strcmp(scenario,"badflag")) SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED","11");
        if (!strcmp(scenario,"missing") || !strcmp(scenario,"badname") ||
            !strcmp(scenario,"badpid") || !strcmp(scenario,"badflag") || !strcmp(scenario,"absent") || !strcmp(scenario,"selection")) {
            started = GetTickCount();
            assert(!P2PVerifiedScriptsFetch(!strcmp(scenario,"selection") ? "bad/path" : "mp_killhouse_sh",
                                           "war","mp",&blob,&size,error,sizeof(error)));
            assert(GetTickCount()-started < 1500 && !blob && !size && !sink_calls);
            printf("PASS %s: refused before any server DLL load\n",scenario);return 0;
        }
        make_packet();
        if (!strcmp(scenario,"magic")) packet[0] = 'X';
        if (!strcmp(scenario,"wrongpid")) SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID","4294967295");
        if (!strcmp(scenario,"sink")) sink_refuse = 1;
        pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1,4096,4096,500,NULL);
        assert(pipe != INVALID_HANDLE_VALUE);
        thread = CreateThread(NULL,0,server,pipe,0,&id);assert(thread);
        started = GetTickCount();
        result = P2PVerifiedScriptsFetch("mp_killhouse_sh","war","mp",&blob,&size,error,sizeof(error));
        if (!strcmp(scenario,"good") || !strcmp(scenario,"fragmented") || !strcmp(scenario,"sink") || !strcmp(scenario,"missingapi")) {
            assert(result && size == packet_size && !memcmp(blob,packet,size));
            if (!strcmp(scenario,"missingapi")) {
                assert(!P2PVerifiedScriptsInstall(GetModuleHandleA("kernel32.dll"),blob,size,error,sizeof(error)));
                assert(!sink_calls);
            } else {
                result = P2PVerifiedScriptsInstall(GetModuleHandleA(NULL),blob,size,error,sizeof(error));
                assert(result == !sink_refuse && sink_calls == 1);
                if (!sink_refuse) assert(!memcmp(copied,packet,size));
            }
        } else assert(!result && !blob && !size && !sink_calls);
        P2PVerifiedScriptsFree(blob,size);
        assert(GetTickCount()-started < 1500);
        wait = WaitForSingleObject(thread,2000);assert(wait == WAIT_OBJECT_0);CloseHandle(thread);
        printf("PASS %s: owned local pipe and bounded source handoff\n",scenario);
    }
    return 0;
#endif
}
