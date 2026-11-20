#ifdef OL_SERVER
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "scr_vm_functions.h"
#include "ol_profile.h"
#include "script_profile_vm.h"
#include "ol_stats.h"
#include "cvar.h"
#include "owned_items.h"
#include "connect.h"
/* <player> isItemUnlocked( <item> )
 *
 * IW4 method 0x5f9960 (table entry 0x79afc0). The requirement columns come
 * from the mounted CoDOL tables and the player's own secure profile; the
 * definition set and the profile blob are the ones the playerdata script
 * accessors observe, so a script write made this session is read back here.
 *
 * There is no unlocked-by-default answer for a missing or unreadable profile,
 * table or requirement: each of those is a script error. The single true
 * result this returns without proving a requirement is for an item that
 * mp/unlocktable.csv does not list at all, which is what IW4 itself answers
 * and what ProfileItemUnlocked documents. */
/* Resolve one item for a player. On success returns 1 with *value set; on
 * failure returns 0 with *reason naming what could not be established. The
 * two script methods below differ only in what they do with a failure. */
static int item_unlocked(scr_entref_t ref,const char *item,int *value,const char **reason)
{
    const structured_schema_t *schema;const uint8_t *profile;
    const engine_table_t *unlocks=NULL,*ranks=NULL,*challenges=NULL;
    int status;cvar_t *root;
    schema=ScriptProfileSchema(PROFILE_SECURE);
    profile=ScriptProfileBlob(ref.entnum,PROFILE_SECURE);
    if(!schema){*reason="isItemUnlocked requires the mounted mp/playersecuredata.def";return 0;}
    if(!profile){*reason="isItemUnlocked requires a server-bound secure profile";return 0;}
    status=LoadEngineStringTable("mp/unlocktable.csv",&unlocks);
    root=Cvar_FindVar("scriptPath");
    if(!status && root)status=LoadReferenceStringTable(root->string,"mp/unlocktable.csv",&unlocks);
    if(status!=1 || LoadEngineStringTable("mp/rankTable.csv",&ranks)!=1 ||
       LoadEngineStringTable("mp/allChallengesTable.csv",&challenges)!=1) {
        *reason="isItemUnlocked requires the mounted unlock, rank and challenge tables";return 0;
    }
    if(!ProfileItemUnlocked(profile,STATS_SIZE,schema,unlocks,ranks,challenges,item,value)) {
        *reason="isItemUnlocked: this profile does not satisfy the table's requirement semantics";return 0;
    }
    return 1;
}
static const char *player_item_param(scr_entref_t ref,const char *method)
{
    const char *item;
    if(ref.classnum || ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client ||
       !g_entities[ref.entnum].r.inuse) {Scr_Error(va("%s: entity must be a player entity",method));return NULL;}
    if(Scr_GetNumParam()!=1){Scr_Error(va("%s requires one item name",method));return NULL;}
    item=Scr_GetString(0);
    if(!item || !*item){Scr_ParamError(0,va("%s requires one item name",method));return NULL;}
    return item;
}
void Scr_IsItemUnlocked(scr_entref_t ref)
{
    const char *item,*reason=NULL;int value;
    if(!(item=player_item_param(ref,"isItemUnlocked")))return;
    if(!item_unlocked(ref,item,&value,&reason)){Scr_Error(reason);return;}
    Scr_AddInt(value);
}
/* <player> codol_tryIsItemUnlocked( <item> )
 *
 * isItemUnlocked without the script error: the same resolution, but an item
 * whose requirement cannot be established returns undefined (and says why on
 * the console) instead of stopping the VM. A loadout asks this so that one
 * bad unlock row cannot abort a spawn; the caller decides what undefined
 * means. Argument errors (not a player, no item) are still script errors. */
void Scr_TryIsItemUnlocked(scr_entref_t ref)
{
    const char *item,*reason=NULL;int value;
    if(!(item=player_item_param(ref,"codol_tryIsItemUnlocked")))return;
    if(!item_unlocked(ref,item,&value,&reason)) {
        Com_Printf(CON_CHANNEL_SCRIPT,"^3codol_tryIsItemUnlocked(%s) for client %d: %s\n",item,ref.entnum,reason);
        Scr_AddUndefined();
        return;
    }
    Scr_AddInt(value);
}
/* <player> codol_tryChallengeState( <challenge ref> )
 *
 * The derived state of one challenge for this player: one plus the number of
 * mp/allChallengesTable.csv targets the stored progress (secure profile
 * challenges[<column 4 id>]) has reached -- the value C1's unlock flags and
 * IW4's unlock rules compare against (a tier N gate is met when state > N).
 * Returns undefined, with the reason on the console, when the player has no
 * bound secure profile or the challenge cannot be resolved; it never stops
 * the VM, because a loadout asks it in the middle of a spawn. */
void Scr_TryChallengeState(scr_entref_t ref)
{
    const structured_schema_t *schema;const uint8_t *profile;
    const engine_table_t *challenges=NULL;const char *ref_name,*reason=NULL;int state;
    if(!(ref_name=player_item_param(ref,"codol_tryChallengeState")))return;
    schema=ScriptProfileSchema(PROFILE_SECURE);
    profile=ScriptProfileBlob(ref.entnum,PROFILE_SECURE);
    if(!schema)reason="requires the mounted mp/playersecuredata.def";
    else if(!profile)reason="requires a server-bound secure profile";
    else if(LoadEngineStringTable("mp/allChallengesTable.csv",&challenges)!=1)
        reason="requires the mounted challenge table";
    else if(!ProfileChallengeState(profile,STATS_SIZE,schema,challenges,ref_name,&state))
        reason="the challenge has no stored progress or targets";
    if(reason) {
        Com_Printf(CON_CHANNEL_SCRIPT,"^3codol_tryChallengeState(%s) for client %d: %s\n",ref_name,ref.entnum,reason);
        Scr_AddUndefined();
        return;
    }
    Scr_AddInt(state);
}
/* <player> codol_tryOwnsItem( <marketplace item name> )
 *
 * Whether this player has BOUGHT the item in the lobby store: 1 or 0, from
 * the purchase manifest the lobby exports beside the player's mpdata_s in
 * profilePath (owned_items.h). The name is resolved to its id
 * through mp/coMarketplaceItems.csv column 1 -> column 0, the last matching
 * row winning as in C1's own lookup (0x007286E0). Returns undefined, with the
 * reason on the console, when there is no profile directory, no connect
 * identity, the name is not a marketplace item, or the manifest cannot be
 * read; it never stops the VM, because a loadout asks it during a spawn.
 * No manifest at all is 0: the lobby has recorded no purchase. */
static int marketplace_item_id(const char *name,unsigned *id)
{
    const engine_table_t *market=NULL;int r,found=-1;
    if(LoadEngineStringTable("mp/coMarketplaceItems.csv",&market)!=1 || !market ||
       market->rowCount<0 || market->columnCount<2 || (market->rowCount && !market->values))return 0;
    for(r=0;r<market->rowCount;++r) {
        const char *const *cells=market->values+(size_t)r*market->columnCount;
        if(cells[0] && cells[1] && !Q_stricmp(cells[1],name))found=r;
    }
    if(found<0)return 0;
    {
        const char *cell=market->values[(size_t)found*market->columnCount];char *end;unsigned long v;
        v=strtoul(cell,&end,10);
        if(end==cell || *end || !v || v>0xffffu)return 0;
        *id=(unsigned)v;
    }
    return 1;
}
void Scr_TryOwnsItem(scr_entref_t ref)
{
    const char *item,*reason=NULL;cvar_t *directory;uint64_t uid;unsigned id=0;int owned=-1;
    if(!(item=player_item_param(ref,"codol_tryOwnsItem")))return;
    directory=Cvar_FindVar("profilePath");
    uid=ClientConnectIdentity(ref.entnum);
    if(!directory || !directory->string || !*directory->string)reason="profilePath is empty";
    else if(!uid)reason="the client has no connect identity";
    else if(!marketplace_item_id(item,&id))reason="not an mp/coMarketplaceItems.csv item";
    else if((owned=ReadOwnedItem(directory->string,uid,id,(uint32_t)time(NULL)))<0)
        reason="the lobby's owned_items manifest is unreadable";
    if(reason) {
        Com_Printf(CON_CHANNEL_SCRIPT,"^3codol_tryOwnsItem(%s) for client %d: %s\n",item,ref.entnum,reason);
        Scr_AddUndefined();
        return;
    }
    Scr_AddInt(owned);
}
#endif
