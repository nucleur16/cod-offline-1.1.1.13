/* Native mounted weapon composition; no serialized pointers reach the engine. */
#include "weapon_native.h"
#include "xmodel_native.h"
#include "xanim_native.h"
#include "effects_native.h"
#include "sound_native.h"
#include <string.h>
/* Resolve an adjusted asset name through its native provider. The old source
 * bytes may be absent after a rename, so the name and slot type are authoritative. */
static int load_image_effect(const char *name,const FxEffectDef **out)
{
    if(!name) { *out=NULL; return 1; }
    return *name && LoadEngineNativeEffect(name,out)==1 && *out;
}
/* C1 WeaponDef +7b8 is the projectile explosion effect. The retained
 * G_ExplodeMissile keeps a persistent smoke entity only when kind 5 has one
 * (C1 52ef3b..52ef4f makes the same test), so leaving it null silently drops
 * smoke grenade persistence. Tabun is kind 5 with no effect, as in C1. A
 * missing effect degrades to null, like the muzzle flashes: it is cosmetic. */
static const FxEffectDef *load_explosion_effect(const char *weapon,const weapon_tail_t *tail)
{
    const FxEffectDef *effect=NULL; unsigned i;
    for(i=0;i<tail->count;++i) {
        const weapon_dependency_t *entry=tail->entries+i;
        if(entry->offset!=0x7b8 || entry->kind!=WEAPON_DEP_ASSET || !entry->name || !*entry->name) continue;
        if(LoadEngineNativeEffect(entry->name,&effect)!=1 || !effect) {
            Com_PrintWarning(CON_CHANNEL_SYSTEM,
                "CoDOL weapon %s: explosion effect '%s' did not resolve in any mounted zone (%s)\n",
                weapon ? weapon : "?",entry->name,NativeEffectFailureDetail());
            effect=NULL;
        }
        break;
    }
    return effect;
}
int LoadEngineWeaponCoreFromImage(const weapon_source_image_t *image,
    weapon_hide_tag_handles_t *owned_tags,weapon_sound_bindings_t *owned_sounds,
    native_weapon_core_t *out)
{
    weapon_animations_t animations; weapon_effects_t effects;
    weapon_visuals_t visuals; weapon_tail_t tail;
    weapon_sounds_t sound_names; weapon_hide_tags_t tag_names;
    weapon_sound_bindings_t sounds; weapon_hide_tag_handles_t tags={{0}};
    native_weapon_core_t result={0};
    const XModel *models[11]={0},*projectile=NULL;
    const struct XAnimParts *parts[144]={0};
    const FxEffectDef *flashes[2]={0},*ejection[4]={0};
    unsigned i,j,projectile_slots=0;
    if(!image || !owned_tags || !owned_sounds || !out ||
       WeaponSourceAnimations(image,&animations)!=1 ||
       WeaponSourceEffects(image,&effects)!=1 ||
       WeaponSourceVisuals(image,&visuals)!=1 ||
       WeaponSourceTail(image,&tail)!=1 ||
       WeaponSourceSounds(image,&sound_names)!=1 ||
       WeaponSourceHideTags(image,&tag_names)!=1) return -1;
    for(i=0;i<11;++i) {
        weapon_model_t model; int status=WeaponSourceModel(image,i,&model);
        if(status<0 || (status==1 &&
           (LoadEnginePhysicsXModel(model.name,models+i)!=1 || !models[i]))) return -1;
    }
    for(i=0;i<144;++i) if(animations.names[i] && *animations.names[i])
        if(LoadEngineNativeXAnim(animations.names[i],parts+i)!=1 || !parts[i]) return -1;
    for(i=0;i<2;++i) if(!load_image_effect(effects.names[i],flashes+i)) return -1;
    for(i=0;i<tail.count;++i) if(tail.entries[i].offset==0x7b0) {
        const weapon_dependency_t *entry=tail.entries+i;
        if(++projectile_slots!=1 || entry->kind!=WEAPON_DEP_ASSET || entry->asset.type!=4) return -1;
        if(entry->name && (!*entry->name ||
           LoadEnginePhysicsXModel(entry->name,&projectile)!=1 || !projectile)) return -1;
    }
    if(projectile_slots!=1) return -1;
    for(i=0;i<4;++i) {
        const weapon_dependency_t *entry=NULL;
        for(j=0;j<14;++j) if(visuals.entries[j].offset==0x440+4*i) {
            if(entry) return -1;
            entry=visuals.entries+j;
        }
        if(!entry || entry->kind!=WEAPON_DEP_ASSET || entry->asset.type!=30 ||
           !load_image_effect(entry->name,ejection+i)) return -1;
    }
    if(BuildNativeWeaponSoundBindings(&sound_names,&tail,&sounds)!=1 ||
       AcquireEngineWeaponHideTags(&tag_names,&tags)!=1) return -1;
    if(!BuildNativeWeaponCoreFromImage(image,models,&tags,&result) ||
       !ApplyNativeWeaponSounds(&sounds,&result.definition)) {
        FreeNativeWeaponCore(&result);
        ReleaseEngineWeaponHideTags(&tags); return -1;
    }
    result.definition.projectileModel=(XModel *)projectile;
    result.definition.projExplosionEffect=(struct FxEffectDef *)load_explosion_effect(NULL,&tail);
    result.definition.viewFlashEffect=(FxEffectDef *)flashes[0];
    result.definition.worldFlashEffect=(FxEffectDef *)flashes[1];
    result.definition.viewShellEjectEffect=(FxEffectDef *)ejection[0];
    result.definition.worldShellEjectEffect=(FxEffectDef *)ejection[1];
    result.definition.viewLastShotEjectEffect=(FxEffectDef *)ejection[2];
    result.definition.worldLastShotEjectEffect=(FxEffectDef *)ejection[3];
    memcpy(result.animation_parts,parts,sizeof(parts));
    /* No pointer into the temporary bindings may escape. */
    if(result.definition.bounceSound) result.definition.bounceSound=owned_sounds->arrays[0];
    FreeNativeWeaponCore(out);
    ReleaseEngineWeaponHideTags(owned_tags);
    *owned_tags=tags; *owned_sounds=sounds; *out=result; return 1;
}
int RegisterMountedWeaponKeys(weapon_registry_t *registry,unsigned wire,const char *name)
{
    mounted_weapon_t source; weapon_ammo_names_t names;
    if(!registry || !name || !*name || name[0]=='*' ||
       LoadMountedWeapon(name,&source)!=1 || !source.asset.data || source.asset.size<0xbfc ||
       !source.strings[0] || strcmp(name,source.strings[0]) ||
       LoadMountedWeaponAmmoNames(name,&names)!=1) return 0;
    return RegisterWeaponKeys(registry,wire,name,names.ammo,names.clip,
        source.asset.data[0xbbc],source.asset.data[0x560],-1);
}

static int load_projectile_model(const mounted_weapon_t *source,
    const weapon_tail_t *tail,const XModel **out)
{
    const weapon_dependency_t *entry=NULL;
    const uint8_t *p; unsigned i; int present;
    if(!source->asset.data || source->asset.size<0xbfc || tail->count>64) return 0;
    for(i=0;i<tail->count;++i) if(tail->entries[i].offset==0x7b0) {
        if(entry) return 0;
        entry=tail->entries+i;
    }
    if(!entry || entry->kind!=WEAPON_DEP_ASSET) return 0;
    p=source->asset.data+0x7b0; present=!!(p[0]|p[1]|p[2]|p[3]);
    if(!present) return !entry->name && !entry->asset.data && !entry->asset.size;
    /* by name: a weapon read from C1's own record carries no server definition for its slots */
    if(!entry->name || !*entry->name || entry->asset.type!=4) return 0;
    /* alpha0022/docs/projectiles_fix.md: C1 missile renderer 443c0f
     * reads def+7b0. Retained G_RegisterWeapon uses projectileModel +584. */
    return LoadEnginePhysicsXModel(entry->name,out)==1 && *out;
}

/* Last failure site of LoadEngineWeaponCore, for the engine error path.
 * A pure module: no console output, only a retrievable reason. */
static int core_fail_line;
static int core_fail(int line) { core_fail_line=line; return -1; }
int EngineWeaponCoreFailureLine(void) { return core_fail_line; }
int LoadEngineWeaponCore(const char *name,native_weapon_core_t *out)
{
    ResetNativeWeaponCoreFailure();
    mounted_weapon_t source;
    weapon_animations_t animations;
    weapon_ammo_names_t ammo;
    weapon_tail_t tail;
    const weapon_hide_tag_handles_t *tags=NULL;
    const XModel *models[11]={0}; unsigned i; int status;
    const XModel *projectile=NULL;
    weapon_effects_t effects;
    const FxEffectDef *flashes[2]={0};
    weapon_visuals_t visuals;
    const FxEffectDef *ejection[4]={0};
    weapon_sounds_t sound_names;
    const weapon_sound_bindings_t *sounds=NULL;
    const struct XAnimParts *parts[144]={0};
    if(!name || !*name || !out) return core_fail(__LINE__);
    status=LoadMountedWeapon(name,&source); if(status!=1) return status<0 ? core_fail(__LINE__) : status;
    if(LoadMountedWeaponAnimations(name,&animations)!=1 ||
       LoadMountedWeaponAmmoNames(name,&ammo)!=1 ||
       LoadMountedWeaponTail(name,&tail)!=1) return core_fail(__LINE__);
    for(i=0;i<11;++i) {
        weapon_model_t model;
        status=LoadMountedWeaponModel(name,i,&model);
        if(status<0) return core_fail(__LINE__);
        if(status==1 && LoadEnginePhysicsXModel(model.name,&models[i])!=1) return core_fail(__LINE__);
    }
    for(i=0;i<144;++i) if(animations.names[i] && *animations.names[i]) {
        if(LoadEngineNativeXAnim(animations.names[i],&parts[i])!=1 || !parts[i]) {
            /* Name the clip: a ported zone that misses one otherwise only
             * reports this line. */
            Com_PrintWarning(CON_CHANNEL_SYSTEM,"CoDOL native weapon %s: animation slot %u '%s' is in no mounted zone or not convertible\n",
                             name,i,animations.names[i]);
            return core_fail(__LINE__);
        }
    }
    /* Animation-name slot 7 is the melee (knife) viewmodel xanim, slot 8 the
     * melee charge. docs/MELEE_ANIM_20260920.md proves the slot from the
     * shipped weaponattach records -- three of them write a literally *_melee
     * name to def+0x098. C1's name getter 0x45c500 returns def[0x7c+4*index]
     * raw and substitutes nothing, so an empty slot plays no animation, while
     * def+0x590 still lets the weapon melee and deal damage.
     *
     * MEASURED 2026-09-20: 22 of the 40 weapons the zm roster hands out ship
     * with that slot empty, among them coltanaconda_zm (".44 Magnum") and
     * m14ebr_zm -- the reported bug. It is a gap in the shipped definitions,
     * and the server has no channel to close it: no zone a zm match mounts
     * carries a weaponattach asset, so C1's composed-name override path is
     * unavailable. Name it once per weapon rather than leaving it to be
     * rediscovered from a player report, for the same reason d6b75caa names
     * the unresolvable muzzle flash below. */
    if(source.asset.size>=0xbfc) {
        const uint8_t *melee=source.asset.data+0x590;
        unsigned damage=(unsigned)melee[0]|(unsigned)melee[1]<<8|
                        (unsigned)melee[2]<<16|(unsigned)melee[3]<<24;
        if(damage && damage<=0x7fffffffu && (!animations.names[7] || !*animations.names[7]))
            Com_PrintWarning(CON_CHANNEL_SYSTEM,
                "CoDOL weapon %s: melee damage %u but animation slot 7 is empty; "
                "C1 plays no melee animation for it (shipped definition gap)\n",
                name,damage);
    }
    if(LoadEngineWeaponHideTags(name,&tags)!=1) return core_fail(__LINE__);
    if(!load_projectile_model(&source,&tail,&projectile)) return core_fail(__LINE__);
    if(LoadMountedWeaponEffects(name,&effects)!=1) return core_fail(__LINE__);
    for(i=0;i<2;++i) {
        const uint8_t *p=source.asset.data+0x35c+4*i;
        int present=!!(p[0]|p[1]|p[2]|p[3]);
        if(present!=!!effects.names[i] || (effects.names[i] && !*effects.names[i])) return core_fail(__LINE__);
        /* An unresolvable muzzle flash DEGRADES; it does not kill the server.
         * MEASURED 2026-09-20: the first zm boot to publish a full 131-slot
         * weapon window died here on m1911_zm -- "invalid or unsupported
         * data", with a line number and nothing else to go on.
         *
         * NULL is already a legal value for both of these fields: it is what
         * they hold when the definition declares no flash at all (the
         * `present` test above), and they are assigned straight into
         * definition.viewFlashEffect / worldFlashEffect. It also matches how
         * the rest of this server treats a missing dependency -- an absent
         * xmodel becomes 'void', an absent xanim becomes the null animation,
         * both with a warning -- and it matches C1, which does not refuse to
         * start a map over a cosmetic effect.
         *
         * The warning NAMES the weapon and the effect, because the bare line
         * number cost a boot cycle to interpret. If the named effect turns
         * out to ship in a mounted zone after all, that is a resolver bug
         * rather than missing content, and this is what will reveal it. */
        if(present && (LoadEngineNativeEffect(effects.names[i],&flashes[i])!=1 || !flashes[i])) {
            Com_PrintWarning(CON_CHANNEL_SYSTEM,
                "CoDOL weapon %s: %s muzzle flash '%s' did not resolve in any mounted zone (%s); "
                "loading the weapon without it\n",
                name, i ? "world" : "view", effects.names[i],NativeEffectFailureDetail());
            flashes[i]=NULL;
        }
    }
    if(LoadMountedWeaponVisuals(name,&visuals)!=1) return core_fail(__LINE__);
    for(i=0;i<4;++i) {
        const weapon_dependency_t *entry=NULL; unsigned j;
        const uint8_t *p=source.asset.data+0x440+4*i;
        int present=!!(p[0]|p[1]|p[2]|p[3]);
        for(j=0;j<14;++j) if(visuals.entries[j].offset==0x440+4*i) {
            if(entry) return core_fail(__LINE__);
            entry=visuals.entries+j;
        }
        if(!entry || entry->kind!=WEAPON_DEP_ASSET) return core_fail(__LINE__);
        if(!present) {
            if(entry->name || entry->asset.data || entry->asset.size) return core_fail(__LINE__);
        } else if(!entry->name || !*entry->name || entry->asset.type!=30 ||
            LoadEngineNativeEffect(entry->name,&ejection[i])!=1 || !ejection[i]) return core_fail(__LINE__);
    }
    /* Every sound name the definition carries becomes a native alias list
     * before the core is replaced. Names no mounted archive defines stay null;
     * a malformed alias, curve or loaded sound fails the whole weapon. */
    if(LoadMountedWeaponSounds(name,&sound_names)!=1) return core_fail(__LINE__);
    if(LoadEngineWeaponSounds(name,&sound_names,&tail,&sounds)!=1 || !sounds) return core_fail(__LINE__);
    if(!BuildNativeWeaponCore(&source,models,&animations,&ammo,&tail,tags,out)) return core_fail(__LINE__);
    out->definition.projectileModel=(XModel *)projectile;
    out->definition.projExplosionEffect=(struct FxEffectDef *)load_explosion_effect(name,&tail);
    /* C1 leading dependency slots +35c/+360 are view/world muzzle flashes.
     * All dependency loads finish before replacing the caller's core. */
    out->definition.viewFlashEffect=(FxEffectDef *)flashes[0];
    out->definition.worldFlashEffect=(FxEffectDef *)flashes[1];
    /* C1 brass selection 4819d0..481a08: last-shot pair then ordinary pair,
     * each selected by the local-view predicate, matching IW3 brass code. */
    out->definition.viewShellEjectEffect=(FxEffectDef *)ejection[0];
    out->definition.worldShellEjectEffect=(FxEffectDef *)ejection[1];
    out->definition.viewLastShotEjectEffect=(FxEffectDef *)ejection[2];
    out->definition.worldLastShotEjectEffect=(FxEffectDef *)ejection[3];
    /* Established slot roles only; the bindings keep the CoDOL-only akimbo,
     * loop-end and rolling-surface sounds for later gameplay work. */
    if(!ApplyNativeWeaponSounds(sounds,&out->definition)) {
        FreeNativeWeaponCore(out); return core_fail(__LINE__);
    }
    memcpy(out->animation_parts,parts,sizeof(parts)); return 1;
}
