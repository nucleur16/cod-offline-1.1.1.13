#ifndef KERNEL_NAME_POLICY_H
#define KERNEL_NAME_POLICY_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Display-only name policy. These values never authenticate, select a room,
 * change a username dvar, bind a peer UID, or write progression. */
typedef struct {char canonical[64];uint64_t uid,generation;uint32_t session;} KnpIdentity;
typedef struct {KnpIdentity identity;char alias[64];uint32_t alias_at,next_request;} KnpCache;
typedef struct {const unsigned char *at,*end;unsigned depth;} KnpJson;

static int KnpValidName(const char *text,int *extended)
{
    const unsigned char *p=(const unsigned char *)text;
    size_t size=strlen(text);int high=0;
    if(!size || size>63)return 0;
    while(*p) {
        unsigned a=*p++,n=0,low=0x80,upper=0xbf;
        if(a<32 || a==127)return 0;
        if(a<128)continue;
        high=1;
        if(a>=0xc2 && a<=0xdf)n=1;
        else if(a>=0xe0 && a<=0xef){n=2;if(a==0xe0)low=0xa0;if(a==0xed)upper=0x9f;}
        else if(a>=0xf0 && a<=0xf4){n=3;if(a==0xf0)low=0x90;if(a==0xf4)upper=0x8f;}
        else return 0;
        if(!*p || *p<low || *p>upper)return 0;
        ++p;
        while(--n){if(!*p || *p<0x80 || *p>0xbf)return 0;++p;}
    }
    if(extended)*extended=high;
    return 1;
}
static void KnpSpace(KnpJson *json)
{
    while(json->at<json->end && (*json->at==' ' || *json->at=='\t' || *json->at=='\r' || *json->at=='\n'))++json->at;
}
static int KnpHex(KnpJson *json,uint32_t *value)
{
    *value=0;
    for(unsigned i=0;i<4;++i){unsigned c;if(json->at==json->end)return 0;c=*json->at++;
        if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;else if(c>='A'&&c<='F')c=c-'A'+10;else return 0;
        *value=(*value<<4)|c;}
    return 1;
}
static int KnpAppend(char *out,size_t capacity,size_t *length,unsigned char value)
{
    if(out){if(value<32 || *length+1>=capacity)return 0;out[*length]=(char)value;}
    ++*length;return 1;
}
static int KnpString(KnpJson *json,char *out,size_t capacity)
{
    size_t length=0;
    if(json->at==json->end || *json->at++!='"')return 0;
    while(json->at<json->end) {
        uint32_t c=*json->at++;
        if(c=='"'){if(out){if(length>=capacity)return 0;out[length]=0;}return 1;}
        if(c<32)return 0;
        if(c!='\\'){if(!KnpAppend(out,capacity,&length,(unsigned char)c))return 0;continue;}
        if(json->at==json->end)return 0;
        c=*json->at++;
        if(c=='b')c=8;else if(c=='f')c=12;else if(c=='n')c=10;else if(c=='r')c=13;else if(c=='t')c=9;
        else if(c=='u') {
            uint32_t second;
            if(!KnpHex(json,&c))return 0;
            if(c>=0xd800 && c<=0xdbff) {
                if(json->end-json->at<6 || json->at[0]!='\\' || json->at[1]!='u')return 0;
                json->at+=2;if(!KnpHex(json,&second) || second<0xdc00 || second>0xdfff)return 0;
                c=0x10000+((c-0xd800)<<10)+(second-0xdc00);
            } else if(c>=0xdc00 && c<=0xdfff)return 0;
            if(c>=0x10000){if(!KnpAppend(out,capacity,&length,0xf0|(c>>18)) || !KnpAppend(out,capacity,&length,0x80|((c>>12)&63)))return 0;}
            else if(c>=0x800){if(!KnpAppend(out,capacity,&length,0xe0|(c>>12)))return 0;}
            if(c>=0x800){if(!KnpAppend(out,capacity,&length,0x80|((c>>6)&63)))return 0;}
            else if(c>=0x80){if(!KnpAppend(out,capacity,&length,0xc0|(c>>6)))return 0;}
            if(!KnpAppend(out,capacity,&length,(unsigned char)(c<0x80?c:0x80|(c&63))))return 0;
            continue;
        } else if(c!='"' && c!='\\' && c!='/')return 0;
        if(!KnpAppend(out,capacity,&length,(unsigned char)c))return 0;
    }
    return 0;
}
static int KnpLiteral(KnpJson *json,const char *text)
{
    size_t size=strlen(text);if((size_t)(json->end-json->at)<size || memcmp(json->at,text,size))return 0;
    json->at+=size;return 1;
}
static int KnpSkip(KnpJson *json)
{
    KnpSpace(json);if(json->at==json->end || json->depth>=16)return 0;
    if(*json->at=='"')return KnpString(json,NULL,0);
    if(*json->at=='{' || *json->at=='[') {
        unsigned char end=*json->at++=='{'?'}':']';++json->depth;KnpSpace(json);
        if(json->at<json->end && *json->at==end){++json->at;--json->depth;return 1;}
        for(;;) {
            if(end=='}'){if(!KnpString(json,NULL,0))return 0;KnpSpace(json);if(json->at==json->end || *json->at++!=':')return 0;}
            if(!KnpSkip(json))return 0;
            KnpSpace(json);if(json->at==json->end)return 0;
            if(*json->at==end){++json->at;--json->depth;return 1;}
            if(*json->at++!=',')return 0;
            KnpSpace(json);
        }
    }
    if(*json->at=='t')return KnpLiteral(json,"true");
    if(*json->at=='f')return KnpLiteral(json,"false");
    if(*json->at=='n')return KnpLiteral(json,"null");
    if(*json->at=='-')++json->at;
    if(json->at==json->end)return 0;
    if(*json->at=='0')++json->at;
    else {if(*json->at<'1'||*json->at>'9')return 0;do{++json->at;}while(json->at<json->end&&*json->at>='0'&&*json->at<='9');}
    if(json->at<json->end&&*json->at=='.'){++json->at;if(json->at==json->end||*json->at<'0'||*json->at>'9')return 0;do{++json->at;}while(json->at<json->end&&*json->at>='0'&&*json->at<='9');}
    if(json->at<json->end&&(*json->at=='e'||*json->at=='E')){++json->at;if(json->at<json->end&&(*json->at=='+'||*json->at=='-'))++json->at;if(json->at==json->end||*json->at<'0'||*json->at>'9')return 0;do{++json->at;}while(json->at<json->end&&*json->at>='0'&&*json->at<='9');}
    return 1;
}
static int KnpResponse(const char *body,size_t size,const char *canonical,uint64_t uid,char alias[64])
{
    KnpJson json;
    char identity[64]={0},name[64]={0},received_uid[32]={0},expected_uid[32];unsigned seen=0;int uid_null=0;
    alias[0]=0;if(!uid || !size || size>4096 || !canonical[0])return 0;
    json.at=(const unsigned char *)body;json.end=json.at+size;json.depth=0;
    KnpSpace(&json);if(json.at==json.end || *json.at++!='{')return 0;KnpSpace(&json);
    if(json.at<json.end && *json.at=='}')return 0;
    for(;;) {
        char key[64];unsigned bit=0;
        if(!KnpString(&json,key,sizeof(key)))return 0;
        KnpSpace(&json);
        if(json.at==json.end || *json.at++!=':')return 0;
        KnpSpace(&json);
        if(!strcmp(key,"identity"))bit=1;else if(!strcmp(key,"uid"))bit=2;else if(!strcmp(key,"name"))bit=4;
        if(bit && (seen&bit))return 0;
        seen|=bit;
        if(bit==1){if(!KnpString(&json,identity,sizeof(identity)))return 0;}
        else if(bit==2){if(json.at<json.end&&*json.at=='n'){if(!KnpLiteral(&json,"null"))return 0;uid_null=1;}else if(!KnpString(&json,received_uid,sizeof(received_uid)))return 0;}
        else if(bit==4){if(!KnpString(&json,name,sizeof(name)))return 0;}
        else if(!KnpSkip(&json))return 0;
        KnpSpace(&json);if(json.at==json.end)return 0;
        if(*json.at=='}'){++json.at;break;}
        if(*json.at++!=',')return 0;
        KnpSpace(&json);
    }
    KnpSpace(&json);if(json.at!=json.end || seen!=7 || strcmp(identity,canonical) || !KnpValidName(name,NULL))return 0;
    snprintf(expected_uid,sizeof(expected_uid),"%llu",(unsigned long long)uid);
    if((!uid_null && strcmp(received_uid,expected_uid)) || !strcmp(name,expected_uid))return 0;
    memcpy(alias,name,strlen(name)+1);return 1;
}
static int KnpSame(const KnpIdentity *left,const KnpIdentity *right)
{
    return left->uid==right->uid && left->generation==right->generation &&
           left->session==right->session && !strcmp(left->canonical,right->canonical);
}
static void KnpIdentityUpdate(KnpCache *cache,const char *canonical,uint64_t uid,uint32_t session,uint32_t now)
{
    size_t size=strlen(canonical);
    if(size>=sizeof(cache->identity.canonical)){canonical="";size=0;uid=0;}
    if(cache->identity.uid==uid && cache->identity.session==session && !strcmp(cache->identity.canonical,canonical))return;
    memcpy(cache->identity.canonical,canonical,size+1);cache->identity.uid=uid;cache->identity.session=session;
    ++cache->identity.generation;cache->alias[0]=0;cache->alias_at=now;cache->next_request=now;
}
static int KnpReply(KnpCache *cache,const KnpIdentity *key,const char *alias,int ok,uint32_t now)
{
    if(!KnpSame(&cache->identity,key))return 0;
    if(ok && KnpValidName(alias,NULL))memcpy(cache->alias,alias,strlen(alias)+1);
    else {cache->alias[0]=0;ok=0;}
    cache->alias_at=now;return ok;
}
static int KnpAlias(const KnpCache *cache,const char *canonical,uint64_t uid,uint32_t now,char out[64])
{
    out[0]=0;
    if(!uid || cache->identity.uid!=uid || strcmp(cache->identity.canonical,canonical) || (uint32_t)(now-cache->alias_at)>=60000)return 0;
    memcpy(out,cache->alias,strlen(cache->alias)+1);return out[0]!=0;
}
#endif
