/* Included by kernel_name_display.c. Exact retained name-only render paths.
 * Never rewrite canonical model rows, account names, UID caches or chat text. */
static uint64_t KndLocalUID(void)
{
    uint64_t uid=0;BYTE ready=0;
    if(!KndRead(0xcea9f8u,&uid,8))return 0;
    if(uid)return uid;
    return KndRead(0x6c44f27u,&ready,1) && ready && KndRead(0x6c44f58u,&uid,8)?uid:0;
}
static __attribute__((used,noinline,force_align_arg_pointer)) const char * __cdecl KndSelectAlias(const char *original)
{
    char canonical[128],alias[64];uint64_t uid=KndLocalUID();KndBuffers *buffers;
    if(!uid || !KndCString(original,canonical,sizeof(canonical)))return original;
    AcquireSRWLockShared(&g_kndLock);int ok=KnpAlias(&g_kndCache,canonical,uid,GetTickCount(),alias);
    ReleaseSRWLockShared(&g_kndLock);
    if(!ok || !(buffers=KndThreadBuffers()))return original;
    memcpy(buffers->alias,alias,strlen(alias)+1);return buffers->alias;
}
static __attribute__((used,noinline,force_align_arg_pointer)) const char * __cdecl KndSelectMember(const char *original,const void *member)
{
    uint64_t local=KndLocalUID(),row=0;uint32_t dvar=0,text=0;
    char raw[128],canonical[128],alias[64],combined[64];size_t prefix=0;KndBuffers *buffers;
    if(!local || !KndRead((uint32_t)(uintptr_t)member,&row,8) || row!=local ||
       !KndRead(0x6c26318u,&dvar,4) || !dvar || !KndRead(dvar+12,&text,4) ||
       !KndCString((const char *)(uintptr_t)text,canonical,sizeof(canonical)) ||
       !KndCString(original,raw,sizeof(raw)))return original;
    if(strcmp(raw,canonical)) {
        char *end=raw[0]=='['?strstr(raw,"] "):NULL;
        if(!end || strcmp(end+2,canonical))return original;
        prefix=(size_t)(end+2-raw);
    }
    AcquireSRWLockShared(&g_kndLock);int ok=KnpAlias(&g_kndCache,canonical,local,GetTickCount(),alias);
    ReleaseSRWLockShared(&g_kndLock);
    if(!ok || prefix+strlen(alias)>=sizeof(combined))return original;
    memcpy(combined,raw,prefix);memcpy(combined+prefix,alias,strlen(alias)+1);
    if(!KnpValidName(combined,NULL) || !(buffers=KndThreadBuffers()))return original;
    memcpy(buffers->alias,combined,strlen(combined)+1);return buffers->alias;
}
static __attribute__((used,noinline,force_align_arg_pointer)) int __cdecl KndLobbyLabel(const char *original,char *output)
{
    const char *selected=KndSelectMember(original,original-8);
    wchar_t wide[64]={0};char encoded[70]={0};BOOL replaced=FALSE;
    if(selected==original)return 0;
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,selected,-1,wide,64);
    if(!count || !WideCharToMultiByte(936,WC_NO_BEST_FIT_CHARS,wide,count,encoded,sizeof(encoded),NULL,&replaced) || replaced)return 0;
    memcpy(output,encoded,sizeof(encoded));return 1;
}
static int KndChineseFont(void)
{
    uint32_t dvar=0,language=0;
    return KndRead(0x2800a80u,&dvar,4) && dvar && KndRead(dvar+12,&language,4) && language==11;
}
static int KndNativeEncoding(const char *source,size_t size,char encoded[1024],size_t *length)
{
    wchar_t wide[1024];BOOL replaced=FALSE;int extended=0;
    if(!size || size>=1024)return 0;
    for(size_t i=0;i<size;++i)extended|=(unsigned char)source[i]>=128;
    if(!extended)return 0;
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source,(int)size,wide,1024);
    if(!count)return 0;
    int bytes=WideCharToMultiByte(936,WC_NO_BEST_FIT_CHARS,wide,count,encoded,1024,NULL,&replaced);
    if(!bytes || replaced)return 0;
    *length=(size_t)bytes;return 1;
}
static __attribute__((used,noinline,force_align_arg_pointer)) const char * __cdecl KndHUDMessage(const char *original)
{
    char copy[1024],encoded[1024];size_t name_at=0,name_size=0,encoded_size=0;int localized=1,connected=0;KndBuffers *buffers;
    if(!KndChineseFont() || !KndCString(original,copy,sizeof(copy)))return original;
    size_t length=strlen(copy);
    for(size_t at=0;at<length;) {
        unsigned marker=(unsigned char)copy[at];
        if(marker==0x14 || marker==0x15 || marker==0x16) {
            if(marker==0x14)localized=1;
            if(marker==0x15)localized=0;
            if(marker==0x16)return original;
            ++at;continue;
        }
        size_t end=at;while(end<length && copy[end]!=0x14 && copy[end]!=0x15 && copy[end]!=0x16)++end;
        if(localized) {
            if(connected || end-at!=sizeof("MP_CONNECTED")-1 || memcmp(copy+at,"MP_CONNECTED",end-at))return original;
            connected=1;
        } else {
            if(name_size || end-at>69)return original;
            name_at=at;name_size=end-at;
            for(size_t i=at;i<end;++i)if((unsigned char)copy[i]<32 || (unsigned char)copy[i]==127)return original;
        }
        at=end;
    }
    if(!connected || !name_size || !KndNativeEncoding(copy+name_at,name_size,encoded,&encoded_size) ||
       length-name_size+encoded_size>=1024 || !(buffers=KndThreadBuffers()))return original;
    memcpy(buffers->hud,copy,name_at);memcpy(buffers->hud+name_at,encoded,encoded_size);
    memcpy(buffers->hud+name_at+encoded_size,copy+name_at+name_size,length-name_at-name_size+1);
    return buffers->hud;
}
static __attribute__((used,noinline,force_align_arg_pointer)) void __cdecl KndObituary(char *output)
{
    char copy[70],encoded[1024];size_t length=0;
    if(!KndChineseFont() || !KndCString(output,copy,sizeof(copy)) ||
       !KndNativeEncoding(copy,strlen(copy),encoded,&length) || length>=sizeof(copy))return;
    memcpy(output,encoded,length);output[length]=0;
}
void * __cdecl KND_Original(void *output,const char *text);
static __attribute__((used,noinline,force_align_arg_pointer)) void * __cdecl KndConvert(void *output,const char *text)
{
    char copy[128];int extended=0;BYTE in_game=0;uint32_t movie=0;
    if(!KndCString(text,copy,sizeof(copy)) || !KnpValidName(copy,&extended) || !extended ||
       !KndRead(0x6c44d6cu,&in_game,1) || !KndRead(in_game?0x810c50cu:0x810c2a4u,&movie,4) || !movie)
        return KND_Original(output,text);
    ((uint32_t *)output)[0]=((uint32_t *)output)[1]=0;
    typedef void (__attribute__((thiscall)) *Create)(void *,void *,const char *);
    ((Create)(uintptr_t)0x744aa0u)((void *)(uintptr_t)movie,output,text);return output;
}
__attribute__((used)) uint32_t g_kndOriginal=0x5c7340u,g_kndSourceContinue=0x5aa786u,g_kndMemberContinue=0x695990u;
__attribute__((used)) uint32_t g_kndLobbyContinue=0x60ad9au,g_kndLobbyAlias=0x60ade7u,g_kndLanguage=0x2800a80u;
__attribute__((used)) uint32_t g_kndOriginalHUD=0x448e10u,g_kndOriginalObituary=0x488af0u;
void KND_Convert(void);void KND_Source(void);void KND_Member(void);void KND_Lobby(void);void KND_HUD(void);void KND_Obituary(void);
__asm__(".text\n.globl _KND_Original\n_KND_Original:\n"
        "pushl %ebp\nmovl %esp,%ebp\npushl %esi\nmovl 8(%ebp),%esi\npushl 12(%ebp)\ncall *_g_kndOriginal\naddl $4,%esp\npopl %esi\nleave\nret\n"
        ".globl _KND_Convert\n_KND_Convert:\n"
        "pushl %ebp\nmovl %esp,%ebp\npushl 8(%ebp)\npushl %esi\ncall _KndConvert\naddl $8,%esp\nleave\nret\n"
        ".globl _KND_Source\n_KND_Source:\n"
        "pushfl\npushal\nmovl 12(%eax),%ecx\npushl %ecx\ncall _KndSelectAlias\naddl $4,%esp\nmovl %eax,24(%esp)\npopal\npopfl\npushl $63\npushl %ecx\njmp *_g_kndSourceContinue\n"
        ".globl _KND_Member\n_KND_Member:\n"
        "pushfl\npushal\nmovl -100(%ebp),%eax\nleal -8(%edi),%edx\npushl %edx\npushl %eax\ncall _KndSelectMember\naddl $8,%esp\nmovl %eax,-100(%ebp)\npopal\npopfl\nmovl -32(%ebp),%edx\nmovl -36(%ebp),%ecx\njmp *_g_kndMemberContinue\n"
        ".globl _KND_Lobby\n_KND_Lobby:\n"
        "pushfl\npushal\nleal -80(%ebp),%eax\npushl %eax\npushl %ebx\ncall _KndLobbyLabel\naddl $8,%esp\ntestb %al,%al\njz 1f\npopal\npopfl\nmovl _g_kndLanguage,%edx\nmovl (%edx),%edx\njmp *_g_kndLobbyAlias\n"
        "1: popal\npopfl\nmovl _g_kndLanguage,%edx\nmovl (%edx),%edx\njmp *_g_kndLobbyContinue\n"
        ".globl _KND_HUD\n_KND_HUD:\n"
        "pushfl\npushal\npushl %ecx\ncall _KndHUDMessage\naddl $4,%esp\nmovl %eax,24(%esp)\npopal\npopfl\njmp *_g_kndOriginalHUD\n"
        ".globl _KND_Obituary\n_KND_Obituary:\n"
        "call *_g_kndOriginalObituary\npushfl\npushal\ntestl %eax,%eax\njz 2f\npushl %esi\ncall _KndObituary\naddl $4,%esp\n"
        "2: popal\npopfl\nret\n");
