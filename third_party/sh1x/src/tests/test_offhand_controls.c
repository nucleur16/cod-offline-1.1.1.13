#include "scr_vm_functions.h"
#include "engine_state.h"
#include "weapon_flags.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
gentity_t g_entities[MAX_GENTITIES];
static gclient_t player;
static unsigned errors;
/* This fixture exercises use/offhand publication, not the separate hand
 * state or optional crosshair diagnostics. Fail if diagnostics become active. */
cvar_t *ol_debugTrace;
int WeaponStateSecondBarrelArmed(const playerState_t *ps)
{ assert(ps); return 0; }
void QDECL Com_Printf(conChannel_t channel,const char *fmt,...)
{ (void)channel; (void)fmt; assert(!"unexpected crosshair diagnostic"); }
void Scr_ObjectError(const char *s) { assert(s && *s); ++errors; }
int main(void)
{
    scr_entref_t ref={0}; player_fields_t wire,before; unsigned i,k;
    ref.entnum=2; g_entities[2].client=&player;
    player.ps.weapFlags=0x82; /* Preserve global disable and active offhand. */
    Scr_DisableOffhandWeapons(ref); assert(player.ps.weapFlags==0x1082);
    Scr_DisableOffhandWeapons(ref); assert(player.ps.weapFlags==0x1082);
    memset(&wire,0xa5,sizeof(wire)); before=wire;
    assert(OverlayEngineOffhandControl(&wire,&player.ps));
    for(i=0;i<PLAYER_FIELDS;++i) {
        if(OL_PlayerField(i)->offset==0x344) assert(wire.field[i]==(before.field[i]|0xe0000));
        else assert(wire.field[i]==before.field[i]);
    }
    Scr_EnableOffhandWeapons(ref); assert(player.ps.weapFlags==0x82);
    Scr_DisableWeaponSwitch(ref); assert(player.ps.weapFlags==0x882);
    Scr_DisableWeaponSwitch(ref); assert(player.ps.weapFlags==0x882);
    Scr_DisableOffhandWeapons(ref); assert(player.ps.weapFlags==0x1882);
    before=wire;
    assert(OverlayEngineWeaponSwitchControl(&wire,&player.ps));
    for(i=0;i<PLAYER_FIELDS;++i) {
        if(OL_PlayerField(i)->offset==0x344) assert(wire.field[i]==(before.field[i]|0x800));
        else assert(wire.field[i]==before.field[i]);
    }
    Scr_EnableWeaponSwitch(ref); assert(player.ps.weapFlags==0x1082);
    Scr_EnableWeaponSwitch(ref); assert(player.ps.weapFlags==0x1082);
    assert(OverlayEngineWeaponSwitchControl(&wire,&player.ps));
    for(i=0;i<PLAYER_FIELDS;++i)
        if(OL_PlayerField(i)->offset==0x344) assert(wire.field[i]==(before.field[i]&~0x800u));
    Scr_EnableOffhandWeapons(ref); assert(player.ps.weapFlags==0x82);
    before=wire;
    assert(OverlayEngineOffhandControl(&wire,&player.ps));
    for(i=0;i<PLAYER_FIELDS;++i)
        if(OL_PlayerField(i)->offset==0x344) assert(wire.field[i]==(before.field[i]&~0xe0000u));
    Scr_EnableOffhandWeapons(ref); assert(player.ps.weapFlags==0x82);
    for(k=0;k<3;++k) {
        ref.entnum=k==0?3:k==1?MAX_GENTITIES:2; ref.classnum=k==2?2:0;
        Scr_DisableOffhandWeapons(ref); assert(errors==k+1 && player.ps.weapFlags==0x82);
    }
    for(k=0;k<3;++k) {
        ref.entnum=k==0?3:k==1?MAX_GENTITIES:2; ref.classnum=k==2?2:0;
        Scr_DisableWeaponSwitch(ref); assert(errors==k+4 && player.ps.weapFlags==0x82);
    }
    before=wire;
    assert(!OverlayEngineOffhandControl(&wire,NULL) && !memcmp(&wire,&before,sizeof(wire)));
    assert(!OverlayEngineOffhandControl(NULL,&player.ps));
    assert(!OverlayEngineWeaponSwitchControl(&wire,NULL) && !memcmp(&wire,&before,sizeof(wire)));
    assert(!OverlayEngineWeaponSwitchControl(NULL,&player.ps));
    for(k=0;k<4;++k) {
        unsigned flags=(k&1)|((k&2)?0x80:0);
        memset(&wire,0xa5,sizeof(wire)); before=wire;
        player.ps.weapFlags=flags|USE_DISABLED|WEAP_OFFHAND_DISABLED|WEAP_SWITCH_DISABLED;
        assert(OverlayEngineWeaponUseControl(&wire,&player.ps));
        for(i=0;i<PLAYER_FIELDS;++i) {
            if(OL_PlayerField(i)->offset==0x344) {
                /* C1 owns the shared action, breath/crosshair, shield and
                 * pickup-hint bits in this word. The zeroed test player must
                 * clear those stale bits while preserving every other bit. */
                assert(wire.field[i]==((before.field[i]&~0x10119du)|flags));
            }
            else assert(wire.field[i]==before.field[i]);
        }
        before=wire;
        assert(!OverlayEngineWeaponUseControl(&wire,NULL) && !memcmp(&wire,&before,sizeof(wire)));
    }
    assert(!OverlayEngineWeaponUseControl(NULL,&player.ps));
    puts("PASS: native weapon controls, reference validation, independent flags and CoDOL publication");
}
