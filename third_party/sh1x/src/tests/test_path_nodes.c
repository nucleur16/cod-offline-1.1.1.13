#include "scr_vm_functions.h"
#include "cscr_variable.h"
#include "g_public.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdlib.h>
void GetNodeField(unsigned, int, int);
int SetNodeField(unsigned, int, int);
int IsStoredPathNode(const char *);
void Com_Error(int level, const char *text, ...) { (void)level; (void)text; assert(0); }
static unsigned cursor, resets, fields, errors, outputs, count, entity;
static int duplicate, empty;
static float vector[3], number;
static char string[128];
static const char *args[2];
int Q_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void G_ResetEntityParsePoint(void) { cursor = 0; ++resets; }
static void pair(SpawnVar *v, const char *key, const char *value)
{
    unsigned i = v->numSpawnVars++;
    v->spawnVars[i].key = v->spawnVarChars + v->numSpawnVarChars;
    strcpy(v->spawnVarChars + v->numSpawnVarChars, key); v->numSpawnVarChars += strlen(key)+1;
    v->spawnVars[i].value = v->spawnVarChars + v->numSpawnVarChars;
    strcpy(v->spawnVarChars + v->numSpawnVarChars, value); v->numSpawnVarChars += strlen(value)+1;
}
qboolean G_ParseSpawnVars(SpawnVar *v)
{
    memset(v, 0, sizeof(*v));
    if (empty || cursor == 3) return qfalse;
    pair(v, "classname", cursor == 0 ? "worldspawn" : cursor == 1 ? "node_pathnode" : "node_cover_left");
    pair(v, "targetname", cursor == 1 || duplicate ? "start" : "end");
    pair(v, "origin", "1 2 3"); pair(v, "angles", "0 90 0");
    pair(v, "target", "end"); pair(v, "speed", "12.5");
    pair(v, "lookahead", "nan"); pair(v, "spawnflags", "4");
    ++cursor; return qtrue;
}
void Scr_AddClassField(unsigned c, const char *n, unsigned offset) { assert(c == 2 && n && offset < 11); ++fields; }
char *Scr_GetString(unsigned i) { assert(i < 2); return (char *)args[i]; }
void Scr_MakeArray(void) { count = 0; }
void Scr_AddArray(void) { ++count; }
void Scr_AddEntityNum(int i, unsigned c) { assert(c == 2 && i >= 0 && i < 2); entity = i; ++outputs; }
void Scr_Error(const char *s) { assert(s && *s); ++errors; }
void Scr_ObjectError(const char *s) { Scr_Error(s); }
/* The builtins report a bad argument through the VM's parameter-error path
 * and format the message with va. Both belong to the VM this harness does not
 * link, so they join the error stubs above and count the same way. */
void Scr_ParamError(int index,const char *s) { (void)index; Scr_Error(s); }
char *va(const char *fmt,...)
{
    static char text[1024];
    va_list argptr;
    va_start(argptr,fmt);
    vsnprintf(text,sizeof(text),fmt,argptr);
    va_end(argptr);
    return text;
}
void Scr_AddString(const char *s) { assert(strlen(s) < sizeof(string)); strcpy(string, s); ++outputs; }
void Scr_AddFloat(float f) { number = f; ++outputs; }
void Scr_AddInt(int i) { number = i; ++outputs; }
void Scr_AddVector(const float *v) { memcpy(vector, v, sizeof(vector)); ++outputs; }
/* waypoints/<map>_nodes.map override (script_path_nodes.c): the map and
 * zone type select it, and the filesystem serves the file when one is set. */
static const char *map_name = "mp_test", *zone_type = "mp", *override_text;
static unsigned reads, frees;
const char *ServerMapName(void) { return map_name; }
const char *ServerZoneType(void) { return zone_type; }
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{
    va_list argptr; int n;
    va_start(argptr, fmt); n = vsnprintf(dest, (size_t)size, fmt, argptr); va_end(argptr);
    return n;
}
int FS_ReadFile(const char *path, void **buffer)
{
    ++reads; *buffer = NULL;
    assert(strcmp(path, "waypoints/mp_test_nodes.map") == 0);
    if (!override_text) return -1;
    *buffer = strdup(override_text);
    return (int)strlen(override_text);
}
void FS_FreeFile(void *buffer) { assert(buffer); free(buffer); ++frees; }
void Com_Printf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
int main(void)
{
    InitPathNodes(); assert(resets == 2 && fields == 10 && cursor == 0);
    args[0] = "start"; args[1] = "targetname";
    Scr_GetNode(); assert(outputs == 1 && entity == 0);
    GetNodeField(2, 0, 5); assert(vector[0] == 1 && vector[1] == 2 && vector[2] == 3);
    GetNodeField(2, 0, 1); assert(strcmp(string, "end") == 0);
    GetNodeField(2, 0, 7); assert(number == 4);
    unsigned before = outputs;
    GetNodeField(2, 0, 3); assert(outputs == before);
    /* animscript is field 8 and is a real field the spawn record does not
     * carry, so it answers with nothing and reports nothing. Field 10 is past
     * the end of the table and is the error. */
    GetNodeField(2, 0, 8); assert(outputs == before && !errors);
    /* type is field 9: IW4's node.type, from the classname. */
    GetNodeField(2, 0, 9); assert(outputs == before + 1 && strcmp(string, "Path") == 0);
    GetNodeField(2, 1, 9); assert(strcmp(string, "Cover Left") == 0);
    before = outputs;
    GetNodeField(2, 0, 10); assert(outputs == before && errors == 1);
    assert(IsStoredPathNode("node_pathnode") && IsStoredPathNode("node_cover_left"));
    assert(!IsStoredPathNode("worldspawn") && !IsStoredPathNode("info_vehicle_node"));
    args[0] = "START"; Scr_GetNode(); assert(outputs == before);
    args[0] = "missing"; Scr_GetNode(); assert(outputs == before);
    Scr_GetNodeArray(); assert(count == 0);
    args[0] = "end"; args[1] = "target";
    Scr_GetNodeArray(); assert(count == 2);
    before = outputs; Scr_GetNode(); assert(outputs == before && errors == 2);
    Scr_GetAllPathNodes(); assert(count == 2);
    GetNodeField(2, 2, 0); assert(errors == 3);
    SetNodeField(2, 0, 0); assert(errors == 4);
    empty = 1; InitPathNodes(); Scr_GetAllPathNodes(); assert(count == 0);
    /* Without an override file every mp init asked for one and kept the level's set. */
    assert(reads == 2 && frees == 0);
    /* An mp override replaces the level's node_* records with the file's;
     * its non-node records are skipped. */
    empty = 0;
    override_text = "{ \"classname\" \"node_pathnode\" \"targetname\" \"a\" \"origin\" \"4 5 6\" }\n"
                    "{ \"classname\" \"info_player_start\" \"origin\" \"0 0 0\" }\n"
                    "{\n\"classname\" \"node_cover_right\"\n\"targetname\" \"b\"\n}\n";
    InitPathNodes(); assert(reads == 3 && frees == 1);
    Scr_GetAllPathNodes(); assert(count == 2);
    args[0] = "a"; args[1] = "targetname"; before = outputs;
    Scr_GetNode(); assert(outputs == before + 1 && entity == 0);
    GetNodeField(2, 0, 5); assert(vector[0] == 4 && vector[1] == 5 && vector[2] == 6);
    GetNodeField(2, 1, 9); assert(strcmp(string, "Cover Right") == 0);
    /* Co-op keeps the base level's SP node set and never reads the file. */
    zone_type = "coop";
    InitPathNodes(); assert(reads == 3);
    args[0] = "a"; before = outputs; Scr_GetNode(); assert(outputs == before);
    Scr_GetAllPathNodes(); assert(count == 2);
    override_text = NULL; zone_type = "mp";
    ClearPathNodes(); ClearPathNodes();
    puts("PASS: native path node ownership, class/fields, map filtering, lookup/misses, duplicate rejection, mp node-file override and reset");
}
