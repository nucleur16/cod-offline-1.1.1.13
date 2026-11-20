/* Bounded generation-fenced receipt chain, not a replacement payload spool.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_reports.h"
#include <stdio.h>
#include <string.h>
static int hex(const char *s,unsigned n) {
    unsigned i,nonzero=0;
    if(!s||strlen(s)!=n)return 0;
    for(i=0;i<n;i++){if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;nonzero|=s[i]!='0';}
    return nonzero!=0;
}
int PersistentReportsBegin(persistent_reports_t *s,const char *boot,const char *lease,uint64_t epoch,const char *request,pa_hash_t hash) {
    char text[256],scope[65];int n;
    if(!s||s->scope[0]||!hex(boot,32)||!hex(lease,32)||!epoch||!hex(request,64)||!hash)return 0;
    n=snprintf(text,sizeof(text),"codol-resident-reports-v1\nboot=%s\nlease=%s\nepoch=%llu\nrequest=%s\n",boot,lease,(unsigned long long)epoch,request);
    if(n<0||(size_t)n>=sizeof(text)||!hash(text,(size_t)n,scope)||!hex(scope,64))return 0;
    memset(s,0,sizeof(*s));strcpy(s->scope,scope);strcpy(s->chain,scope);return 1;
}
int PersistentReportsPublished(persistent_reports_t *s,const char *scope,const char *path,const void *payload,size_t size,pa_hash_t hash) {
    char path_sha[65],payload_sha[65],chain[65],text[300];unsigned i;int n;
    if(!s||!hex(scope,64)||strcmp(s->scope,scope)||!path||!*path||strlen(path)>1023||!payload||!size||size>65536||!hash)return 0;
    if(!hash(path,strlen(path),path_sha)||!hex(path_sha,64)||!hash(payload,size,payload_sha)||!hex(payload_sha,64))return 0;
    for(i=0;i<s->count;i++)if(!strcmp(s->receipts[i].path,path_sha))return !strcmp(s->receipts[i].payload,payload_sha)?2:0;
    if(s->sealed[0]||s->count>=PR_RECEIPTS)return 0;
    n=snprintf(text,sizeof(text),"%s\n%s\n%s\n%u\n",s->chain,path_sha,payload_sha,s->count+1);
    if(n<0||(size_t)n>=sizeof(text)||!hash(text,(size_t)n,chain)||!hex(chain,64))return 0;
    strcpy(s->receipts[s->count].path,path_sha);strcpy(s->receipts[s->count].payload,payload_sha);
    ++s->count;strcpy(s->chain,chain);return 1;
}
int PersistentReportsSeal(persistent_reports_t *s,int barrier,pa_hash_t hash) {
    char text[200],sealed[65];int n;
    if(!s||!hex(s->scope,64)||barrier!=1||!hash)return 0;
    if(s->sealed[0])return 2;
    n=snprintf(text,sizeof(text),"codol-report-barrier-v1\n%s\n%s\n%u\n",s->scope,s->chain,s->count);
    if(n<0||(size_t)n>=sizeof(text)||!hash(text,(size_t)n,sealed)||!hex(sealed,64))return 0;
    strcpy(s->sealed,sealed);return 1;
}
int PersistentReportsRetire(persistent_reports_t *s,const char *scope,const char *sealed) {
    if(!s||!hex(scope,64)||!hex(sealed,64)||strcmp(s->scope,scope)||strcmp(s->sealed,sealed))return 0;
    memset(s,0,sizeof(*s));return 1;
}
