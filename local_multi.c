/* Production retains the unmodified C1 single-instance branch and mutex.
 * Only the explicitly compiled local QA DLL may give the exact game mutex
 * a process-local name, after both command and loopback routing guards pass. */
#if defined(CODOL_LOCAL_TESTING) && CODOL_LOCAL_TESTING == 1
#include "local_multi_policy.h"

static const char kCodolGameMutex[]="Global\\{{5BD75B76-601A-4b20-80C8-D2317C94BB30}}";

static HANDLE WINAPI LocalGameMutex(LPSECURITY_ATTRIBUTES attributes,BOOL owner,LPCSTR name)
{
    if(name && strcmp(name,kCodolGameMutex)==0) {
        char local[80];
        _snprintf(local,sizeof(local),"Local\\CoDOLClassicTest-%lu",(unsigned long)GetCurrentProcessId());
        local[sizeof(local)-1]=0;
        return CreateMutexA(attributes,owner,local);
    }
    return CreateMutexA(attributes,owner,name);
}

static BOOL LocalMultiEnabled(void)
{
    int argc=0;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    int enabled=argv && CodolLocalMultiArgument(argc,(const wchar_t *const *)argv);
    if(argv)LocalFree(argv);
    if(!enabled)return FALSE;
    HANDLE input=CreateFileA("dwauth.ini",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(input==INVALID_HANDLE_VALUE)return FALSE;
    char data[257];DWORD size=GetFileSize(input,NULL),got=0;
    BOOL ok=size>0 && size<=256 && ReadFile(input,data,size,&got,NULL) && got==size;
    CloseHandle(input);
    return ok && CodolLocalMultiRoute(data,size);
}
#endif

static void PatchMultiInstance(void)
{
#if defined(CODOL_LOCAL_TESTING) && CODOL_LOCAL_TESTING == 1
    static const BYTE branch[]={0x81,0x3d,0xd8,0x07,0xf9,0x07,0xb7,0,0,0,
        0x8b,0x45,0x08,0x53,0x56,0x89,0x44,0x24,0x10,0x8b,0x45,0x10,0x57,0x75,0x45};
    static const BYTE initializer[]={0x68,0xcc,0x14,0xc7,0,0x6a,0,0x6a,0,0xff,0x15,0x3c,0x22,0xb4,0};
    const BYTE *check=(const BYTE *)(DWORD_PTR)0x00630c3a;
    const BYTE *init=(const BYTE *)(DWORD_PTR)0x00b32060;
    const char *name=(const char *)(DWORD_PTR)0x00c714cc;
    if(!LocalMultiEnabled()) { Log("local multi-client: disabled by local-only guards");return; }
    if(!Readable(check,sizeof(branch)) || memcmp(check,branch,sizeof(branch)) ||
       !Readable(init,sizeof(initializer)) || memcmp(init,initializer,sizeof(initializer)) ||
       !Readable(name,sizeof(kCodolGameMutex)) || memcmp(name,kCodolGameMutex,sizeof(kCodolGameMutex))) {
        Log("local multi-client: native guard differs; single-client unchanged");return;
    }
    if(RedirectIndirectCall(0x00b32069,0x00b4223c,(void *)LocalGameMutex))
        Log("local multi-client: loopback QA process mutex; native single-client branch retained");
#else
    Log("single-client: production native mutex and branch retained");
#endif
}
