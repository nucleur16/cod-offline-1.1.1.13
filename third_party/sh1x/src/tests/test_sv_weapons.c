#include "sv_weapons.h"
#include "sv_state.h"
#include "weapon_native.h"
#include "usercmd.h"
#include "game.h"
#include "weapon_slots.h"
#include "weapon_compose.h"
#include "weapon_bits.h"
#include "playerstate_ext.h"
#include "qcommon_io.h"
#include "server.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
static int native_count=3;
static struct WeaponDef native_defs[3];
static unsigned broadcasts,broadcast_first,broadcast_count;
/* The fixture runs the production registry, server publication and engine
 * mapping over a synthetic retained-engine table. Only engine-owned services
 * and the unrelated composed-weapon/map-sun adapters are supplied here. */
cvar_t *ol_debugTrace;
serverStatic_t svs;
static gclient_t fixture_client;
const void *PlayerStateExtLiveBase(unsigned *count)
{if(count)*count=1;return &fixture_client;}
const void *PlayerStateExtSnapshotBase(unsigned *count)
{if(count)*count=0;return NULL;}
const void *PlayerStateExtArchiveBase(unsigned *count)
{if(count)*count=0;return NULL;}
unsigned int bg_numAmmoTypes=1, bg_numWeapClips=1, bg_numSharedAmmoCaps=1;
unsigned int bg_lastParsedWeaponIndex;
struct WeaponDef *bg_weapAmmoTypes[NATIVE_AMMO_POOL];
struct WeaponDef *bg_weapClips[NATIVE_AMMO_POOL];
struct WeaponDef *bg_sharedAmmoCaps[NATIVE_AMMO_POOL];
struct WeaponDef *bg_weaponDefs[WEAPON_SLOTS+1];
void Com_Printf(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
void Com_PrintWarning(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
int RefreshMapSunSampleSize(void){return 1;}
void ResetMapSunSampleSize(void){}
int RegisterComposedWeaponKeys(weapon_registry_t *r,unsigned slot,const char *name)
{(void)r;(void)slot;(void)name;assert(!"composed weapon outside this fixture");return 0;}
int ParseComposedWeaponName(const char *name,composed_weapon_t *out)
{(void)out;assert(name && name[0]!='*');return 0;}
int ComposedWeaponTail(const char *name,weapon_tail_t *out)
{(void)name;(void)out;assert(!"composed weapon outside this fixture");return 0;}
int LoadComposedWeapon(const char *name,const struct WeaponDef **out)
{(void)name;(void)out;assert(!"composed weapon outside this fixture");return 0;}
int BG_GetWeaponIndexForName(const char *name,void (*register_weapon)(unsigned))
{(void)name;(void)register_weapon;assert(!"native append outside this fixture");return 0;}
void G_RegisterWeapon(unsigned weapon)
{(void)weapon;assert(!"native append outside this fixture");}
void BroadcastWeaponConfigstrings(unsigned first,unsigned count)
{
    ++broadcasts; broadcast_first=first; broadcast_count=count;
    assert(WeaponToWire(2)==4);
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),5366),"m4_mp"));
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),5367),"gl_m4_mp"));
}
int BG_GetNumWeapons(void) { return native_count; }
struct WeaponDef *BG_GetWeaponDef(int index) { assert(index>=0 && index<3); return native_defs+index; }
/* Mounted facts are controlled here; actual mounted loading is covered by
 * test_weapon_loader. Configstring precedence and registry are real. */
int RegisterMountedWeaponKeys(weapon_registry_t *r,unsigned slot,const char *name)
{
    if(!strcmp(name,"scar_mp") || !strcmp(name,"scarsapr_mp"))
        return RegisterWeaponKeys(r,slot,name,"nato","scar_clip",1,0,-1);
    if(!strcmp(name,"none") || !strcmp(name,"defaultweapon_mp") ||
       !strcmp(name,"m4_mp") || !strcmp(name,"gl_m4_mp"))
        return RegisterWeaponKeys(r,slot,name,NULL,NULL,0,0,-1);
    return 0;
}
static int table_failure,partner_failure;
int LoadEngineStringTable(const char *name,const engine_table_t **out)
{
    static const char *cells[]={"scar_mp","scar_mp","scarsapr_mp"};
    static engine_table_t table={"mp/coWeaponAttachmentTable.csv",1,3,cells};
    assert(!strcmp(name,table.name)); if(table_failure) return -1;
    *out=&table; return 1;
}
int LoadMountedWeaponTail(const char *name,weapon_tail_t *out)
{
    if(partner_failure && !strcmp(name,"gl_m4_mp")) return -1;
    memset(out,0,sizeof(*out));
    out->count=1; out->entries[0].offset=0xb78; out->entries[0].kind=WEAPON_DEP_STRING;
    if(!strcmp(name,"scar_mp")) out->entries[0].name="scarsapr_mp";
    if(!strcmp(name,"scarsapr_mp")) out->entries[0].name="scar_mp"; /* Already registered cycle. */
    if(!strcmp(name,"m4_mp")) out->entries[0].name="gl_m4_mp";
    if(!strcmp(name,"gl_m4_mp")) out->entries[0].name="m4_mp";
    return 1;
}
int main(void)
{
    const char *cells[]={"5364","scar_mp","5365","scarsapr_mp"};
    const weapon_registry_t *r; int ammo=-99,clip=-99;
    assert(!RefreshServerWeaponRegistry() && !ServerWeaponRegistry());
    assert(SetServerInheritedConfigstrings("mp",2,cells));
    assert(RefreshServerWeaponRegistry()); r=ServerWeaponRegistry();
    assert(r && GetWeaponKeys(r,0,&ammo,&clip) && !ammo && !clip);
    assert(GetWeaponKeys(r,1,&ammo,&clip) && ammo==1 && clip==1);
    assert(GetWeaponKeys(r,3,&ammo,&clip) && ammo==2 && clip==2);
    native_defs[0].szInternalName="none"; native_defs[1].szInternalName="scarsapr_mp";
    native_defs[2].szInternalName="scar_mp";
    assert(RefreshServerWeaponMappings());
    assert(WeaponToWire(1)==3 && WeaponToEngine(3)==1);
    assert(WeaponToWire(2)==2 && WeaponToEngine(2)==2 && WeaponToEngine(1)==-1);
    {
        ol_usercmd_t wire={0}; usercmd_t command;
        wire.serverTime=1234; wire.weapon=3; wire.offHandIndex=2; wire.direction=1;
        ToEngineUsercmd(&wire,&command,0,0);
        assert(command.weapon==1 && command.offHandIndex==2 && command.serverTime==1234 && command.forwardmove==127);
        /* Same definition count, different native registration order: a count
         * cache would miss this change. The next input uses the new mapping. */
        native_defs[1].szInternalName="scar_mp"; native_defs[2].szInternalName="scarsapr_mp";
        assert(RefreshServerWeaponMappings());
        ToEngineUsercmd(&wire,&command,0,0);
        assert(command.weapon==2 && command.offHandIndex==1);
        wire.weapon=699; ToEngineUsercmd(&wire,&command,2,0);
        assert(command.weapon==2); /* Unloaded client ID retains current weapon. */
        native_defs[1].szInternalName="scarsapr_mp"; native_defs[2].szInternalName="scar_mp";
        assert(RefreshServerWeaponMappings());
    }
    native_defs[2].szInternalName="unpublished";
    assert(!RefreshServerWeaponMappings());
    assert(WeaponToEngine(3)==1 && WeaponToWire(2)==2);
    native_defs[2].szInternalName="scarsapr_mp";
    assert(!RefreshServerWeaponMappings() && WeaponToWire(2)==2);
    native_defs[2].szInternalName="scar_mp"; native_count=WEAPON_SLOTS+1;
    assert(!RefreshServerWeaponMappings() && WeaponToEngine(3)==1);
    native_count=1; assert(RefreshServerWeaponMappings());
    assert(WeaponToWire(1)==-1 && WeaponToEngine(3)==-1);
    assert(OL_SetConfigstring(ServerConfigstrings(),5364,"unresolved"));
    assert(!RefreshServerWeaponRegistry() && !ServerWeaponRegistry());
    assert(OL_SetConfigstring(ServerConfigstrings(),5364,""));
    assert(!RefreshServerWeaponRegistry() && !ServerWeaponRegistry());
    assert(OL_SetConfigstring(ServerConfigstrings(),5364,"scar_mp"));
    assert(RefreshServerWeaponRegistry());
    assert(OL_SetConfigstring(ServerConfigstrings(),5365,""));
    assert(RefreshServerWeaponRegistry());
    r=ServerWeaponRegistry(); ammo=clip=-99;
    assert(!GetWeaponKeys(r,3,&ammo,&clip) && ammo==-99 && clip==-99);
    assert(OL_SetConfigstring(ServerConfigstrings(),5363,"scar_mp"));
    assert(!RefreshServerWeaponRegistry() && !ServerWeaponRegistry());
    assert(OL_SetConfigstring(ServerConfigstrings(),42,"unrelated"));
    assert(InitializeServerWeaponConfigstrings("mp"));
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),5364),"scar_mp"));
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),5365),"scarsapr_mp"));
    assert(!*GetConfigstring(ServerConfigstrings(),5439)); /* Stale inherited tail suppressed. */
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),42),"unrelated"));
    assert(RefreshServerWeaponRegistry());
    table_failure=1;
    native_count=3; native_defs[2].szInternalName="m4_mp";
    partner_failure=1; r=ServerWeaponRegistry();
    assert(!RefreshServerWeaponMappings() && ServerWeaponRegistry()==r && !broadcasts);
    assert(!*GetConfigstring(ServerConfigstrings(),5366));
    assert(WeaponToWire(2)==-1);
    partner_failure=0;
    assert(RefreshServerWeaponMappings());
    assert(broadcasts==1 && broadcast_first==4 && broadcast_count==2);
    assert(WeaponToWire(1)==3 && WeaponToWire(2)==4);
    assert(GetWeaponKeys(ServerWeaponRegistry(),5,&ammo,&clip) && ammo==4 && clip==4);
    assert(RefreshServerWeaponMappings() && broadcasts==1);
    native_defs[2].szInternalName="unknown_stock";
    r=ServerWeaponRegistry();
    assert(!RefreshServerWeaponMappings() && ServerWeaponRegistry()==r);
    assert(WeaponToWire(2)==4 && broadcasts==1);
    assert(!*GetConfigstring(ServerConfigstrings(),5368));
    native_defs[2].szInternalName="m4_mp";
    {
        playerState_t *ps=&fixture_client.ps;
        engine_player_bindings_t binding={0},before;
        binding.alt_weapon=73;
        native_defs[1].iAmmoIndex=7; native_defs[1].iClipIndex=9;
        native_defs[2].iAmmoIndex=8; native_defs[2].iClipIndex=10;
        WeaponBitsOwnedSet(ps,1,1); WeaponBitsOwnedSet(ps,2,1);
        ps->ammo[7]=90; ps->ammo[8]=75;
        ps->ammoclip[9]=30; ps->ammoclip[10]=25;
        assert(BuildServerAmmoBindings(&binding,ps));
        assert(binding.alt_weapon==73 && binding.clip_mask==0x7fff && binding.reserve_mask==0x7fff);
        assert(binding.clips[0].engine_index==9 && binding.clips[0].wire_key==2);
        assert(binding.clips[1].engine_index==10 && binding.clips[1].wire_key==3);
        assert(binding.reserves[0].engine_index==7 && binding.reserves[0].wire_key==2);
        {
            player_array_updates_t updates={0};
            assert(OverlayEngineAmmo(&updates,ps,binding.clip_mask,binding.clips,
                binding.reserve_mask,binding.reserves));
            assert(updates.clip_ammo[0][0]==30 && updates.clip_ammo[1][0]==25);
            assert(updates.reserve_ammo[0]==90 && updates.reserve_ammo[1]==75);
        }
        before=binding; WeaponBitsOwnedSet(ps,3,1); /* Owned but not loaded. */
        /* Left unbound; the frame keeps its other records. */
        assert(BuildServerAmmoBindings(&binding,ps) && !memcmp(&binding,&before,sizeof(binding)));
        WeaponBitsInventoryClear(ps);
        assert(BuildServerAmmoBindings(&binding,ps));
        assert(binding.clips[0].engine_index==-1 && binding.reserves[0].engine_index==-1);
    }
    assert(!InitializeServerWeaponConfigstrings("mp") && !ServerWeaponRegistry());
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),5364),"scar_mp"));
    ResetServerWeaponRegistry(); ResetServerConfigstrings();
    assert(!ServerWeaponRegistry());
    puts("PASS: server weapon registry follows inherited/override names, rejects gaps and invalidates failed refreshes");
    return 0;
}
