/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "owned_items.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OWNED_FILE_MAX = 1 << 20 };

/* Parse one unsigned decimal field; advances *p. Returns 1 when a number was read. */
static int read_number(const char **p,const char *end,uint64_t *out)
{
    const char *s=*p;uint64_t v=0;int any=0;
    while(s<end && (*s==' ' || *s=='\t'))++s;
    if(s<end && *s=='-') {
        /* A negative expiry is the lobby's -1 "permanent"; anything else is malformed. */
        ++s;
        while(s<end && *s>='0' && *s<='9'){v=v*10+(uint64_t)(*s-'0');++s;any=1;if(v>0xffffffffu)return 0;}
        if(!any || v!=1)return 0;
        *out=0xffffffffu;*p=s;return 1;
    }
    while(s<end && *s>='0' && *s<='9') {
        v=v*10+(uint64_t)(*s-'0');++s;any=1;
        if(v>0xffffffffu)return 0;
    }
    if(!any)return 0;
    *out=v;*p=s;return 1;
}

int OwnedItemsLookup(const char *text,size_t length,unsigned item,uint32_t now)
{
    const char *p=text,*end=text ? text+length : NULL,*line_end;int owned=0;
    if(!text)return 0;
    while(p<end) {
        uint64_t id,expiry,qty;const char *q;
        line_end=memchr(p,'\n',(size_t)(end-p));
        if(!line_end)line_end=end;
        q=p;
        if(read_number(&q,line_end,&id) && read_number(&q,line_end,&expiry) &&
           read_number(&q,line_end,&qty) && id==item) {
            /* The last row for an item wins, as the lobby writes one row per item. */
            owned=qty>0 && (expiry==0 || expiry==0xffffffffu || expiry>now);
        }
        p=line_end<end ? line_end+1 : end;
    }
    return owned;
}

/* 1 read, 0 absent, -1 unreadable. */
static int read_text(const char *directory,const char *name,char **out,size_t *length)
{
    char path[4096];FILE *f;long size;char *text;int written,ok;
    written=snprintf(path,sizeof(path),"%s/%s",directory,name);
    if(written<0 || (size_t)written>=sizeof(path))return -1;
    f=fopen(path,"rb");if(!f)return errno==ENOENT ? 0 : -1;
    if(fseek(f,0,SEEK_END) || (size=ftell(f))<0 || size>OWNED_FILE_MAX || fseek(f,0,SEEK_SET)) {
        fclose(f);return -1;
    }
    text=malloc((size_t)size+1);if(!text){fclose(f);return -1;}
    ok=fread(text,1,(size_t)size,f)==(size_t)size && !ferror(f);
    if(fclose(f))ok=0;
    if(!ok){free(text);return -1;}
    text[size]=0;*out=text;*length=(size_t)size;return 1;
}

int ReadOwnedItem(const char *directory,uint64_t uid,unsigned item,uint32_t now)
{
    char name[64];char *text=NULL;size_t length=0;int status=0,owned;
    if(!directory || !*directory)return -1;
    if(uid) {
        snprintf(name,sizeof(name),"owned_items.%" PRIu64 ".txt",uid);
        status=read_text(directory,name,&text,&length);
    }
    if(!status)status=read_text(directory,"owned_items.txt",&text,&length);
    if(status<=0)return status;
    owned=OwnedItemsLookup(text,length,item,now);
    free(text);return owned;
}
