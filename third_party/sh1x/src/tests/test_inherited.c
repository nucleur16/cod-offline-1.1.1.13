#include "configstrings.h"
#include "sv_state.h"
#include "notetrack_effects.h"
#include "const_models.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int not_mud_dark(const char *name, void *context)
{ (void)context; return strcmp(name, "impacts/footstep_mud_dark") != 0; }
/* The coop baseline's footstep rows (CS 6162/6168) and effect slots 37/38. */
static void notetrack_effects(void)
{
    configstrings_t inherited={0},overrides={0};
    const char *cells[]={"3688","impacts/footstep_water","3689","impacts/footstep_mud_dark",
        "6134","land;-1;;0;land_;;","6162","footstep_left_large;14;j_ball_le;38;step_run_;;",
        "6168","footstep_left_large;20;j_ball_le;37;step_run_;;","6170","bad;row"};
    assert(ImportConstantConfigstrings(&inherited,6,cells));
    assert(DisableUnavailableNotetrackEffects(NULL,&inherited,not_mud_dark,NULL,NULL)==-1);
    assert(DisableUnavailableNotetrackEffects(&overrides,&inherited,not_mud_dark,NULL,NULL)==1);
    assert(overrides.count==1 && !strcmp(overrides.values[6162],"footstep_left_large;14;j_ball_le;0;step_run_;;"));
    /* Idempotent: the rewritten row names effect 0 and is left alone. */
    assert(DisableUnavailableNotetrackEffects(&overrides,&inherited,not_mud_dark,NULL,NULL)==0);
    ClearConfigstrings(&overrides); ClearConfigstrings(&inherited);
}
/* The zm models C1's constant table names but no zm zone ships. */
static int zm_zone_model(const char *name, void *context)
{
    (void)context;
    return !strcmp(name,"zombie_vending_speedcola") || !strcmp(name,"codo_treasure_chest_box2") ||
        !strcmp(name,"codo_treasure_chest_box2_lid") || !strcmp(name,"zombie_vending_jugg");
}
static void const_models(void)
{
    configstrings_t inherited={0},overrides={0};
    const char *cells[]={"1440","zombie_vending_jugg","1442","zombie_vending_sleight",
        "1444","codo_treasure_chest_box","1445","codo_treasure_chest_box_lid"};
    assert(ImportConstantConfigstrings(&inherited,4,cells));
    assert(SubstituteConstModels(NULL,&inherited,"zm",zm_zone_model,NULL,NULL)==-1);
    /* Other categories have no rows. */
    assert(SubstituteConstModels(&overrides,&inherited,"mp",zm_zone_model,NULL,NULL)==0 && !overrides.count);
    assert(SubstituteConstModels(&overrides,&inherited,"zm",zm_zone_model,NULL,NULL)==3);
    assert(overrides.count==3 && !overrides.values[1440]);
    assert(!strcmp(overrides.values[1442],"zombie_vending_speedcola"));
    assert(!strcmp(overrides.values[1444],"codo_treasure_chest_box2"));
    assert(!strcmp(overrides.values[1445],"codo_treasure_chest_box2_lid"));
    /* A map entity naming the shipped model reuses the overridden slot. */
    assert(RegisterInheritedModelName(&overrides,&inherited,"codo_treasure_chest_box2")==129 &&
        overrides.count==3);
    assert(SubstituteConstModels(&overrides,&inherited,"zm",zm_zone_model,NULL,NULL)==0);
    ClearConfigstrings(&overrides); ClearConfigstrings(&inherited);
}
/* zm_asylum_sh: the constant table's zm_prototype_sh rows, as that zone set
 * ships them (2026-10-13 session 11 parity log, CS 1441 and 3771). */
static int asylum_zone_asset(const char *name, void *context)
{
    (void)context;
    return !strcmp(name,"zombie_power_lever_handle_arm02") ||
        !strcmp(name,"destructibles/fx_barrelexp") ||
        !strcmp(name,"maps/zombie/fx_zomb_moon_proto_asylum");
}
static int prototype_zone_asset(const char *name, void *context)
{
    (void)context;
    return !strcmp(name,"zombie_power_lever_handle_arm01") || !strcmp(name,"props/barrelexp") ||
        !strcmp(name,"zombie_power_lever_handle_arm02") || !strcmp(name,"destructibles/fx_barrelexp");
}
static void const_effects(void)
{
    configstrings_t inherited={0},overrides={0};
    const char *cells[]={"1441","zombie_power_lever_handle_arm01","1447","vehicle_coupe_white_destructible",
        "3771","props/barrelexp","3775","maps/zombie/fx_zomb_moon_proto","3724",
        "fire/bigcity_destroyed_building_fire_smoke"};
    assert(ImportConstantConfigstrings(&inherited,5,cells));
    /* zm_prototype_sh ships its own rows: nothing changes. */
    assert(SubstituteConstModels(&overrides,&inherited,"zm",prototype_zone_asset,NULL,NULL)==0);
    assert(SubstituteConstEffects(&overrides,&inherited,"zm",prototype_zone_asset,NULL,NULL)==0);
    assert(!overrides.count);
    /* zm_asylum_sh: the lever and both effects map, the coupe and the
     * bigcity fire (nothing equivalent ships) stay. */
    assert(SubstituteConstModels(&overrides,&inherited,"zm",asylum_zone_asset,NULL,NULL)==1);
    assert(SubstituteConstEffects(&overrides,&inherited,"zm",asylum_zone_asset,NULL,NULL)==2);
    assert(!strcmp(overrides.values[1441],"zombie_power_lever_handle_arm02"));
    assert(!strcmp(overrides.values[3771],"destructibles/fx_barrelexp"));
    assert(!strcmp(overrides.values[3775],"maps/zombie/fx_zomb_moon_proto_asylum"));
    assert(!overrides.values[1447] && !overrides.values[3724]);
    /* A script loading the constant name reuses the substituted slot (3771 =
     * effects + 120), and so does one naming the shipped effect. */
    assert(RegisterEffectName(&overrides,&inherited,"props/barrelexp")==120 && overrides.count==3);
    assert(RegisterEffectName(&overrides,&inherited,"destructibles/fx_barrelexp")==120);
    assert(RegisterInheritedModelName(&overrides,&inherited,"zombie_power_lever_handle_arm01")==126);
    ClearConfigstrings(&overrides); ClearConfigstrings(&inherited);
}
/* Coop: mp/constConfigStrings_coop.csv was captured on the training map;
 * coop_take_mp_fav_tropical ships some of its rows under other names
 * (2026-10-13 session 13 parity log, CS 1999 and 3659). */
static int fav_zone_asset(const char *name, void *context)
{
    (void)context;
    return !strcmp(name,"weapon_m4") || !strcmp(name,"explosions/aerial_explosion_large");
}
static int training_zone_asset(const char *name, void *context)
{
    (void)context;
    return !strcmp(name,"codo_weapon_m4a1tech_world") || !strcmp(name,"weapon_m4") ||
        !strcmp(name,"explosions/helicopter_explosion") ||
        !strcmp(name,"explosions/aerial_explosion_large");
}
static void const_coop(void)
{
    configstrings_t inherited={0},overrides={0};
    const char *cells[]={"1999","codo_weapon_m4a1tech_world","1983","weapon_minigun",
        "3659","explosions/helicopter_explosion"};
    assert(ImportConstantConfigstrings(&inherited,3,cells));
    /* The training map ships its own rows: nothing changes. */
    assert(SubstituteConstModels(&overrides,&inherited,"coop",training_zone_asset,NULL,NULL)==0);
    assert(SubstituteConstEffects(&overrides,&inherited,"coop",training_zone_asset,NULL,NULL)==0);
    assert(!overrides.count);
    /* The coop rows never apply to another category. */
    assert(SubstituteConstModels(&overrides,&inherited,"mp",fav_zone_asset,NULL,NULL)==0);
    /* The favela: the M4 and the explosion map, the minigun stays. */
    assert(SubstituteConstModels(&overrides,&inherited,"coop",fav_zone_asset,NULL,NULL)==1);
    assert(SubstituteConstEffects(&overrides,&inherited,"coop",fav_zone_asset,NULL,NULL)==1);
    assert(!strcmp(overrides.values[1999],"weapon_m4") && !overrides.values[1983]);
    assert(!strcmp(overrides.values[3659],"explosions/aerial_explosion_large"));
    /* A script precaching the constant's name reuses the substituted slot. */
    assert(RegisterInheritedModelName(&overrides,&inherited,"codo_weapon_m4a1tech_world")==1999-1315 &&
        overrides.count==2);
    ClearConfigstrings(&overrides); ClearConfigstrings(&inherited);
}
int main(void)
{
    notetrack_effects();
    const_models();
    const_effects();
    const_coop();
    configstrings_t inherited={0},overrides={0};
    char white[]="white";
    const char *cells[]={"4276",white,"681","MP_VICTORY","2","cod"};
    const char *bad[]={"6519","bad"}; unsigned i,count; size_t bytes; const char *saved;
    assert(ImportConstantConfigstrings(&inherited,3,cells));
    white[0]='X'; assert(!strcmp(inherited.values[4276],"white"));
    assert(RegisterHudMaterial(&overrides,&inherited,"white")==1 && !overrides.count);
    assert(RegisterLocalizedString(&overrides,&inherited,"MP_VICTORY")==1 && !overrides.count);
    assert(RegisterHudMaterial(&overrides,&inherited,"damage_feedback")==2 && overrides.count==1);
    assert(RegisterHudMaterial(&overrides,&inherited,"damage_feedback")==2 && overrides.count==1);
    assert(RegisterLocalizedString(&overrides,&inherited,"Hello player")==2 && overrides.count==2);
    assert(RegisterHudMaterial(&overrides,&inherited,"")==0);
    assert(RegisterHudMaterial(&overrides,&inherited,"bad name")==-1);
    assert(RegisterHudMaterial(&inherited,&inherited,"new")==-1);
    assert(OL_SetConfigstring(&overrides,4276,""));
    /* Clearing a constant does not make its slot safe to repurpose. */
    assert(RegisterHudMaterial(&overrides,&inherited,"replacement")==3);
    saved=inherited.values[4276]; count=inherited.count; bytes=inherited.bytes;
    assert(!ImportConstantConfigstrings(&inherited,1,bad));
    bad[0]="-1"; assert(!ImportConstantConfigstrings(&inherited,1,bad));
    bad[0]="999999999999999999999"; assert(!ImportConstantConfigstrings(&inherited,1,bad));
    bad[0]="12x"; assert(!ImportConstantConfigstrings(&inherited,1,bad));
    { const char *duplicate[]={"2","a","02","b"};
      assert(!ImportConstantConfigstrings(&inherited,2,duplicate)); }
    assert(inherited.values[4276]==saved && inherited.count==count && inherited.bytes==bytes);
    for(i=1;i<HUD_MATERIAL_SLOTS;++i) assert(OL_SetConfigstring(&inherited,OL_CS_MATERIALS+i,"occupied"));
    count=overrides.count; bytes=overrides.bytes;
    assert(RegisterHudMaterial(&overrides,&inherited,"no room")==-1);
    assert(RegisterHudMaterial(&overrides,&inherited,"no_room")==-1 && overrides.count==count && overrides.bytes==bytes);
    assert(ImportConstantConfigstrings(&inherited,0,NULL) && !inherited.count && !inherited.bytes);
    ClearConfigstrings(&overrides); ClearConfigstrings(&inherited);
    ResetServerConfigstrings();
    assert(!ServerInheritedConfigstrings());
    assert(!SetServerInheritedConfigstrings("invalid",0,NULL));
    assert(!SetServerInheritedConfigstrings("mp",1,bad) && !ServerInheritedConfigstrings());
    assert(SetServerInheritedConfigstrings("mp",3,cells));
    assert(ServerInheritedConfigstrings()->count==3);
    assert(!SetServerInheritedConfigstrings("coop",0,NULL));
    assert(!SetServerInheritedConfigstrings("mp",0,NULL));
    assert(OL_SetConfigstring(ServerConfigstrings(),4277,"custom"));
    ResetServerConfigstrings();
    assert(!ServerInheritedConfigstrings() && !ServerConfigstrings()->count);
    assert(SetServerInheritedConfigstrings("coop",0,NULL));
    assert(ServerInheritedConfigstrings() && !ServerInheritedConfigstrings()->count);
    ResetServerConfigstrings();
    { uint32_t checksum=0;
      assert(ConstConfigChecksum("survival",&checksum) && checksum==UINT32_C(0xad1e1713));
      assert(IsZoneType("survival") && ZoneTypeRunsSoldiers("survival"));
      assert(!ZoneTypeRunsSoldiers("mp"));
      assert(SetServerMapIdentity("mp_killhouse_sh","survival"));
      assert(SetServerInheritedConfigstrings("survival",0,NULL));
      assert(ServerMapIdentityMatches("mp_killhouse_sh","survival"));
      assert(!ServerMapIdentityMatches("mp_killhouse_sh","mp"));
      assert(!SetServerMapIdentity("mp_killhouse_sh","mp"));
      ResetServerConfigstrings();
      assert(!ServerZoneType() && !ServerInheritedConfigstrings());
      assert(SetServerMapIdentity("mp_killhouse_sh","mp"));
      assert(SetServerInheritedConfigstrings("mp",0,NULL));
      assert(ServerMapIdentityMatches("mp_killhouse_sh","mp"));
      ResetServerConfigstrings(); }
    puts("PASS: inherited configstring import, reuse, reservation and atomic failure"); return 0;
}
