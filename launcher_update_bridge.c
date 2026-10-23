/* One-time upgrade from installed launchers that cannot replace themselves.
 * The ordinary HTTPS manifest delivers a staged launcher; its exact release
 * hash is compiled into this bridge. No download, elevation or firewall rule.
 * Called before game startup, outside DllMain and the loader lock. */
#include <wincrypt.h>
#include <tlhelp32.h>
#ifndef CODOL_LAUNCHER_BRIDGE_HOST_SHA256
#define CODOL_LAUNCHER_BRIDGE_HOST_SHA256 ""
#endif

static int LauncherBridgeHash(const wchar_t *path,char out[65])
{
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD got=0,length=32;
    BYTE bytes[32768],digest[32];int ok=0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)||
       !CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))goto done;
    for(;;){
        if(!ReadFile(file,bytes,sizeof(bytes),&got,NULL))goto done;
        if(!got)break;
        if(!CryptHashData(hash,bytes,got,0))goto done;
    }
    if(!CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)||length!=32)goto done;
    for(unsigned i=0;i<32;i++)sprintf(out+2*i,"%02x",digest[i]);
    out[64]=0;ok=1;
done:
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return ok;
}

static DWORD LauncherBridgeParent(void)
{
    HANDLE list=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(list==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W entry;memset(&entry,0,sizeof(entry));entry.dwSize=sizeof(entry);
    DWORD parent=0;
    if(Process32FirstW(list,&entry))do{
        if(entry.th32ProcessID==GetCurrentProcessId()){parent=entry.th32ParentProcessID;break;}
    }while(Process32NextW(list,&entry));
    CloseHandle(list);return parent;
}

static int TryLauncherUpgrade(void)
{
    const char *expected=CODOL_LAUNCHER_BRIDGE_HOST_SHA256;
    if(strlen(expected)!=64)return 0; /* An unpinned development build is inert. */
    wchar_t root[MAX_PATH],staged[MAX_PATH],target[MAX_PATH],receipt[MAX_PATH];
    DWORD n=GetModuleFileNameW(NULL,root,MAX_PATH);
    if(!n||n>=MAX_PATH)return 0;
    wchar_t *slash=wcsrchr(root,L'\\');if(!slash)return 0;*slash=0;
    if(wcslen(root)+90>=MAX_PATH)return 0;
    swprintf(staged,MAX_PATH,L"%ls\\TCLS\\LauncherHost.pending.exe",root);
    DWORD attributes=GetFileAttributesW(staged);
    if(attributes==INVALID_FILE_ATTRIBUTES)return 0;
    if(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return 0;
    DWORD parentPid=LauncherBridgeParent();if(!parentPid)return 0;
    HANDLE parent=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,parentPid);
    if(!parent)return 0;
    n=MAX_PATH;
    if(!QueryFullProcessImageNameW(parent,0,target,&n)){CloseHandle(parent);return 0;}
    wchar_t allowed[MAX_PATH];
    swprintf(allowed,MAX_PATH,L"%ls\\TCLS\\LauncherHost.exe",root);
    int valid=!_wcsicmp(target,allowed);
    swprintf(allowed,MAX_PATH,L"%ls\\TCLS\\CODOLLauncher.exe",root);
    valid=valid||!_wcsicmp(target,allowed);
    FILETIME creation,exit,kernel,user;
    if(!valid||!GetProcessTimes(parent,&creation,&exit,&kernel,&user)){CloseHandle(parent);return 0;}
    ULARGE_INTEGER created;created.LowPart=creation.dwLowDateTime;created.HighPart=creation.dwHighDateTime;
    char current[65],incoming[65];
    if(!LauncherBridgeHash(target,current)){CloseHandle(parent);return 0;}
    swprintf(receipt,MAX_PATH,L"%ls.update-v1.sha256",target);
    char recorded[67]={0};
    FILE *record=_wfopen(receipt,L"rb");
    if(record){size_t count=fread(recorded,1,sizeof(recorded),record);fclose(record);
        if(count==65&&recorded[64]=='\n'&&!memcmp(recorded,current,64)){CloseHandle(parent);return 0;}}
    if(!strcmp(current,expected)){CloseHandle(parent);return 0;}
    if(!LauncherBridgeHash(staged,incoming)||strcmp(incoming,expected)){
        Log("launcher bridge: staged release hash differs; unchanged");CloseHandle(parent);return 0;
    }
    wchar_t previous[65],next[65];
    for(unsigned i=0;i<65;i++){previous[i]=(wchar_t)current[i];next[i]=(wchar_t)incoming[i];}
    const wchar_t *name=wcsrchr(target,L'\\');if(!name){CloseHandle(parent);return 0;}name++;
    wchar_t command[1024];
    swprintf(command,1024,L"\"%ls\" --codol-apply-launcher-update %ls %lu %llu %ls %ls",
             staged,name,(unsigned long)parentPid,(unsigned long long)created.QuadPart,previous,next);
    STARTUPINFOW si;PROCESS_INFORMATION child;memset(&si,0,sizeof(si));memset(&child,0,sizeof(child));si.cb=sizeof(si);
    BOOL started=CreateProcessW(staged,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,root,&si,&child);
    CloseHandle(parent);
    if(!started){Log("launcher bridge: could not start staged updater (%lu)",(unsigned long)GetLastError());return 0;}
    CloseHandle(child.hThread);CloseHandle(child.hProcess);
    Log("launcher bridge: verified replacement handed off before game startup");
    return 1;
}
