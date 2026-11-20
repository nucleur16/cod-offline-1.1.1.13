#!/usr/bin/env python3
"""Compile actual Win32 reader bodies with explicit OS doubles; no Windows EXE runs."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/online/net/persistent_assignment.c').read_text()
start=s.index('static int private_handle(')
end=s.index('\nint PersistentCatalogueAllows',start)
reader=s[start:end]
# The existing digest syntax validator is production code too.
hstart=s.index('static int hex(');hend=s.index('\nstatic int number(',hstart)
fixture=r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define _WIN32 1
#undef NULL
#define NULL 0
typedef uintptr_t HANDLE;
typedef unsigned long DWORD;
typedef void *PSID;
typedef void *PSECURITY_DESCRIPTOR;
typedef struct {PSID Sid;} SID_AND_ATTRIBUTES;
typedef struct {SID_AND_ATTRIBUTES User;} TOKEN_USER;
typedef struct {unsigned char AceType,AceFlags;unsigned short AceSize;} ACE_HEADER;
typedef struct {ACE_HEADER Header;DWORD Mask;unsigned SidStart;} ACCESS_ALLOWED_ACE;
typedef struct {DWORD AceCount;} ACL,*PACL;
typedef struct {DWORD dwFileAttributes;} BY_HANDLE_FILE_INFORMATION;
typedef struct {int64_t QuadPart;} LARGE_INTEGER;
typedef int (*pa_hash_t)(const void *,size_t,char *);
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define TOKEN_QUERY 1
#define TokenUser 1
#define SE_FILE_OBJECT 1
#define OWNER_SECURITY_INFORMATION 1
#define DACL_SECURITY_INFORMATION 2
#define ERROR_SUCCESS 0
#define SECURITY_MAX_SID_SIZE 64
#define WinLocalSystemSid 2
#define WinBuiltinAdministratorsSid 3
#define INHERIT_ONLY_ACE 8
#define ACCESS_DENIED_ACE_TYPE 1
#define ACCESS_ALLOWED_ACE_TYPE 0
#define GENERIC_READ 1
#define READ_CONTROL 2
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 1
#define FILE_FLAG_BACKUP_SEMANTICS 1
#define FILE_FLAG_OPEN_REPARSE_POINT 2
#define FILE_FLAG_SEQUENTIAL_SCAN 4
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_ATTRIBUTE_REPARSE_POINT 1024
#define FILE_TYPE_DISK 1
static ACL acl={1};
static ACCESS_ALLOWED_ACE ace={{0,0,0},1,1};
static unsigned owner=1,user_sid=1;
static DWORD attrs[3];
static int opens,closes,bad_acl,bad_owner,read_fail,short_read,file_type;
static int64_t length;
static HANDLE GetCurrentProcess(void){return 10;}
static int OpenProcessToken(HANDLE h,DWORD access,HANDLE *out){assert(h==10&&access==TOKEN_QUERY);*out=3;return 1;}
static int GetTokenInformation(HANDLE h,int kind,void *out,DWORD cap,DWORD *bytes){assert(h==3&&kind==TokenUser&&cap>=sizeof(TOKEN_USER));((TOKEN_USER*)out)->User.Sid=&user_sid;*bytes=sizeof(TOKEN_USER);return 1;}
static DWORD GetSecurityInfo(HANDLE h,int type,DWORD mask,PSID *o,void *group,PACL *a,void *sacl,PSECURITY_DESCRIPTOR *sd){assert(type==SE_FILE_OBJECT&&mask==3&&!group&&!sacl);owner=bad_owner?9:1;ace.SidStart=bad_acl==(int)h?9:1;*o=&owner;*a=&acl;*sd=(void*)1;return 0;}
static int EqualSid(PSID a,PSID b){return *(unsigned*)a==*(unsigned*)b;}
static int CreateWellKnownSid(int kind,void *domain,void *out,DWORD *size){assert(!domain&&*size>=4);*(unsigned*)out=kind;*size=4;return 1;}
static int GetAce(PACL a,DWORD i,void **out){assert(a==&acl&&!i);*out=&ace;return 1;}
static int IsValidSid(PSID s){return s!=NULL;}
static void LocalFree(void *p){assert(p==(void*)1);}
static int CloseHandle(HANDLE h){assert(h>=1&&h<=3);++closes;return 1;}
static HANDLE CreateFileA(const char *path,DWORD access,DWORD share,void *security,DWORD creation,DWORD flags,HANDLE template_handle){assert(access==3&&share==FILE_SHARE_READ&&!security&&creation==OPEN_EXISTING&&!template_handle);++opens;if(flags&FILE_FLAG_BACKUP_SEMANTICS){assert(!strcmp(path,"private"));return 1;}assert(strstr(path,"private\\") == path);return 2;}
static int GetFileInformationByHandle(HANDLE h,BY_HANDLE_FILE_INFORMATION *info){info->dwFileAttributes=attrs[h];return 1;}
static int GetFileType(HANDLE h){assert(h==2);return file_type;}
static int GetFileSizeEx(HANDLE h,LARGE_INTEGER *out){assert(h==2);out->QuadPart=length;return 1;}
static int ReadFile(HANDLE h,void *data,DWORD count,DWORD *got,void *overlap){assert(h==2&&!overlap);if(read_fail)return 0;memset(data,'x',count);*got=count-(short_read?1:0);return 1;}
static void reset(void){attrs[1]=FILE_ATTRIBUTE_DIRECTORY;attrs[2]=0;opens=closes=bad_acl=bad_owner=read_fail=short_read=0;file_type=FILE_TYPE_DISK;length=3;}
static int digest_ok=1;
static const char digest[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static int hash(const void *data,size_t bytes,char *out){assert(data&&bytes==3);memset(out,digest_ok?'a':'b',64);out[64]=0;return 1;}
"""
main=r"""
int main(void){char data[16];size_t size=99;
reset();assert(PersistentBlobRead("private",digest,"assign",data,sizeof(data),&size,hash));assert(size==3&&opens==2&&closes==4);
reset();bad_owner=1;size=99;assert(!PersistentBlobRead("private",digest,"assign",data,sizeof(data),&size,hash)&&size==99&&opens==1);
reset();bad_acl=1;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();bad_acl=2;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();attrs[1]|=FILE_ATTRIBUTE_REPARSE_POINT;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();attrs[2]=FILE_ATTRIBUTE_REPARSE_POINT;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();attrs[2]=FILE_ATTRIBUTE_DIRECTORY;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();file_type=0;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();length=17;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();length=0;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();read_fail=1;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();short_read=1;assert(!windows_blob_read("private","x",data,sizeof(data),&size));
reset();digest_ok=0;size=99;assert(!PersistentBlobRead("private",digest,"assign",data,sizeof(data),&size,hash)&&size==99);
puts("PASS production Win32 persistent blob owner/ACL, reparse/type, read bound/short/error and digest guards");}
"""
with tempfile.TemporaryDirectory(prefix='codol-win-reader-') as tmp:
 p=Path(tmp);source=p/'reader.c';binary=p/'reader'
 source.write_text(fixture+s[hstart:hend]+'\n#ifdef _WIN32\n'+reader+main)
 subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
