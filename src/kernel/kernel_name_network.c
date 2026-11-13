/* Included by kernel_name_display.c. One bounded display-data request at a
 * time. No credentials, cookies, redirect following or developer origin. */
#include <winhttp.h>
static int KndFetch(const KnpIdentity *identity,char alias[64])
{
    static const char hex[]="0123456789ABCDEF";
    char encoded[190],body[4097];wchar_t path[256];size_t used=0,body_size=0;
    const wchar_t prefix[]=L"/api/community/display-name?identity=";
    HINTERNET internet=NULL,connection=NULL,request=NULL;DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD status=0,length=sizeof(status),started=0;int ok=0;
    alias[0]=0;
    if(!memchr(identity->canonical,0,sizeof(identity->canonical)))return 0;
    for(const unsigned char *p=(const unsigned char *)identity->canonical;*p;++p) {
        unsigned c=*p;if(c<33 || c>126)return 0;
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')encoded[used++]=(char)c;
        else {encoded[used++]='%';encoded[used++]=hex[c>>4];encoded[used++]=hex[c&15];}
    }
    encoded[used]=0;
    size_t prefix_length=sizeof(prefix)/sizeof(prefix[0])-1;
    if(!identity->uid || !used || prefix_length+used+1>sizeof(path)/sizeof(path[0]))return 0;
    memcpy(path,prefix,prefix_length*sizeof(wchar_t));
    for(size_t i=0;i<=used;++i)path[prefix_length+i]=(unsigned char)encoded[i];
    internet=WinHttpOpen(L"CoDOL-NameDisplay/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!internet || !WinHttpSetTimeouts(internet,2000,2000,2000,2000))goto done;
    connection=WinHttpConnect(internet,L"codol.kewkew.org",443,0);
    request=connection?WinHttpOpenRequest(connection,L"GET",path,NULL,WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):NULL;
    started=GetTickCount();
    if(!request || !WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy)) ||
       !WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(request,NULL) ||
       !WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX) || status!=200)goto done;
    for(;;) {
        char bytes[512];DWORD got=0;
        if((DWORD)(GetTickCount()-started)>=8000 || !WinHttpReadData(request,bytes,sizeof(bytes),&got))goto done;
        if(!got)break;
        if(body_size+got>4096)goto done;
        memcpy(body+body_size,bytes,got);body_size+=got;
    }
    body[body_size]=0;ok=KnpResponse(body,body_size,identity->canonical,identity->uid,alias);
done:
    if(request)WinHttpCloseHandle(request);
    if(connection)WinHttpCloseHandle(connection);
    if(internet)WinHttpCloseHandle(internet);
    return ok;
}
static DWORD WINAPI KndWorker(LPVOID unused)
{
    (void)unused;
    for(;;) {
        KnpIdentity identity;char alias[64];int ready,ok;
        if(WaitForSingleObject(g_kndEvent,INFINITE)!=WAIT_OBJECT_0)return 0;
        AcquireSRWLockExclusive(&g_kndLock);identity=g_kndRequest;ready=g_kndPending;g_kndPending=0;
        ReleaseSRWLockExclusive(&g_kndLock);
        if(!ready)continue;
        ok=KndFetch(&identity,alias);
        AcquireSRWLockExclusive(&g_kndLock);
        KnpReply(&g_kndCache,&identity,alias,ok,GetTickCount());g_kndBusy=0;
        ReleaseSRWLockExclusive(&g_kndLock);
    }
}
