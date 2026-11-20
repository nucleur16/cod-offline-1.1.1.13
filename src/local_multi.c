/* Multi-instance: every build gives C1's single-instance mutex a per-process name (PatchMultiInstance).
 * The dwauth.ini routing below is for the local QA DLL only. */
#if defined(K33_LOCAL_TESTING) && K33_LOCAL_TESTING == 1
#include "local_multi_policy.h"

/* The current A5 CRT fopen chain calls C1's own B421D8 slot. This is not a
 * global API detour and does not intercept a system CRT's import table. */
static struct { int state;HANDLE route;FARPROC original; } g_localQaRoute;
static int LocalQaRouteState(void) {
    char text[16];DWORD error,n;unsigned long value=0;
    if(g_localQaRoute.state)return g_localQaRoute.state;
    SetLastError(ERROR_SUCCESS);n=GetEnvironmentVariableA("CODOL_LOCAL_QA_DWAUTH_HANDLE",text,sizeof(text));error=GetLastError();
    if(!n&&error==ERROR_ENVVAR_NOT_FOUND)return g_localQaRoute.state=1; // absent, legacy QA unchanged
    g_localQaRoute.state=-1;
    if(!n||n>=sizeof(text)||(n>1&&text[0]=='0'))return -1;
    for(DWORD i=0;i<n;i++){if(text[i]<'0'||text[i]>'9'||value>214748364UL)return -1;
        value=value*10+(unsigned)(text[i]-'0');if(value>0x7fffffffUL)return -1;}
    if(!value)return -1;
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    int selected=argv&&LocalMultiArgument(argc,(const wchar_t*const*)argv);if(argv)LocalFree(argv);
    wchar_t pipe[128],parent[32],ceiling[32];
    DWORD pn=GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_PIPE",pipe,128);
    DWORD hn=GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_PARENT_HANDLE",parent,32);
    DWORD cn=GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_SESSION_CEILING",ceiling,32);
    if(!selected||!pn||pn>=128||!hn||hn>=32||!cn||cn>=32)return -1;
    HANDLE source=(HANDLE)(DWORD_PTR)value;BY_HANDLE_FILE_INFORMATION info;
    if(GetFileType(source)!=FILE_TYPE_DISK||!GetFileInformationByHandle(source,&info)||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       info.nNumberOfLinks!=1||info.nFileSizeHigh||!info.nFileSizeLow||info.nFileSizeLow>256)return -1;
    HANDLE input=ReOpenFile(source,GENERIC_READ,FILE_SHARE_READ,FILE_FLAG_SEQUENTIAL_SCAN);
    if(input==INVALID_HANDLE_VALUE)return -1;
    char data[256];DWORD got=0;int ok=ReadFile(input,data,info.nFileSizeLow,&got,NULL)&&got==info.nFileSizeLow&&LocalMultiRoute(data,got);
    if(!CloseHandle(input))ok=0;
    if(!ok)return -1;
    g_localQaRoute.route=source;return g_localQaRoute.state=2;
}
static HANDLE LocalQaOpenDwauth(const char* legacyPath) {
    int state=LocalQaRouteState();
    if(state==1)return CreateFileA(legacyPath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(state!=2){SetLastError(ERROR_ACCESS_DENIED);return INVALID_HANDLE_VALUE;}
    // ReOpenFile provides a separate file position for every CRT descriptor.
    return ReOpenFile(g_localQaRoute.route,GENERIC_READ,FILE_SHARE_READ,FILE_FLAG_SEQUENTIAL_SCAN);
}
static HANDLE WINAPI LocalQaCreateFileA(LPCSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,
                                       DWORD disposition,DWORD flags,HANDLE templateFile) {
    if(path&&!lstrcmpiA(path,"dwauth.ini")&&LocalQaRouteState()!=1) {
        if(access!=GENERIC_READ||disposition!=OPEN_EXISTING||security||templateFile){SetLastError(ERROR_ACCESS_DENIED);return INVALID_HANDLE_VALUE;}
        return LocalQaOpenDwauth(path);
    }
    return ((HANDLE(WINAPI*)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE))g_localQaRoute.original)
        (path,access,share,security,disposition,flags,templateFile);
}
static FARPROC LocalQaRouteImport(const C1Import *imp,FARPROC original) {
    if(imp->slot!=0x00b421d8||lstrcmpiA(imp->dll,"KERNEL32.dll")||strcmp(imp->name,"CreateFileA"))return original;
    g_localQaRoute.original=original;
    return LocalQaRouteState()==1?original:(FARPROC)LocalQaCreateFileA;
}
#endif

/* Several game instances at once (every build). C1 creates the machine-wide mutex below at start-up and
 * quits when it already exists (the check at 0x630C3A compares the saved GetLastError with 0xB7,
 * ERROR_ALREADY_EXISTS). Its CreateMutexA call is redirected so each process gets a mutex of its own
 * name: the second instance no longer sees ERROR_ALREADY_EXISTS. The lobby picks a free port per
 * instance; give each instance its own profile with +set dw_username <name>. */
static const char kGameMutex[]="Global\\{{5BD75B76-601A-4b20-80C8-D2317C94BB30}}";

static HANDLE WINAPI LocalGameMutex(LPSECURITY_ATTRIBUTES attributes,BOOL owner,LPCSTR name)
{
    if(name && strcmp(name,kGameMutex)==0) {
        char local[80];
        _snprintf(local,sizeof(local),"Local\\K33Instance-%lu",(unsigned long)GetCurrentProcessId());
        local[sizeof(local)-1]=0;
        return CreateMutexA(attributes,owner,local);
    }
    return CreateMutexA(attributes,owner,name);
}

static void PatchMultiInstance(void)
{
    static const BYTE branch[]={0x81,0x3d,0xd8,0x07,0xf9,0x07,0xb7,0,0,0,
        0x8b,0x45,0x08,0x53,0x56,0x89,0x44,0x24,0x10,0x8b,0x45,0x10,0x57,0x75,0x45};
    static const BYTE initializer[]={0x68,0xcc,0x14,0xc7,0,0x6a,0,0x6a,0,0xff,0x15,0x3c,0x22,0xb4,0};
    const BYTE *check=(const BYTE *)(DWORD_PTR)0x00630c3a;
    const BYTE *init=(const BYTE *)(DWORD_PTR)0x00b32060;
    const char *name=(const char *)(DWORD_PTR)0x00c714cc;
    if(!Readable(check,sizeof(branch)) || memcmp(check,branch,sizeof(branch)) ||
       !Readable(init,sizeof(initializer)) || memcmp(init,initializer,sizeof(initializer)) ||
       !Readable(name,sizeof(kGameMutex)) || memcmp(name,kGameMutex,sizeof(kGameMutex))) {
        Log("multi-instance: native single-instance code differs; only one instance can run");return;
    }
    if(RedirectIndirectCall(0x00b32069,0x00b4223c,(void *)LocalGameMutex))
        Log("multi-instance: per-process game mutex; more instances can run");
}
