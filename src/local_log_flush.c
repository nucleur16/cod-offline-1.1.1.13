/* Fixed local-development log visibility API. C1 d8211b2... only.
 * Com_LogPrint 594010 holds CRITICAL_SECTION 2B62AB0, writes handle
 * [2574660] through FS_Write615120. The latter's optional flush at61518E
 * and the exact log shutdown caller62C0D3..62C0F3 call C1's cdecl
 * fflushADB1F7(FILE*), not this DLL's CRT. Never flush NULL/all streams.
 * This command writes no native log content: only a fixed nonce/PID receipt,
 * optionally bound to a fresh exchange nonce while the launch prefix stays fixed.
 */
#include "production_profile.h"

typedef enum {
    LF_NOT_RUN,LF_ARGUMENTS,LF_PID,LF_NONCE,LF_EXCHANGE,LF_MULTI,LF_VERIFIED_FLAG,LF_POLICY_PATH,
    LF_POLICY_OPEN,LF_POLICY_READ,LF_POLICY_ROUTE,LF_PIPE_ENV,LF_PIPE_API,LF_PIPE_OPEN,
    LF_PIPE_QUERY,LF_PIPE_OWNER,LF_GUARD_FLUSH,LF_GUARD_CALLER,LF_GUARD_DVAR,
    LF_GUARD_PREFIX,LF_GUARD_LOCK,LF_GUARD_LOCK_IMPORT,LF_PREFIX_READ,LF_PREFIX_VALUE,LF_NATIVE_READ,
    LF_NATIVE_HANDLE,LF_NATIVE_ENTRY,LF_NATIVE_FILENAME,LF_NATIVE_STREAM,LF_NATIVE_FLUSH,
    LF_RECEIPT_PATH,LF_RECEIPT_BODY,LF_RECEIPT_CREATE,LF_RECEIPT_WRITE,LF_RECEIPT_CLOSE,
    LF_SUCCESS,LF_SHIPPING
} K33LfStage;
static K33LfStage g_k33LfStage=LF_NOT_RUN;
static int K33LfFail(K33LfStage stage) { g_k33LfStage=stage;return 0; }
static const char *K33LfStageName(void)
{
#if K33_LOCAL_TESTING
    static const char *const names[]={
        "not-run","arguments","pid","nonce","exchange","multi-opt-in","verified-flag","policy-path",
        "policy-open","policy-read","policy-route","pipe-environment","pipe-api","pipe-open",
        "pipe-query","pipe-owner","guard-flush","guard-log-caller","guard-dvar",
        "guard-prefix-caller","guard-log-lock","guard-lock-import","prefix-read","prefix-value","native-memory",
        "native-handle","native-entry","native-filename","native-stream","native-flush",
        "receipt-path","receipt-body","receipt-create","receipt-write","receipt-close",
        "success","shipping"};
    return (unsigned)g_k33LfStage<sizeof(names)/sizeof(names[0]) ? names[g_k33LfStage]:"invalid-stage";
#else
    return "shipping";
#endif
}

#if K33_LOCAL_TESTING
#include "local_multi_policy.h"
/* The launcher's CODOL_VERIFIED_SCRIPTS_REQUIRED flag (the verified-script service itself was removed with
 * p2p_verified_scripts.c): zero is absent/off, one is explicitly selected, minus one is malformed. */
static int K33LfVerifiedFlag(void)
{
    char value[4];
    DWORD size = GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED", value, sizeof(value));
    if (!size || (size == 1 && value[0] == '0')) return 0;
    return size == 1 && value[0] == '1' ? 1 : -1;
}
/* The launcher's identity pipe name and parent pid, as p2p_verified_scripts.c accepted them. */
static int K33LfPipeName(const char *name)
{
    static const char prefix[] = "\\\\.\\pipe\\CoDOLVerifiedScripts-";
    unsigned n;
    if (strncmp(name, prefix, sizeof(prefix)-1) || strlen(name) != sizeof(prefix)-1+32) return 0;
    for (n = sizeof(prefix)-1; name[n]; ++n)
        if (!((name[n] >= '0' && name[n] <= '9') || (name[n] >= 'a' && name[n] <= 'f'))) return 0;
    return 1;
}
static int K33LfPid(const char *text, ULONG *pid)
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

#define K33LF_LOG_HANDLE 0x02574660u
#define K33LF_FILES 0x02AAA320u
#define K33LF_STRIDE 0x11Cu
#define K33LF_LOCK 0x02B62AB0u
#define K33LF_DVAR 0x006232C0u
#define K33LF_FLUSH 0x00ADB1F7u

static const char kK33LfC1Hash[]="d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60";
static const char kK33LfAbi[]="c1-d8211b2-logflush-v1";
static const char kK33LfExchangeAbi[]="c1-d8211b2-logflush-v2";

#ifndef K33LF_AT
#define K33LF_AT(va) ((void *)(DWORD_PTR)(va))
#endif
#ifndef K33LF_WRITE
#define K33LF_WRITE WriteFile
#endif

static int K33LfNonce(const char *value)
{
    if (!value) return 0;
    for (unsigned i=0;i<32;i++)
        if (!value[i] || !((value[i]>='0' && value[i]<='9') || (value[i]>='a' && value[i]<='f'))) return 0;
    return value[32]==0;
}

static int K33LfBytes(DWORD address,const BYTE *bytes,unsigned size)
{
    const void *at=K33LF_AT(address);
    return at && Readable(at,size) && !memcmp(at,bytes,size);
}

static int K33LfGuard(void)
{
    /* Independently pin the ABI entry, its exact current-log caller, the
     * filename lookup, and the same critical section used by Com_LogPrint. */
    static const BYTE flush[]={0x6a,0x0c,0x68,0x00,0x48,0xc9,0x00,0xe8,0xbd,0xd4,0x00,0x00,0x33,0xf6};
    static const BYTE caller[]={0xa1,0x60,0x46,0x57,0x02,0x56,0x57,0x8b,0xf9,0x89,0xbd,0xf4,0xfe,0xff,0xff,
        0x85,0xc0,0x74,0x31,0x69,0xc0,0x1c,0x01,0x00,0x00,0x8b,0x80,0x20,0xa3,0xaa,0x02,
        0x50,0xe8,0xff,0xf0,0x4a,0x00};
    static const BYTE dvar[]={0x55,0x8b,0xec,0x51,0x53,0x56,0x57,0x68,0xd0,0x98,0xab,0x02};
    static const BYTE prefix[]={0x68,0xac,0x12,0xc6,0x00,0x8b,0xf8,0xe8,0x88,0xf3,0x08,0x00};
    static const BYTE lock_head[]={0x68,0xb0,0x2a,0xb6,0x02,0xff,0x15};
    static const BYTE lock_tail[]={0x83,0x3d,0xc4,0xd8,0xc3,0x06,0x00,0x0f};
    if (!K33LfBytes(K33LF_FLUSH,flush,sizeof(flush))) return K33LfFail(LF_GUARD_FLUSH);
    if (!K33LfBytes(0x0062C0D3u,caller,sizeof(caller))) return K33LfFail(LF_GUARD_CALLER);
    if (!K33LfBytes(K33LF_DVAR,dvar,sizeof(dvar))) return K33LfFail(LF_GUARD_DVAR);
    if (!K33LfBytes(0x00593F2Cu,prefix,sizeof(prefix))) return K33LfFail(LF_GUARD_PREFIX);
    /* make_exe.py preserves the packed executable's .text, whose call
     * uses B421EC; the reconstructed C1 reference uses9A1D2FC. The exact
     * live and on-disk packed instruction agree. No other operand is allowed.
     * FillImports resolves B421EC as KERNEL32!EnterCriticalSection. Also
     * validate the actual function, so a shaped but redirected call refuses. */
    const BYTE *lock=(const BYTE *)K33LF_AT(0x00594020u);
    if (!lock || !Readable(lock,19) || memcmp(lock,lock_head,sizeof(lock_head)) ||
        memcmp(lock+11,lock_tail,sizeof(lock_tail))) return K33LfFail(LF_GUARD_LOCK);
    DWORD slot;memcpy(&slot,lock+7,4);
    if (slot!=0x00B421ECu && slot!=0x09A1D2FCu) return K33LfFail(LF_GUARD_LOCK);
    FARPROC genuine=GetProcAddress(GetModuleHandleA("kernel32.dll"),"EnterCriticalSection");
    const FARPROC *resolved=(const FARPROC *)K33LF_AT(slot);
    if (!genuine || !resolved || !Readable(resolved,sizeof(*resolved)) || *resolved!=genuine)
        return K33LfFail(LF_GUARD_LOCK_IMPORT);
    return 1;
}

static int K33LfPolicy(void)
{
    int argc=0;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    int selected=argv && LocalMultiArgument(argc,(const wchar_t *const *)argv);
    if (argv) LocalFree(argv);
    if (!selected) return K33LfFail(LF_MULTI);
    if (K33LfVerifiedFlag()!=1) return K33LfFail(LF_VERIFIED_FLAG);
    char path[MAX_PATH],data[256];DWORD size=GetModuleFileNameA(NULL,path,sizeof(path));
    if (!size || size>=sizeof(path)) return K33LfFail(LF_POLICY_PATH);
    while (size && path[size-1]!='\\') size--;
    if (!size || size+sizeof("dwauth.ini")>sizeof(path)) return K33LfFail(LF_POLICY_PATH);
    memcpy(path+size,"dwauth.ini",sizeof("dwauth.ini"));
    HANDLE file=LocalQaOpenDwauth(path);
    if (file==INVALID_HANDLE_VALUE) return K33LfFail(LF_POLICY_OPEN);
    DWORD bytes=GetFileSize(file,NULL),got=0;
    int ok=bytes>0 && bytes<=sizeof(data) && ReadFile(file,data,bytes,&got,NULL) && got==bytes;
    CloseHandle(file);
    if (!ok) return K33LfFail(LF_POLICY_READ);
    return (LocalMultiRoute(data,bytes) && bytes>=10 &&
        !memcmp(data,"127.0.0.3",9) && (data[9]=='\n' || data[9]=='\r')) ? 1:K33LfFail(LF_POLICY_ROUTE);
}

static int K33LfPipe(void)
{
    char name[128],text[16];ULONG expected=0,actual=0;
    /* A separate original-child identity endpoint avoids consuming/busy-opening
     * the signed startup CPSQ transaction. Invalid explicit selection never
     * falls back to another pipe. The legacy absent-variable route is unchanged. */
    DWORD a,b,probe_error;
    SetLastError(ERROR_SUCCESS);
    a=GetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE",name,sizeof(name));
    probe_error=GetLastError();
    if (!a && probe_error==ERROR_ENVVAR_NOT_FOUND)
        a=GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",name,sizeof(name));
    b=GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID",text,sizeof(text));
    if (!a || a>=sizeof(name) || !b || b>=sizeof(text) || !K33LfPipeName(name) || !K33LfPid(text,&expected)) return K33LfFail(LF_PIPE_ENV);
    typedef BOOL (WINAPI *ServerPid)(HANDLE,PULONG);
    ServerPid server=(ServerPid)GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetNamedPipeServerProcessId");
    if (!server) return K33LfFail(LF_PIPE_API);
    /* No WaitNamedPipe or I/O: busy/missing service refuses immediately.
     * Identification QoS prevents a pipe server impersonating C1. */
    HANDLE pipe=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
    if (pipe==INVALID_HANDLE_VALUE) return K33LfFail(LF_PIPE_OPEN);
    int queried=server(pipe,&actual);
    CloseHandle(pipe);
    if (!queried) return K33LfFail(LF_PIPE_QUERY);
    return actual==expected ? 1:K33LfFail(LF_PIPE_OWNER);
}

static const char *K33LfPrefix(void)
{
#ifdef K33LF_FIXTURE
    const BYTE *dvar=K33LfFixtureDvar();
#else
    const BYTE *dvar=((const BYTE *(__cdecl *)(const char *))K33LF_AT(K33LF_DVAR))("1436");
#endif
    if (!dvar || !Readable(dvar,16)) { K33LfFail(LF_PREFIX_READ);return NULL; }
    const char *value=*(const char *const *)(dvar+12);
    if (!value || !Readable(value,39)) { K33LfFail(LF_PREFIX_READ);return NULL; }
    return value;
}

static int K33LfNative(const char *prefix)
{
    if (!K33LfGuard()) return 0;
    if (!Readable(K33LF_AT(K33LF_LOG_HANDLE),4) ||
        !Readable(K33LF_AT(K33LF_LOCK),sizeof(CRITICAL_SECTION))) return K33LfFail(LF_NATIVE_READ);
    char expected[64];
    _snprintf(expected,sizeof(expected),"%s_console_mp.log",prefix);
    expected[sizeof(expected)-1]=0;
    CRITICAL_SECTION *lock=(CRITICAL_SECTION *)K33LF_AT(K33LF_LOCK);
    int result=0;
    EnterCriticalSection(lock);
    DWORD handle=*(volatile DWORD *)K33LF_AT(K33LF_LOG_HANDLE);
    /* Main-thread file allocation613C60 uses handles1..48. Check the
     * retained filename too; no other native FILE* may enter this call. */
    g_k33LfStage=LF_NATIVE_HANDLE;
    if (handle>=1 && handle<=48) {
        const BYTE *entry=(const BYTE *)K33LF_AT(K33LF_FILES+handle*K33LF_STRIDE);
        g_k33LfStage=LF_NATIVE_ENTRY;
        if (entry && Readable(entry,0x11c)) {
            g_k33LfStage=LF_NATIVE_FILENAME;
            if (!memcmp(entry+0x1c,expected,strlen(expected)+1)) {
                void *stream=*(void *const *)entry;
                g_k33LfStage=LF_NATIVE_STREAM;
                if (stream && Readable(stream,32)) {
                    g_k33LfStage=LF_NATIVE_FLUSH;
#ifdef K33LF_FIXTURE
                    result=K33LfFixtureFlush(stream)==0;
#else
                    result=((int(__cdecl *)(void *))K33LF_AT(K33LF_FLUSH))(stream)==0;
#endif
                }
            }
        }
    }
    LeaveCriticalSection(lock);
    return result;
}

static int K33LfReceiptExchange(DWORD pid,const char *nonce,const char *prefix,const char *exchange)
{
    char path[MAX_PATH],body[448];DWORD count=GetModuleFileNameA(NULL,path,sizeof(path));
    if (!count || count>=sizeof(path)) return K33LfFail(LF_RECEIPT_PATH);
    while (count && path[count-1]!='\\') count--;
    if (!count) return K33LfFail(LF_RECEIPT_PATH);
    int name=exchange ?
        _snprintf(path+count,sizeof(path)-count,"main\\%s_logflush_%lu_%s.json",prefix,(unsigned long)pid,exchange):
        _snprintf(path+count,sizeof(path)-count,"main\\%s_logflush_%lu.json",prefix,(unsigned long)pid);
    if (name<0 || (unsigned)name>=sizeof(path)-count) return K33LfFail(LF_RECEIPT_PATH);
    int length=exchange ? _snprintf(body,sizeof(body),
        "{\"version\":2,\"pid\":%lu,\"nonce\":\"%s\",\"prefix\":\"%s\",\"flushed\":true,"
        "\"c1_reference_sha256\":\"%s\",\"abi\":\"%s\",\"exchange\":\"%s\"}\n",
        (unsigned long)pid,nonce,prefix,kK33LfC1Hash,kK33LfExchangeAbi,exchange):
        _snprintf(body,sizeof(body),
        "{\"version\":1,\"pid\":%lu,\"nonce\":\"%s\",\"prefix\":\"%s\",\"flushed\":true,"
        "\"c1_reference_sha256\":\"%s\",\"abi\":\"%s\"}\n",
        (unsigned long)pid,nonce,prefix,kK33LfC1Hash,kK33LfAbi);
    if (length<0 || (unsigned)length>=sizeof(body)) return K33LfFail(LF_RECEIPT_BODY);
    HANDLE file=CreateFileA(path,GENERIC_WRITE|DELETE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) return K33LfFail(LF_RECEIPT_CREATE);
    DWORD written=0;
    int ok=K33LF_WRITE(file,body,(DWORD)length,&written,NULL) && written==(DWORD)length && FlushFileBuffers(file);
    if (!ok) {
        K33LfFail(LF_RECEIPT_WRITE);
        FILE_DISPOSITION_INFO remove;remove.DeleteFile=TRUE;
        SetFileInformationByHandle(file,FileDispositionInfo,&remove,sizeof(remove));
    }
    if (!CloseHandle(file)) return K33LfFail(LF_RECEIPT_CLOSE);
    if (ok) g_k33LfStage=LF_SUCCESS;
    return ok;
}

static int K33LfReceipt(DWORD pid,const char *nonce,const char *prefix)
{
    return K33LfReceiptExchange(pid,nonce,prefix,NULL);
}
#endif

static int K33LfExecuteExchange(const char *pid_text,const char *nonce,const char *exchange)
{
#if K33_LOCAL_TESTING
    ULONG pid=0;char expected[39];
    g_k33LfStage=LF_NOT_RUN;
    if (!pid_text || !K33LfPid(pid_text,&pid) || pid!=GetCurrentProcessId()) return K33LfFail(LF_PID);
    if (!K33LfNonce(nonce)) return K33LfFail(LF_NONCE);
    if (exchange && !K33LfNonce(exchange)) return K33LfFail(LF_EXCHANGE);
    if (!K33LfPolicy() || !K33LfPipe() || !K33LfGuard()) return 0;
    _snprintf(expected,sizeof(expected),"codol_%s",nonce);expected[sizeof(expected)-1]=0;
    const char *prefix=K33LfPrefix();
    if (!prefix) return 0;
    if (memcmp(prefix,expected,sizeof(expected))) return K33LfFail(LF_PREFIX_VALUE);
    return K33LfNative(expected) && (exchange ? K33LfReceiptExchange(pid,nonce,expected,exchange):
                                   K33LfReceipt(pid,nonce,expected));
#else
    (void)pid_text;(void)nonce;(void)exchange;
    return K33LfFail(LF_SHIPPING);
#endif
}

static int K33LfExecute(const char *pid_text,const char *nonce)
{
    return K33LfExecuteExchange(pid_text,nonce,NULL);
}

#ifndef K33LF_FIXTURE
static void __cdecl K33LfCommand(void)
{
    int ok;
    if (VoiceArgc()==3) ok=K33LfExecute(VoiceArgv(1),VoiceArgv(2));
    else if (VoiceArgc()==4) ok=K33LfExecuteExchange(VoiceArgv(1),VoiceArgv(2),VoiceArgv(3));
    else ok=K33LfFail(LF_ARGUMENTS);
    Log("k33logflush: %s (stage=%s)",ok ? "owned local log flushed":"refused",K33LfStageName());
}
static C1Cmd g_k33LfCommand={NULL,"k33logflush",(void *)K33LfCommand};
static void InstallK33LogFlush(void) { C1AddCommand(&g_k33LfCommand); }
#endif
