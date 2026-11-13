/* Preserve the retired proxy's data-only display-mode fallback. Called only
 * on the native main thread; never poll a file per frame or change game models. */
#include <stdint.h>
#include <wchar.h>
static wchar_t g_kvPath[MAX_PATH];
static int g_kvLoaded,g_kvHaveSaved,g_kvSaved;
static int KvPath(void)
{
    if(g_kvPath[0])return 1;
    DWORD length=GetModuleFileNameW(NULL,g_kvPath,MAX_PATH);
    if(!length || length>=MAX_PATH-40){g_kvPath[0]=0;return 0;}
    wchar_t *slash=wcsrchr(g_kvPath,L'\\');
    if(!slash){g_kvPath[0]=0;return 0;}
    lstrcpyW(slash+1,L"codol_video_mode.cfg");return 1;
}
static int KvLoad(int fallback)
{
    if(!g_kvLoaded) {
        char value=0;DWORD count=0;g_kvLoaded=1;
        if(KvPath()) {
            HANDLE file=CreateFileW(g_kvPath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                    NULL,OPEN_EXISTING,0,NULL);
            if(file!=INVALID_HANDLE_VALUE) {
                int valid=ReadFile(file,&value,1,&count,NULL) && count==1 && (value=='0'||value=='1');
                CloseHandle(file);
                if(valid){g_kvHaveSaved=1;g_kvSaved=value=='1';}
            }
        }
    }
    return g_kvHaveSaved?g_kvSaved:fallback;
}
static void KvSave(int enabled)
{
    static DWORD retry;static int failed;
    wchar_t temporary[MAX_PATH];DWORD count=0;char value[2]={enabled?'1':'0','\n'};int written;
    if((g_kvHaveSaved && g_kvSaved==enabled) || !KvPath())return;
    DWORD now=GetTickCount();
    if(failed && (int32_t)(now-retry)<0)return;
    failed=1;retry=now+1000;
    /* Per-process temp files prevent multiple clients from truncating another
     * client's pending atomic write. Final rename still has last-writer wins. */
    lstrcpyW(temporary,g_kvPath);lstrcatW(temporary,L".tmp.");
    wchar_t *suffix=temporary+lstrlenW(temporary);DWORD pid=GetCurrentProcessId();
    for(int i=7;i>=0;--i)*suffix++=L"0123456789abcdef"[(pid>>(i*4))&15];
    *suffix=0;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    written=WriteFile(file,value,sizeof(value),&count,NULL) && count==sizeof(value) && FlushFileBuffers(file);
    CloseHandle(file);
    if(written && MoveFileExW(temporary,g_kvPath,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        {g_kvHaveSaved=1;g_kvSaved=enabled;failed=0;}
    else DeleteFileW(temporary);
}
