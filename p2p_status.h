/* Readiness is the requested running match, not merely an answering process. */
#ifndef K33_P2P_STATUS_H
#define K33_P2P_STATUS_H
#include <string.h>

static int P2PStatusTokenEquals(const char *line, const char *key, const char *value)
{
    size_t keylen, valuelen;
    int found=0;
    if (!line || !key || !value) return 0;
    keylen=strlen(key);valuelen=strlen(value);
    while (*line) {
        const char *end;
        while (*line==' ' || *line=='\t' || *line=='\r' || *line=='\n') ++line;
        if (!*line) break;
        end=line;
        while (*end && *end!=' ' && *end!='\t' && *end!='\r' && *end!='\n') ++end;
        if ((size_t)(end-line)>keylen && !memcmp(line,key,keylen) && line[keylen]=='=') {
            /* Duplicate identity keys are ambiguous and cannot prove readiness. */
            if (found || (size_t)(end-line)!=keylen+1+valuelen ||
                memcmp(line+keylen+1,value,valuelen)) return 0;
            found=1;
        }
        line=end;
    }
    return found;
}

static int P2PStatusMatches(const char *line, const char *map, const char *session,
                           const char *gametype, const char *category)
{
    if (!map || !*map || !gametype || !*gametype || !category || !*category) return 0;
    return P2PStatusTokenEquals(line,"running","1") &&
           P2PStatusTokenEquals(line,"parked","0") &&
           P2PStatusTokenEquals(line,"map",map) &&
           P2PStatusTokenEquals(line,"session_map",session && *session ? session : map) &&
           P2PStatusTokenEquals(line,"gametype",gametype) &&
           P2PStatusTokenEquals(line,"type",category);
}
#endif
