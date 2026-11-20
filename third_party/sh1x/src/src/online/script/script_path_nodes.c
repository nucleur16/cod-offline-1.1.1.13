#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "cscr_variable.h"
#include "g_public.h"
#include "filesystem.h"
#include "qcommon_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Native VM class 2 already exists. Keep records separate from g_entities;
 * path nodes are script objects, not networked game entities. Connectivity
 * is not inferred from positions or fabricated for navigation queries. */
#define NODE_CLASS 2
/* C1 node spawn 0x6f96bd rejects the next node at count 0x2000. */
#define NODE_LIMIT 8192
static SpawnVar *nodes[NODE_LIMIT];
static unsigned node_count;
static int initialized;
static const struct { const char *name; unsigned type; } fields[] = {
    {"targetname", VAR_STRING}, {"target", VAR_STRING},
    {"script_linkname", VAR_STRING}, {"script_noteworthy", VAR_STRING},
    {"classname", VAR_STRING}, {"origin", VAR_VECTOR}, {"angles", VAR_VECTOR},
    {"spawnflags", VAR_INTEGER},
    /* C1 field table 0xcccf08: short script-string at node+0x0e, generic
     * getter (field type 5). The owned spawn record retains its source value. */
    {"animscript", VAR_STRING},
    /* IW4 node.type: the node class as the animscripts compare it ("Cover
     * Left", "Path", ...), derived from the classname (PathNodeTypeName). */
    {"type", VAR_STRING}
};
#define NODE_FIELD_TYPE 9

/* IW4 pathnode type names (animscripts\init_move_transitions anim.approach_types,
 * cover_arrival, corner.gsc) for the node_* classnames the levels carry. */
static const struct { const char *classname, *iw4; } node_type_names[] = {
    {"node_cover_left", "Cover Left"}, {"node_cover_right", "Cover Right"},
    {"node_cover_stand", "Cover Stand"}, {"node_cover_crouch", "Cover Crouch"},
    {"node_cover_crouch_window", "Cover Crouch Window"}, {"node_cover_prone", "Cover Prone"},
    {"node_concealment_stand", "Conceal Stand"}, {"node_concealment_crouch", "Conceal Crouch"},
    {"node_concealment_prone", "Conceal Prone"}, {"node_pathnode", "Path"},
    {"node_exposed", "Exposed"}, {"node_guard", "Guard"}, {"node_ambush", "Ambush"},
    {"node_turret", "Turret"}, {"node_scripted", "Scripted"}, {"node_door", "Door"},
    {"node_negotiation_begin", "Begin"}, {"node_negotiation_end", "End"},
    {"node_cover_watcher", "Cover Watcher"}
};
const char *PathNodeTypeName(const char *classname)
{
    unsigned i;
    if (!classname) return "Path";
    for (i = 0; i < sizeof(node_type_names)/sizeof(*node_type_names); ++i)
        if (!Q_stricmp(classname, node_type_names[i].classname)) return node_type_names[i].iw4;
    return "Path";
}

static const char *value(const SpawnVar *record, const char *key)
{
    int i;
    for (i = 0; i < record->numSpawnVars; ++i)
        if (!Q_stricmp(record->spawnVars[i].key, key)) return record->spawnVars[i].value;
    return NULL;
}
void ClearPathNodes(void)
{
    unsigned i;
    for (i = 0; i < node_count; ++i) { free(nodes[i]); nodes[i] = NULL; }
    node_count = 0; initialized = 0;
}
/* Server-side node sets for mp maps (waypoints/<map>_nodes.map, shipped in
 * the server pack beside the waypoint CSVs).  CoDOL 3.3.3.3 ships the same
 * stock mp levels with a reworked AI node set: mp_afghan 1871 node_*
 * records where 1.1.1.13's zone carries 1200, mp_killhouse_sh 172 MP-area
 * path nodes where 1.1.1.13 still carries 732 SP-scene ones -- on geometry
 * that matches (the spawn points coincide; tools/map_nodes_333.py
 * extracts and checks them).  The zones stay untouched: when the file exists
 * its node_* records replace the level's own, in mp only (coop actors keep
 * the base level's SP set), before the graph builder and the scripts see
 * them.  The format is the entity string's: { "key" "value" ... } blocks. */
const char *ServerZoneType(void);
const char *ServerMapName(void);
static int override_token(const char **p, char *out, int size)
{
    const char *s = *p;
    int n = 0;
    while (*s && *s != '"' && *s != '{' && *s != '}') ++s;
    if (*s != '"') { *p = s; return 0; }
    ++s;
    while (*s && *s != '"') { if (n < size - 1) out[n++] = *s; ++s; }
    if (*s == '"') ++s;
    out[n] = 0; *p = s;
    return 1;
}
static void load_node_override(void)
{
    const char *map = ServerMapName(), *zone = ServerZoneType(), *p;
    char path[MAX_QPATH], key[256], val[1024];
    void *buffer = NULL;
    unsigned replaced = node_count, loaded = 0;
    int size;
    if (!map || !zone || strcmp(zone, "mp")) return;
    Com_sprintf(path, sizeof(path), "waypoints/%s_nodes.map", map);
    size = FS_ReadFile(path, &buffer);
    if (size <= 0 || !buffer) { if (buffer) FS_FreeFile(buffer); return; }
    ClearPathNodes();
    p = buffer;
    for (;;) {
        SpawnVar *record;
        while (*p && *p != '{') ++p;
        if (!*p) break;
        ++p;
        if (node_count == NODE_LIMIT || !(record = calloc(1, sizeof(*record)))) break;
        for (;;) {
            int klen, vlen;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
            if (!*p || *p == '}') { if (*p) ++p; break; }
            if (!override_token(&p, key, sizeof(key)) || !override_token(&p, val, sizeof(val))) {
                while (*p && *p != '}') ++p;
                continue;
            }
            klen = (int)strlen(key) + 1; vlen = (int)strlen(val) + 1;
            if (record->numSpawnVars == 64 || record->numSpawnVarChars + klen + vlen > (int)sizeof(record->spawnVarChars)) continue;
            memcpy(record->spawnVarChars + record->numSpawnVarChars, key, klen);
            record->spawnVars[record->numSpawnVars].key = record->spawnVarChars + record->numSpawnVarChars;
            record->numSpawnVarChars += klen;
            memcpy(record->spawnVarChars + record->numSpawnVarChars, val, vlen);
            record->spawnVars[record->numSpawnVars].value = record->spawnVarChars + record->numSpawnVarChars;
            record->numSpawnVarChars += vlen;
            record->numSpawnVars++;
        }
        record->spawnVarsValid = 1;
        if (!value(record, "classname") || strncmp(value(record, "classname"), "node_", 5)) { free(record); continue; }
        nodes[node_count++] = record;
        ++loaded;
    }
    FS_FreeFile(buffer);
    initialized = 1;
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL path nodes: %s replaces the level's %u node_* records with %u\n",
        path, replaced, loaded);
}
void InitPathNodes(void)
{
    SpawnVar parsed;
    unsigned i;
    ClearPathNodes();
    for (i = 0; i < sizeof(fields)/sizeof(*fields); ++i)
        Scr_AddClassField(NODE_CLASS, fields[i].name, i);
    G_ResetEntityParsePoint();
    while (G_ParseSpawnVars(&parsed)) {
        const char *classname = value(&parsed, "classname");
        SpawnVar *copy;
        int pair;
        if (!classname || strncmp(classname, "node_", 5)) continue;
        if (node_count == NODE_LIMIT || !(copy = malloc(sizeof(*copy)))) {
            G_ResetEntityParsePoint(); ClearPathNodes();
            Scr_Error("Could not allocate native path nodes"); return;
        }
        memcpy(copy, &parsed, sizeof(*copy));
        for (pair = 0; pair < copy->numSpawnVars; ++pair) {
            copy->spawnVars[pair].key = copy->spawnVarChars +
                (parsed.spawnVars[pair].key - parsed.spawnVarChars);
            copy->spawnVars[pair].value = copy->spawnVarChars +
                (parsed.spawnVars[pair].value - parsed.spawnVarChars);
        }
        nodes[node_count++] = copy;
    }
    /* The normal entity spawn pass must still read the entire map. */
    G_ResetEntityParsePoint();
    initialized = 1;
    load_node_override();
}

int IsStoredPathNode(const char *classname)
{
    if (!classname || strncmp(classname, "node_", 5)) return 0;
    if (!initialized) {
        Com_Error(ERR_DROP, "Path nodes must be initialized before entity spawning");
        return 0;
    }
    return 1;
}

static void lookup(int array, int all)
{
    const char *wanted = NULL, *key = NULL;
    unsigned i;
    int found = -1;
    if (!all) {
        /* C1 resolves the lookup key against its native class-field table,
         * not the arbitrary key/value pairs in the map entity string.
         * GetNode 0x6f9928/0x6f9943 returns undefined for unknown keys;
         * GetNodeArray 0x6f9ae1/0x6f9afc reports parameter 1 instead.
         * Both reject native non-string fields before searching any nodes. */
        static const char *const string_keys[] = {
            "targetname", "target", "animscript", "script_linkname",
            "script_noteworthy"
        };
        static const char *const other_keys[] = {
            "origin", "angles", "radius", "minusedistsq", "spawnflags", "type"
        };
        int recognized = 0;
        wanted = Scr_GetString(0);
        key = Scr_GetString(1);
        for (i = 0; i < sizeof(string_keys)/sizeof(*string_keys); ++i)
            if (!strcmp(key, string_keys[i])) { recognized = 1; break; }
        if (!recognized) {
            for (i = 0; i < sizeof(other_keys)/sizeof(*other_keys); ++i)
                if (!strcmp(key, other_keys[i])) {
                    Scr_ParamError(1, "key is not internally a string");
                    return;
                }
            if (array)
                Scr_ParamError(1, va("key '%s' does not internally belong to nodes", key));
            return;
        }
    }
    if (array) Scr_MakeArray();
    for (i = 0; i < node_count; ++i) {
        const char *candidate = all ? NULL : value(nodes[i], key);
        if (!all && (!candidate || strcmp(candidate, wanted))) continue;
        if (array) { Scr_AddEntityNum(i, NODE_CLASS); Scr_AddArray(); }
        else {
            if (found >= 0) { Scr_Error("getnode matches more than one node"); return; }
            found = (int)i;
        }
    }
    if (!array && found >= 0) Scr_AddEntityNum(found, NODE_CLASS);
    /* No return value is the VM's undefined value on a genuine miss. */
}
void Scr_GetNode(void) { lookup(0, 0); }
void Scr_GetNodeArray(void) { lookup(1, 0); }
void Scr_GetAllPathNodes(void) { lookup(1, 1); }

void GetNodeField(unsigned classnum, int index, int field)
{
    const char *text;
    float vector[3];
    double number;
    char *end;
    if (classnum != NODE_CLASS || index < 0 || (unsigned)index >= node_count ||
        field < 0 || (unsigned)field >= sizeof(fields)/sizeof(*fields)) {
        Scr_ObjectError("Invalid path node field"); return;
    }
    if (field == NODE_FIELD_TYPE) { Scr_AddString(PathNodeTypeName(value(nodes[index], "classname"))); return; }
    text = value(nodes[index], fields[field].name);
    if (!text) return;
    if (fields[field].type == VAR_STRING) { Scr_AddString(text); return; }
    if (fields[field].type == VAR_VECTOR) {
        char extra;
        if (sscanf(text, "%f %f %f %c", &vector[0], &vector[1], &vector[2], &extra) != 3 ||
            !isfinite(vector[0]) || !isfinite(vector[1]) || !isfinite(vector[2])) {
            Scr_Error("Malformed path node vector"); return;
        }
        Scr_AddVector(vector); return;
    }
    number = strtod(text, &end);
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (end == text || *end || !isfinite(number)) { Scr_Error("Malformed path node number"); return; }
    if (fields[field].type == VAR_INTEGER) {
        if (number < -2147483648.0 || number > 2147483647.0 || floor(number) != number) {
            Scr_Error("Invalid path node integer"); return;
        }
        Scr_AddInt((int)number);
    } else {
        float f = (float)number;
        if (!isfinite(f)) { Scr_Error("Path node number out of range"); return; }
        Scr_AddFloat(f);
    }
}
int SetNodeField(unsigned classnum, int index, int field)
{
    /* C1 class-2 assignment 0x56b6ce raises the named read-only error
     * (string 0xc7a61c). T5 Scr_SetPathnodeField 0x921e80 validates node/field then calls
     * 0x921ef0, which unconditionally raises the named read-only error
     * (string 0xd37aa0). Built-in node fields are not dynamic GSC fields;
     * GScr_SetDynamicPathnodeField has a separate path at 0x922070. */
    if (classnum != NODE_CLASS || index < 0 || (unsigned)index >= node_count ||
        field < 0 || (unsigned)field >= sizeof(fields)/sizeof(*fields)) {
        Scr_ObjectError("Invalid path node field"); return 1;
    }
    Scr_Error(va("pathnode property '%s' is read-only", fields[field].name));
    return 1;
}
/* Native actor goal consumer: return the same stored class-2 origin as the
 * VM getter, without touching VM arguments or manufacturing BSP links. */
int PathNodeOrigin(unsigned index, float *origin)
{
    const char *text;
    char extra;
    if (!initialized || index >= node_count || !origin) return 0;
    text = value(nodes[index], "origin");
    return text && sscanf(text, "%f %f %f %c", &origin[0], &origin[1],
        &origin[2], &extra) == 3 && isfinite(origin[0]) &&
        isfinite(origin[1]) && isfinite(origin[2]);
}
/* Read-only census for the native graph builder (bot_nav.c): how many
 * node_* records the entity string carried and one raw key of each. */
unsigned PathNodeCount(void) { return initialized ? node_count : 0; }
const char *PathNodeValue(unsigned index, const char *key)
{
    return initialized && index < node_count && key ? value(nodes[index], key) : NULL;
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}
int PersistentTailPathNodesCanRelease(void){
 if(node_count>NODE_LIMIT||(initialized!=0&&initialized!=1))return 0;
 for(unsigned i=0;i<NODE_LIMIT;++i){if(i>=node_count){if(nodes[i])return 0;continue;}if(!nodes[i])return 0;
 for(unsigned j=0;j<i;++j)if(nodes[i]==nodes[j])return 0;}return 1;}

int PersistentTailPathNodesEmpty(void){return !node_count&&!initialized&&resident_tail_zero(nodes,sizeof(nodes));}
void PersistentTailPathNodesReset(void){ClearPathNodes();}
#endif
