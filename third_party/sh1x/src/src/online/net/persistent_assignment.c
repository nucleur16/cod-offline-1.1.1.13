/* Strict assignment parsing; never accept arbitrary dvar/console commands.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "persistent_assignment.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

const char *const PersistentRuleNames[PA_RULES]={
    "exitToLobby","gpRatePercent","oneShotMatch","sv_playlistId",
    "quickParties","sv_quickPlay","xpRatePercent","zombiesEndlessEnabled",
    "g_allowVote","g_difficulty","g_hardcore","scr_botfill","scr_botmax",
    "scr_bots","scr_game_allowkillcam","scr_hardcore","scr_player_healthregentime",
    "scr_player_maxhealth","scr_team_fftype","scr_xpscale","sv_botDifficulty","sv_fps",
    "sv_maxclients","sv_privateClients","sv_randomMapRotation","ui_hud_hardcore"
};
static int hex(const char *s,unsigned n) {
    unsigned i,nonzero=0;
    if (!s || strlen(s)!=n) return 0;
    for(i=0;i<n;i++) { if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0; nonzero|=s[i]!='0'; }
    return nonzero!=0;
}
static int number(const char *s,uint64_t lo,uint64_t hi,uint64_t *out) {
    uint64_t v=0;unsigned i;
    if (!s || !*s || (s[0]=='0'&&s[1]))return 0;
    for(i=0;s[i];i++) { unsigned d=(unsigned)(s[i]-'0');if(d>9||v>UINT64_MAX/10||(v==UINT64_MAX/10&&d>UINT64_MAX%10))return 0;v=v*10+d; }
    if(v<lo||v>hi)return 0;
    *out=v;return 1;
}
static int token(const char *s) {
    unsigned i;if(!s||!*s||strlen(s)>63)return 0;
    for(i=0;s[i];i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'))return 0;
    return 1;
}
static char *field(char **cursor,const char *name) {
    char *line=*cursor,*end;size_t n=strlen(name);
    if(!line || !(end=strchr(line,'\n')))return NULL;
    *end=0;*cursor=end+1;
    if(strncmp(line,name,n)||line[n]!='='||!line[n+1])return NULL;
    return line+n+1;
}
static int string_field(char **p,const char *name,char *out,size_t cap,int kind) {
    char *s=field(p,name);size_t n;
    if(!s || (n=strlen(s))>=cap || (kind? !hex(s,(unsigned)cap-1):!token(s)))return 0;
    memcpy(out,s,n+1);return 1;
}
static int rule(unsigned i,const char *s) {
    static const unsigned low[PA_RULES]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,10,1,0,0,0};
    static const unsigned high[PA_RULES]={1,400,0,65535,0,0,400,1,0,3,1,18,18,18,1,1,60,1000,3,0,0,60,18,0,0,1};
    uint64_t v;
    if(i==20)return !strcmp(s,"recruit")||!strcmp(s,"regular")||!strcmp(s,"hardened")||!strcmp(s,"veteran");
    if(i==19) {
        size_t n=strlen(s);unsigned j;
        if(n<1||n>4||s[0]<'0'||s[0]>'4')return 0;
        if(n==1)return 1;
        if(n<3||s[1]!='.'||s[n-1]=='0')return 0;
        for(j=2;j<n;j++)if(s[j]<'0'||s[j]>'9'||(s[0]=='4'&&s[j]!='0'))return 0;
        return 1;
    }
    return number(s,low[i],high[i],&v);
}
int PersistentAssignmentDecode(const void *input,size_t size,persistent_assignment_t *out,pa_hash_t hash) {
    char text[PA_LIMIT+1],*p,*s;size_t start,end;unsigned i,j;
    uint64_t v;persistent_assignment_t a;
    if(!input||!out||!hash||!size||size>PA_LIMIT)return 0;
    if(memchr(input,0,size)||memchr(input,'\r',size))return 0;
    memcpy(text,input,size);text[size]=0;
    if(strncmp(text,"codol-persistent-v1\n",20))return 0;
    memset(&a,0,sizeof(a));p=text+20;
    if(!string_field(&p,"slot",a.slot,sizeof(a.slot),1)||!string_field(&p,"boot",a.boot,sizeof(a.boot),1)||
       !string_field(&p,"lease",a.lease,sizeof(a.lease),1)||!string_field(&p,"command",a.command,sizeof(a.command),1))return 0;
    s=field(&p,"epoch");if(!number(s,1,UINT64_MAX,&a.epoch))return 0;
    s=field(&p,"profile");if(!s||(strcmp(s,"normal")&&strcmp(s,"zombies")))return 0;strcpy(a.profile,s);
    if(!string_field(&p,"profile_sha",a.profile_sha,sizeof(a.profile_sha),1)||!string_field(&p,"catalogue_sha",a.catalogue_sha,sizeof(a.catalogue_sha),1))return 0;
    start=(size_t)(p-text);
    if(!string_field(&p,"category",a.category,sizeof(a.category),0)||!string_field(&p,"map",a.map,sizeof(a.map),0)||
       !string_field(&p,"session_map",a.session_map,sizeof(a.session_map),0)||!string_field(&p,"gametype",a.gametype,sizeof(a.gametype),0))return 0;
    if(!strcmp(a.profile,"normal")?(strcmp(a.category,"mp")&&strcmp(a.category,"coop")):strcmp(a.category,"zm"))return 0;
    for(i=0;i<PA_RULES;i++) { s=field(&p,PersistentRuleNames[i]);if(!s||strlen(s)>=sizeof(a.rules[i])||!rule(i,s))return 0;strcpy(a.rules[i],s); }
    end=(size_t)(p-text);
    /* Hash original canonical bytes, not the tokenizer's modified buffer. */
    if(!hash((const char *)input+start,end-start,a.selection)||!hex(a.selection,64))return 0;
    s=field(&p,"members");if(!number(s,1,18,&v))return 0;a.member_count=(unsigned)v;
    for(i=0;i<a.member_count;i++) {
        s=field(&p,"uid");if(!hex(s,16))return 0;v=0;
        for(j=0;j<16;j++)v=(v<<4)|(unsigned)(s[j]<='9'?s[j]-'0':s[j]-'a'+10);
        for(j=0;j<i;j++)if(a.members[j]==v)return 0;
        a.members[i]=v;
    }
    s=field(&p,"expiry");if(!number(s,1,UINT64_MAX,&a.expiry)||*p)return 0;
    if(!number(a.rules[22],1,18,&v)||a.member_count>v)return 0;
    if(!number(a.rules[11],0,v,&v))return 0;
    if(!number(a.rules[22],1,18,&v)||!number(a.rules[12],0,v,&v))return 0;
    memcpy(out,&a,sizeof(a));return 1;
}
int PersistentAssignmentRead(const char *root,const char *digest,persistent_assignment_t *out,pa_hash_t hash) {
    char data[PA_LIMIT];size_t size=0;
    return PersistentBlobRead(root,digest,"assign",data,sizeof(data),&size,hash) &&
        PersistentAssignmentDecode(data,size,out,hash);
}
_Static_assert(offsetof(persistent_grant_t,epoch)==168,"grant epoch ABI");
_Static_assert(sizeof(persistent_grant_t)==344,"grant carrier ABI");
int PersistentGrantDecode(const void *input,size_t size,persistent_grant_t *out) {
    static const char prefix[]="codol-grant-v1\n";
    char text[1025],*p,*s;persistent_grant_t g;uint64_t count,uid;unsigned i,j;
    if(!input||!out||!size||size>1024||memchr(input,0,size)||memchr(input,'\r',size))return 0;
    memcpy(text,input,size);text[size]=0;
    if(size<sizeof(prefix)-1||memcmp(text,prefix,sizeof(prefix)-1))return 0;
    memset(&g,0,sizeof(g));p=text+sizeof(prefix)-1;
    if(!string_field(&p,"slot",g.slot,sizeof(g.slot),1)||!string_field(&p,"boot",g.boot,sizeof(g.boot),1)||
       !string_field(&p,"lease",g.lease,sizeof(g.lease),1)||!string_field(&p,"request",g.request,sizeof(g.request),1))return 0;
    s=field(&p,"epoch");if(!number(s,1,UINT64_MAX,&g.epoch))return 0;
    s=field(&p,"sequence");if(!number(s,1,UINT64_MAX,&g.sequence))return 0;
    s=field(&p,"members");if(!number(s,1,18,&count))return 0;g.member_count=(unsigned)count;
    for(i=0;i<g.member_count;++i) {
        s=field(&p,"uid");if(!hex(s,16))return 0;uid=0;
        for(j=0;j<16;++j)uid=(uid<<4)|(unsigned)(s[j]<='9'?s[j]-'0':s[j]-'a'+10);
        for(j=0;j<i;++j)if(g.members[j]==uid)return 0;g.members[i]=uid;
    }
    s=field(&p,"expiry");if(!number(s,1,UINT64_MAX,&g.expiry)||*p)return 0;
    memcpy(out,&g,sizeof(g));return 1;
}
int PersistentGrantRead(const char *root,const char *digest,persistent_grant_t *out,pa_hash_t hash) {
    char data[1024];size_t size=0;
    return PersistentBlobRead(root,digest,"grant",data,sizeof(data),&size,hash)&&PersistentGrantDecode(data,size,out);
}
#ifdef _WIN32
/* Match POSIX owner/private-mode checks using the opened object's descriptor.
 * A public/inherited allow ACE is rejected; SYSTEM/admin maintenance is allowed.
 * Handles deny writes/deletes during validation and content hashing. */
static int private_handle(HANDLE handle)
{
    HANDLE token=NULL; DWORD bytes=0; unsigned char user[512];
    PSECURITY_DESCRIPTOR security=NULL; PSID owner=NULL; PACL acl=NULL;
    unsigned char system[SECURITY_MAX_SID_SIZE],admin[SECURITY_MAX_SID_SIZE];
    DWORD system_size=sizeof(system),admin_size=sizeof(admin); int ok=0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 0;
    if(!GetTokenInformation(token,TokenUser,user,sizeof(user),&bytes))goto done;
    if(GetSecurityInfo(handle,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
                       &owner,NULL,&acl,NULL,&security)!=ERROR_SUCCESS || !owner || !acl ||
       !EqualSid(owner,((TOKEN_USER *)user)->User.Sid))goto done;
    if(!CreateWellKnownSid(WinLocalSystemSid,NULL,system,&system_size) ||
       !CreateWellKnownSid(WinBuiltinAdministratorsSid,NULL,admin,&admin_size))goto done;
    for(DWORD i=0;i<acl->AceCount;++i) {
        ACE_HEADER *header;
        if(!GetAce(acl,i,(void **)&header))goto done;
        if(header->AceFlags&INHERIT_ONLY_ACE)continue;
        if(header->AceType==ACCESS_DENIED_ACE_TYPE)continue;
        if(header->AceType!=ACCESS_ALLOWED_ACE_TYPE)goto done;
        ACCESS_ALLOWED_ACE *allow=(ACCESS_ALLOWED_ACE *)header;
        PSID sid=&allow->SidStart;
        if(!IsValidSid(sid) || (!EqualSid(sid,((TOKEN_USER *)user)->User.Sid) &&
            !EqualSid(sid,system) && !EqualSid(sid,admin)))goto done;
    }
    ok=1;
done:
    if(security)LocalFree(security);
    CloseHandle(token);return ok;
}
static int windows_blob_read(const char *root,const char *name,void *data,size_t capacity,
                             size_t *used)
{
    HANDLE directory=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info; LARGE_INTEGER length;
    char path[32768]; DWORD got=0; int result=0,n;
    directory=CreateFileA(root,GENERIC_READ|READ_CONTROL,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(directory==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(directory,&info) ||
       !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
       (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || !private_handle(directory))goto done;
    n=snprintf(path,sizeof(path),"%s\\%s",root,name);
    if(n<0 || (size_t)n>=sizeof(path))goto done;
    file=CreateFileA(path,GENERIC_READ|READ_CONTROL,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                     FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(file==INVALID_HANDLE_VALUE || GetFileType(file)!=FILE_TYPE_DISK ||
       !GetFileInformationByHandle(file,&info) ||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) ||
       !private_handle(file) || !GetFileSizeEx(file,&length) ||
       length.QuadPart<1 || (uint64_t)length.QuadPart>capacity)goto done;
    if(!ReadFile(file,data,(DWORD)length.QuadPart,&got,NULL) || got!=(DWORD)length.QuadPart)goto done;
    *used=got;result=1;
done:
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    if(directory!=INVALID_HANDLE_VALUE)CloseHandle(directory);
    return result;
}
#endif
int PersistentBlobRead(const char *root,const char *digest,const char *extension,void *data,size_t capacity,size_t *size,pa_hash_t hash) {
    char name[88],actual[65];size_t used=0;
#ifndef _WIN32
    int dir=-1,fd=-1,result=0;struct stat st;unsigned char extra;
#endif
    if(!root||!data||!size||!hash||!hex(digest,64)||!extension||
       (strcmp(extension,"assign")&&strcmp(extension,"catalogue")&&strcmp(extension,"grant"))||!capacity||capacity>131072)return 0;
    snprintf(name,sizeof(name),"%s.%s",digest,extension);
#ifdef _WIN32
    if(!windows_blob_read(root,name,data,capacity,&used) ||
       !hash(data,used,actual) || strcmp(actual,digest))return 0;
    *size=used;return 1;
#else
    dir=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(dir<0)return 0;
    if(fstat(dir,&st)||st.st_uid!=geteuid()||(st.st_mode&077))goto done;
    snprintf(name,sizeof(name),"%s.%s",digest,extension);
    fd=openat(dir,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)goto done;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077)||st.st_size<1||(uint64_t)st.st_size>capacity)goto done;
    while(used<capacity) { ssize_t n=read(fd,(char *)data+used,capacity-used);if(n<0)goto done;if(!n)break;used+=(size_t)n; }
    if(read(fd,&extra,1)!=0)goto done;
    if(used!=(size_t)st.st_size||!hash(data,used,actual)||strcmp(actual,digest))goto done;
    *size=used;result=1;
done:
    if(fd>=0)close(fd);
    close(dir);return result;
#endif
}
int PersistentCatalogueAllows(const char *root,const char *digest,const persistent_assignment_t *a,pa_hash_t hash) {
    static char data[131073];char prefix[160],wanted[270],previous[270]={0},*p,*e;size_t size=0,n;unsigned found=0,rows=0;
    if(!a||strcmp(digest,a->catalogue_sha)||!PersistentBlobRead(root,digest,"catalogue",data,sizeof(data)-1,&size,hash))return 0;
    if(memchr(data,0,size)||memchr(data,'\r',size))return 0;
    data[size]=0;
    n=(size_t)snprintf(prefix,sizeof(prefix),"codol-catalogue-v1\nprofile=%s\nprofile_sha=%s\n",a->profile,a->profile_sha);
    if(n>=sizeof(prefix)||size<=n||memcmp(data,prefix,n))return 0;
    snprintf(wanted,sizeof(wanted),"%s|%s|%s|%s",a->category,a->map,a->session_map,a->gametype);
    p=data+n;
    while(*p) {
        unsigned separators=0;char *part;size_t len;
        e=strchr(p,'\n');if(!e||(len=(size_t)(e-p))>=sizeof(previous)||!len)return 0;*e=0;
        if(rows && strcmp(previous,p)>=0)return 0;
        strcpy(previous,p);found|=!strcmp(wanted,p);part=p;
        for(;;) { char *bar=strchr(part,'|');if(bar)*bar=0;if(!token(part))return 0;if(!bar)break;++separators;part=bar+1; }
        if(separators!=3)return 0;
        if(++rows>2048)return 0;
        p=e+1;
    }
    return rows && found;
}
int PersistentCatalogueIdle(const char *root,const char *digest,const char *profile,const char *profile_sha,
    uint32_t seed,persistent_assignment_t *out,pa_hash_t hash) {
    static char data[131073];char prefix[160],previous[270]={0},*p,*e;
    size_t size,n;unsigned rows=0;persistent_assignment_t selected;
    if(!out||!profile||!profile_sha||!hex(profile_sha,64)||
       (strcmp(profile,"normal")&&strcmp(profile,"zombies"))||
       !PersistentBlobRead(root,digest,"catalogue",data,sizeof(data)-1,&size,hash)||
       memchr(data,0,size)||memchr(data,'\r',size))return 0;
    data[size]=0;
    n=(size_t)snprintf(prefix,sizeof(prefix),"codol-catalogue-v1\nprofile=%s\nprofile_sha=%s\n",profile,profile_sha);
    if(n>=sizeof(prefix)||size<=n||memcmp(data,prefix,n))return 0;
    memset(&selected,0,sizeof(selected));p=data+n;
    while(*p) {
        char *part,*fields[4];unsigned i;size_t len;
        e=strchr(p,'\n');if(!e||!(len=(size_t)(e-p))||len>=sizeof(previous))return 0;*e=0;
        if(rows&&strcmp(previous,p)>=0)return 0;strcpy(previous,p);part=p;
        for(i=0;i<4;++i) {
            char *bar=strchr(part,'|');if((i<3)!=!!bar)return 0;
            if(bar)*bar=0;if(!token(part)||strlen(part)>=64)return 0;
            fields[i]=part;if(bar)part=bar+1;
        }
        if(!strcmp(profile,"zombies")?strcmp(fields[0],"zm"):
            (strcmp(fields[0],"mp")&&strcmp(fields[0],"coop")))return 0;
        if(++rows>2048)return 0;
        /* Private reservoir RNG does not change the gameplay RNG. */
        seed=seed*1664525u+1013904223u;
        if(rows==1||seed%rows==0) {
            strcpy(selected.category,fields[0]);strcpy(selected.map,fields[1]);
            strcpy(selected.session_map,fields[2]);strcpy(selected.gametype,fields[3]);
        }
        p=e+1;
    }
    if(!rows)return 0;*out=selected;return 1;
}
