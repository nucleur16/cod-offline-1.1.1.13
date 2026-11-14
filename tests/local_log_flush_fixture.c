/* Real Windows policy/pipe/receipt I/O, exact C1 byte guards, injected native
 * stream/lookup calls only. Does not load/execute C1 or bind a game socket. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int Readable(const void *p,unsigned n) { return p!=NULL && n>0; }
#if CODOL_LOCAL_TESTING
static BYTE code_flush[14],code_caller[37],code_dvar[12],code_prefix[12],code_lock[19];
static FARPROC lock_import;
static BYTE fake_dvar[16],entry[0x11c],stream[64];
static DWORD log_handle=1;
static CRITICAL_SECTION log_lock;
static int flush_calls,flush_error,partial_write,sharing_checks;
static char prefix[64];

static const BYTE *K33LfFixtureDvar(void) { return fake_dvar; }
static int __cdecl K33LfFixtureFlush(void *file)
{
    assert(file==stream && log_lock.RecursionCount==1);
    flush_calls++;
    return flush_error ? -1:0;
}
static BOOL WINAPI fixture_write(HANDLE file,LPCVOID data,DWORD size,LPDWORD written,LPOVERLAPPED overlap)
{
    char path[512];DWORD n=GetFinalPathNameByHandleA(file,path,sizeof(path),FILE_NAME_NORMALIZED);
    assert(n>0 && n<sizeof(path));
    HANDLE reader=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                              NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(reader==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION);sharing_checks++;
    return WriteFile(file,data,partial_write ? size/2:size,written,overlap);
}
static void *fixture_at(DWORD va)
{
    switch(va) {
    case 0x00ADB1F7u:return code_flush;
    case 0x0062C0D3u:return code_caller;
    case 0x006232C0u:return code_dvar;
    case 0x00593F2Cu:return code_prefix;
    case 0x00594020u:return code_lock;
    case 0x00B421ECu:case 0x09A1D2FCu:return &lock_import;
    case 0x02574660u:return &log_handle;
    case 0x02B62AB0u:return &log_lock;
    case 0x02AAA43Cu:return entry;
    default:return NULL;
    }
}
#define K33LF_AT(va) fixture_at(va)
#define K33LF_WRITE fixture_write
#endif
#if CODOL_LOCAL_TESTING
/* Same absent-selection file leaf as before. An explicit route belongs to the
 * separate inherited-route fixture and may never silently use this fallback. */
static HANDLE LocalQaOpenDwauth(const char *path)
{
    char value[16];SetLastError(ERROR_SUCCESS);
    DWORD n=GetEnvironmentVariableA("CODOL_LOCAL_QA_DWAUTH_HANDLE",value,sizeof(value));
    assert(!n && GetLastError()==ERROR_ENVVAR_NOT_FOUND);
    return CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
}
#endif
#define K33LF_FIXTURE 1
#include "../src/local_log_flush.c"

#if CODOL_LOCAL_TESTING
static void read_code(HANDLE file,DWORD va,void *out,DWORD bytes)
{
    BYTE header[4096];DWORD got=0;
    SetFilePointer(file,0,NULL,FILE_BEGIN);
    assert(ReadFile(file,header,sizeof(header),&got,NULL) && got==sizeof(header));
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)header;
    IMAGE_NT_HEADERS32 *nt=(IMAGE_NT_HEADERS32 *)(header+dos->e_lfanew);
    assert(dos->e_magic==IMAGE_DOS_SIGNATURE && nt->Signature==IMAGE_NT_SIGNATURE && nt->OptionalHeader.ImageBase==0x400000);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    DWORD rva=va-0x400000;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        if(rva>=s->VirtualAddress && rva+bytes<=s->VirtualAddress+s->SizeOfRawData) {
            SetFilePointer(file,s->PointerToRawData+rva-s->VirtualAddress,NULL,FILE_BEGIN);
            assert(ReadFile(file,out,bytes,&got,NULL) && got==bytes);return;
        }
    }
    assert(!"fixture code address missing");
}
static void load_code(const char *path)
{
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(file!=INVALID_HANDLE_VALUE);
    read_code(file,0xADB1F7,code_flush,sizeof(code_flush));read_code(file,0x62C0D3,code_caller,sizeof(code_caller));
    read_code(file,0x6232C0,code_dvar,sizeof(code_dvar));read_code(file,0x593F2C,code_prefix,sizeof(code_prefix));
    read_code(file,0x594020,code_lock,sizeof(code_lock));CloseHandle(file);
}

static size_t read_receipt(const char *path,char *body,size_t capacity)
{
    FILE *file=fopen(path,"rb");assert(file);size_t size=fread(body,1,capacity-1,file);body[size]=0;fclose(file);return size;
}

static void rearm_pipe(HANDLE pipe,OVERLAPPED *connection)
{
    DWORD ignored;assert(GetOverlappedResult(pipe,connection,&ignored,TRUE));
    assert(DisconnectNamedPipe(pipe));HANDLE event=connection->hEvent;
    assert(ResetEvent(event));ZeroMemory(connection,sizeof(*connection));connection->hEvent=event;
    assert(!ConnectNamedPipe(pipe,connection) && GetLastError()==ERROR_IO_PENDING);
}

static void exchange_tests(const char *which,const char *pid,const char *nonce,HANDLE pipe,OVERLAPPED *connection)
{
    const char *exchange="11111111111111111111111111111111";
    const char *second="22222222222222222222222222222222";
    char path[256],body[448],saved[448];
    _snprintf(path,sizeof(path),"main\\%s_logflush_%s_%s.json",prefix,pid,exchange);
    if(!strcmp(which,"exchangebad"))exchange="1111111111111111111111111111111F";
    if(!strcmp(which,"exchangeempty"))exchange="";
    if(!strcmp(which,"exchangeshort"))exchange="1111111111111111111111111111111";
    if(!strcmp(which,"exchangelong"))exchange="111111111111111111111111111111111";
    if(!strcmp(which,"exchangepath"))exchange="../../../../../../../../x.json";
    if(!strcmp(which,"exchangewrongserver"))SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID","1");
    if(!strcmp(which,"exchangeguard"))code_caller[0]^=1;
    if(!strcmp(which,"exchangepartial"))partial_write=1;
    if(!strcmp(which,"exchangepreexisting")) {
        FILE *file=fopen(path,"wb");assert(file);assert(fwrite("unchanged",1,9,file)==9);fclose(file);
    }
    int good=!strcmp(which,"exchangegood") || !strcmp(which,"exchangerepeat");
    assert(K33LfExecuteExchange(pid,nonce,exchange)==good);
    assert(log_lock.RecursionCount==0);
    if(good) {
        assert(g_k33LfStage==LF_SUCCESS && flush_calls==1 && sharing_checks==1);
        size_t size=read_receipt(path,saved,sizeof(saved));assert(size>100 && size<sizeof(saved)-1);
        assert(strstr(saved,"\"version\":2") && strstr(saved,kK33LfExchangeAbi) && strstr(saved,exchange));
        rearm_pipe(pipe,connection);
        if(!strcmp(which,"exchangerepeat")) {
            assert(K33LfExecuteExchange(pid,nonce,second));assert(flush_calls==2 && sharing_checks==2);
            rearm_pipe(pipe,connection);
            assert(!K33LfExecuteExchange(pid,nonce,exchange) && g_k33LfStage==LF_RECEIPT_CREATE);
            assert(flush_calls==3 && sharing_checks==2);
            assert(read_receipt(path,body,sizeof(body))==size && !memcmp(body,saved,size));
        } else {
            assert(K33LfExecute(pid,nonce)); /* v1 startup can coexist; prefix unchanged. */
            assert(flush_calls==2 && sharing_checks==2);
        }
    } else if(!strcmp(which,"exchangepreexisting")) {
        assert(g_k33LfStage==LF_RECEIPT_CREATE && flush_calls==1 && sharing_checks==0);
        assert(read_receipt(path,body,sizeof(body))==9 && !strcmp(body,"unchanged"));
    } else if(!strcmp(which,"exchangepartial")) {
        assert(g_k33LfStage==LF_RECEIPT_WRITE && flush_calls==1 && sharing_checks==1);
        assert(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES);
    } else {
        K33LfStage expected=!strcmp(which,"exchangewrongserver") ? LF_PIPE_OWNER:
                            !strcmp(which,"exchangeguard") ? LF_GUARD_CALLER:LF_EXCHANGE;
        assert(g_k33LfStage==expected && flush_calls==0 && sharing_checks==0);
    }
    printf("PASS %s\n",which);
}
#endif

int main(int argc,char **argv)
{
    assert(argc>=3);
    const char *which=argv[1],*nonce="0123456789abcdef0123456789abcdef";
    char pid[16];_snprintf(pid,sizeof(pid),"%lu",(unsigned long)GetCurrentProcessId());
#if !CODOL_LOCAL_TESTING
    (void)which;
    assert(!K33LfExecute(pid,nonce) && g_k33LfStage==LF_SHIPPING);
    assert(!K33LfExecuteExchange(pid,nonce,"11111111111111111111111111111111") && g_k33LfStage==LF_SHIPPING);
    assert(!strcmp(K33LfStageName(),"shipping"));puts("PASS shipping refusal");return 0;
#else
    load_code(argv[2]);InitializeCriticalSection(&log_lock);
    lock_import=GetProcAddress(GetModuleHandleA("kernel32.dll"),"EnterCriticalSection");assert(lock_import);
    strcpy(prefix,"codol_");strcat(prefix,nonce);*(char **)(fake_dvar+12)=prefix;
    *(void **)entry=stream;_snprintf((char *)entry+0x1c,256,"%s_console_mp.log",prefix);
    char pipe_name[128];_snprintf(pipe_name,sizeof(pipe_name),"\\\\.\\pipe\\CoDOLVerifiedScripts-%032lx",(unsigned long)GetCurrentProcessId());
    SetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE",NULL);
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED","1");
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",pipe_name);
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID",pid);
    HANDLE pipe=CreateNamedPipeA(pipe_name,PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE|PIPE_WAIT,1,256,256,0,NULL);
    assert(pipe!=INVALID_HANDLE_VALUE);
    OVERLAPPED connection;ZeroMemory(&connection,sizeof(connection));connection.hEvent=CreateEventA(NULL,TRUE,FALSE,NULL);
    assert(!ConnectNamedPipe(pipe,&connection) && GetLastError()==ERROR_IO_PENDING);
    if(!strncmp(which,"exchange",8)) { exchange_tests(which,pid,nonce,pipe,&connection);goto cleanup; }
    if(!strcmp(which,"identitygood")) {
        SetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE",pipe_name);
        SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE","\\\\.\\pipe\\CoDOLVerifiedScripts-00000000000000000000000000000000");
    }
    if(!strcmp(which,"identityempty"))SetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE","");
    if(!strcmp(which,"identitybad"))SetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE","\\\\.\\pipe\\other");
    if(!strcmp(which,"identitymissing"))SetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE","\\\\.\\pipe\\CoDOLVerifiedScripts-00000000000000000000000000000000");
    if(!strcmp(which,"identitywrongserver")) {
        SetEnvironmentVariableA("CODOL_LOCAL_LOG_IDENTITY_PIPE",pipe_name);
        SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID","1");
    }
    if(!strcmp(which,"badpid"))strcpy(pid,"1");
    if(!strcmp(which,"leadingpid"))strcpy(pid,"01");
    if(!strcmp(which,"overflowpid"))strcpy(pid,"4294967296");
    if(!strcmp(which,"badnonce"))nonce="0123456789abcdef0123456789abcdeF";
    if(!strcmp(which,"longnonce"))nonce="0123456789abcdef0123456789abcdef0";
    if(!strcmp(which,"pathnonce"))nonce="../../../../../../../../x.json";
    if(!strcmp(which,"prefix"))prefix[6]='f';
    if(!strcmp(which,"flagoff"))SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPTS_REQUIRED","0");
    if(!strcmp(which,"badpipe"))SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE","\\\\.\\pipe\\other");
    if(!strcmp(which,"wrongserver"))SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID","1");
    if(!strcmp(which,"missingpipe"))SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",NULL);
    if(!strcmp(which,"guardflush"))code_flush[0]^=1;
    if(!strcmp(which,"guardcaller"))code_caller[0]^=1;
    if(!strcmp(which,"guarddvar"))code_dvar[0]^=1;
    if(!strcmp(which,"guardprefix"))code_prefix[0]^=1;
    if(!strcmp(which,"guardlock"))code_lock[0]^=1;
    if(!strcmp(which,"runtimeiat")) { DWORD slot=0x00B421ECu;memcpy(code_lock+7,&slot,4); }
    if(!strcmp(which,"arbitraryiat")) { DWORD slot=0x00B42110u;memcpy(code_lock+7,&slot,4); }
    if(!strcmp(which,"wrongimport"))lock_import=GetProcAddress(GetModuleHandleA("kernel32.dll"),"LeaveCriticalSection");
    if(!strcmp(which,"nullimport"))lock_import=NULL;
    if(!strcmp(which,"guardlocktail"))code_lock[18]^=1;
    if(!strcmp(which,"badhandle"))log_handle=49;
    if(!strcmp(which,"wrongfile"))entry[0x1c]='x';
    if(!strcmp(which,"nullstream"))*(void **)entry=NULL;
    if(!strcmp(which,"flusherror"))flush_error=1;
    if(!strcmp(which,"receiptpartial"))partial_write=1;
    int good=!strcmp(which,"good") || !strcmp(which,"runtimeiat") || !strcmp(which,"identitygood"),ok=K33LfExecute(pid,nonce);
    assert(ok==good);
    assert(flush_calls==(good || !strcmp(which,"flusherror") || !strcmp(which,"receiptpartial") ? 1:0));
    assert(log_lock.RecursionCount==0);
    struct {const char *test;K33LfStage stage;} stages[]={
        {"good",LF_SUCCESS},{"runtimeiat",LF_SUCCESS},{"arbitraryiat",LF_GUARD_LOCK},
        {"identitygood",LF_SUCCESS},{"identityempty",LF_PIPE_ENV},{"identitybad",LF_PIPE_ENV},
        {"identitymissing",LF_PIPE_OPEN},{"identitywrongserver",LF_PIPE_OWNER},
        {"wrongimport",LF_GUARD_LOCK_IMPORT},{"nullimport",LF_GUARD_LOCK_IMPORT},
        {"guardlocktail",LF_GUARD_LOCK},{"badpid",LF_PID},{"leadingpid",LF_PID},{"overflowpid",LF_PID},
        {"badnonce",LF_NONCE},{"longnonce",LF_NONCE},{"pathnonce",LF_NONCE},{"prefix",LF_PREFIX_VALUE},
        {"flagoff",LF_VERIFIED_FLAG},{"badpipe",LF_PIPE_ENV},{"wrongserver",LF_PIPE_OWNER},
        {"missingpipe",LF_PIPE_ENV},{"guardflush",LF_GUARD_FLUSH},{"guardcaller",LF_GUARD_CALLER},
        {"guarddvar",LF_GUARD_DVAR},{"guardprefix",LF_GUARD_PREFIX},{"guardlock",LF_GUARD_LOCK},
        {"badhandle",LF_NATIVE_HANDLE},{"wrongfile",LF_NATIVE_FILENAME},{"nullstream",LF_NATIVE_STREAM},
        {"flusherror",LF_NATIVE_FLUSH},{"multioff",LF_MULTI},{"wrongroute",LF_POLICY_ROUTE},
        {"mixedroute",LF_POLICY_ROUTE},{"receiptpartial",LF_RECEIPT_WRITE}};
    int matched=0;
    for(unsigned i=0;i<sizeof(stages)/sizeof(stages[0]);i++)if(!strcmp(which,stages[i].test)) {
        assert(g_k33LfStage==stages[i].stage);matched=1;
    }
    assert(matched && strcmp(K33LfStageName(),"invalid-stage"));
    printf("stage=%s\n",K33LfStageName());
    if(good) {
        /* Receipt creation is exclusive and never overwrites an earlier result. */
        assert(!K33LfReceipt(GetCurrentProcessId(),nonce,prefix));
        char path[192],body[448];_snprintf(path,sizeof(path),"main\\%s_logflush_%s.json",prefix,pid);
        FILE *file=fopen(path,"rb");assert(file);
        size_t size=fread(body,1,sizeof(body)-1,file);body[size]=0;fclose(file);
        assert(size>100 && size<sizeof(body)-1 && strstr(body,"\"flushed\":true") && strstr(body,kK33LfC1Hash));
        puts(body);
    }
cleanup:
    CancelIoEx(pipe,&connection);DWORD ignored;GetOverlappedResult(pipe,&connection,&ignored,TRUE);
    DisconnectNamedPipe(pipe);CloseHandle(connection.hEvent);CloseHandle(pipe);DeleteCriticalSection(&log_lock);
    if(strncmp(which,"exchange",8))printf("PASS %s\n",which);
    return 0;
#endif
}
