/* Compile the actual QA window policy against owned, non-UI API doubles. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#define CODOL_LOCAL_TESTING 1
typedef struct { DWORD slot; const char *dll; const char *name; } C1Import;
static const char *mode, *property;
static const char good[]="CODOL_QA_BG_0123456789abcdef0123456789abcdef";
static HWND owned=(HWND)11, focused=(HWND)22;
static HANDLE placement;
static int last_command, last_x, last_y;
static UINT last_flags;
static unsigned show_calls, position_calls;
static DWORD WINAPI environment(LPCSTR name, LPSTR value, DWORD size)
{
    const char *text=!strcmp(name,"CODOL_QA_BACKGROUND")?mode:property;
    if (!text) return 0;
    DWORD n=(DWORD)strlen(text);
    if (n<size) memcpy(value,text,n+1);
    return n<size?n:n+1;
}
static DWORD WINAPI fixture_owner(HWND window, LPDWORD process)
{
    *process=window==owned?101:202;
    return window?1:0;
}
static DWORD WINAPI current_pid(void){return 101;}
static HWND WINAPI focus(void){return focused;}
static HANDLE WINAPI get_property(HWND window,LPCSTR name)
{
    return window==owned&&!strcmp(name,good)?placement:NULL;
}
static BOOL WINAPI show(HWND window,int command)
{
    if(window!=owned)return FALSE;
    show_calls++;last_command=command;return TRUE;
}
static BOOL WINAPI position(HWND window,HWND after,int x,int y,int w,int h,UINT flags)
{
    (void)after;(void)w;(void)h;
    if(window!=owned)return FALSE;
    position_calls++;last_x=x;last_y=y;last_flags=flags;return TRUE;
}
#define GetEnvironmentVariableA environment
#define GetWindowThreadProcessId fixture_owner
#define GetCurrentProcessId current_pid
#define GetFocus focus
#define GetPropA get_property
#include "../src/qa_background_window.c"
#define CHECK(test) do { if(!(test)){printf("FAIL line %d: %s\n",__LINE__,#test);return 1;} }while(0)
static void reset(const char *enabled,const char *name)
{
    mode=enabled;property=name;g_qbgChecked=g_qbgEnabled=FALSE;
    memset(g_qbgProperty,0,sizeof(g_qbgProperty));memset((void *)QbgTelemetry,0,sizeof(QbgTelemetry));
    g_qbgImports=0;g_qbgWindow=NULL;placement=NULL;show_calls=position_calls=0;
}
static void install(void)
{
    C1Import entry={0xB423A0,"USER32.dll","ShowWindow"};
    QbgImport(&entry,(FARPROC)(void *)show);
    entry.slot=0xB423FC;entry.name="SetFocus";QbgImport(&entry,(FARPROC)(void *)focus);
    entry.slot=0xB42400;entry.name="SetForegroundWindow";QbgImport(&entry,(FARPROC)(void *)show);
    entry.slot=0xB42490;entry.name="SetWindowPos";QbgImport(&entry,(FARPROC)(void *)position);
}
int main(void)
{
    C1Import entry={0xB423A0,"USER32.dll","ShowWindow"};
    FARPROC original=(FARPROC)(void *)show;
    reset(NULL,NULL);CHECK(QbgImport(&entry,original)==original&&QbgStartupReady(FALSE));
    for(unsigned i=0;i<3;i++) {
        reset("1",i==0?NULL:i==1?"bad":"CODOL_QA_BG_0123456789abcdef0123456789abcdeG");
        install();CHECK(!QbgStartupReady(TRUE));
    }
    reset("1",good);
    entry.slot=0xB423A4;CHECK(QbgImport(&entry,original)==original&&!g_qbgImports);
    entry.slot=0xB423A0;entry.dll="OTHER.dll";CHECK(QbgImport(&entry,original)==original&&!g_qbgImports);
    install();CHECK(!QbgStartupReady(FALSE)&&QbgStartupReady(TRUE)&&QbgTelemetry[8]==15);
    DWORD extended=WS_EX_TOPMOST;int x=100,y=200;
    QbgPrepareWindow(&extended,&x,&y);QbgCreatedWindow(owned);
    CHECK((extended&WS_EX_NOACTIVATE)&&(extended&WS_EX_APPWINDOW)&&!(extended&WS_EX_TOPMOST)&&x==-32000&&y==-32000);
    CHECK(!QbgSetForegroundWindow(owned)&&QbgSetFocus(owned)==focused);
    CHECK(QbgTelemetry[4]==1&&QbgTelemetry[5]==1);
    CHECK(QbgShowWindow(owned,SW_SHOW)&&last_command==SW_SHOWNOACTIVATE);
    CHECK(QbgShowWindow(owned,SW_HIDE)&&last_command==SW_HIDE);
    CHECK(!QbgShowWindow((HWND)99,SW_SHOW)&&show_calls==2);
    CHECK(QbgSetWindowPos(owned,HWND_TOPMOST,100,200,1280,720,SWP_NOMOVE));
    CHECK(last_x==-32000&&last_y==-32000&&!(last_flags&SWP_NOMOVE));
    CHECK((last_flags&(SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER))==(SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER));
    placement=(HANDLE)1;
    CHECK(QbgSetWindowPos(owned,HWND_TOPMOST,100,200,1280,720,0)&&last_x==100&&last_y==200);
    CHECK(!QbgSetWindowPos((HWND)99,NULL,0,0,1,1,0)&&position_calls==2);
    /* A replacement window has no placement property until the launcher verifies it. */
    owned=(HWND)33;placement=NULL;QbgPrepareWindow(&extended,&x,&y);QbgCreatedWindow(owned);
    CHECK(QbgSetWindowPos(owned,NULL,100,200,1280,720,0)&&last_x==-32000);
    CHECK(QbgTelemetry[2]==2&&QbgTelemetry[7]==2);
    puts("PASS: actual QA policy; startup guards, exact imports, no activation, placement handshake and replacement window");
    return 0;
}
