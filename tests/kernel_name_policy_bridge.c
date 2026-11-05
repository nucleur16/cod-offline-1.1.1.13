#include "../kernel_name_policy.h"
static KnpCache cache;
static KnpIdentity request;
int Name(const char *text){return KnpValidName(text,NULL);}
int Response(const char *body,size_t size,const char *canonical,uint64_t uid,char alias[64])
{return KnpResponse(body,size,canonical,uid,alias);}
void Reset(void){memset(&cache,0,sizeof(cache));memset(&request,0,sizeof(request));}
void Identity(const char *name,uint64_t uid,uint32_t session,uint32_t now)
{KnpIdentityUpdate(&cache,name,uid,session,now);}
void Snapshot(void){request=cache.identity;}
uint64_t Generation(void){return cache.identity.generation;}
int Reply(const char *alias,int ok,uint32_t now){return KnpReply(&cache,&request,alias,ok,now);}
int Alias(const char *canonical,uint64_t uid,uint32_t now,char out[64])
{return KnpAlias(&cache,canonical,uid,now,out);}
