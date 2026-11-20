/* WeaponDef in C1's memory layout (feat/iw4-asset-layout, stage 2, type 28).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 keeps a weapon as its 0xBFC-byte serialized header with every pointer slot relocated (loader 0x4c3c20,
 * pointer kinds from load_blocks.c replay_weapon). The server builds exactly that record from the
 * weapon's source image (weapon_attachment.c, which already resolves every slot to a name):
 *   - every scalar byte as serialized;
 *   - string slots: char* to the string;
 *   - sound slots: the server's C1-layout snd_alias_list_t (type 11, converted); a name-only record (below) when
 *     the alias is in a zone the server does not mount;
 *   - the two 31-entry surface sound arrays: pointer to 31 alias-list pointers;
 *   - graph point arrays: pointer to the float pairs, counts at +0xbb0/+0xbb2 as serialized;
 *   - model, effect, material, physics and tracer slots: their target asset. Those types are not held in C1's
 *     layout yet, so the pointer is to a name-only record (C1 headers of those types all begin with their
 *     name); each is replaced by the real record when its type is converted.
 *   - the 32 hide tags at +0x3c and the four 16-entry name maps at +0x2bc are script-string handles, which
 *     belong to the table of whoever holds the record (C1's own SL in the client); the server keeps them zero
 *     here and reads the names from the zone.
 * The retained IW3 WeaponDef the engine uses is still built by weapon_loader.c; this record is what an
 * in-process host compares with C1's (Kernel33 p2passetcheck 28) and, in stage 3, what C1 provides. */
#include <stdlib.h>
#include <string.h>
#include "assets.h"
#include "weapon_attachment.h"
#include "sound_native.h"
#include "c1_weapon.h"

enum { K_NONE, K_STRING, K_SOUND, K_SOUND_ARRAY, K_ASSET, K_POINTS };
/* The pointer slots of C1's WeaponDef, the field table of replay_weapon in loader order. */
static const struct { unsigned offset, count, kind; } c1_weapon_slots[] = {
    {0,4,K_STRING},{0x10,11,K_ASSET},{0x7c,144,K_STRING},
    {0x35c,2,K_ASSET},{0x364,53,K_SOUND},{0x438,2,K_SOUND_ARRAY},
    {0x440,4,K_ASSET},{0x450,2,K_ASSET},{0x52c,6,K_ASSET},
    {0x548,1,K_ASSET},{0x550,1,K_ASSET},
    {0x564,1,K_STRING},{0x56c,1,K_STRING},{0x57c,1,K_STRING},
    {0x684,4,K_ASSET},{0x758,1,K_ASSET},{0x7b0,1,K_ASSET},
    {0x7b8,2,K_ASSET},{0x7c0,2,K_SOUND},{0x8cc,2,K_ASSET},
    {0x8ec,1,K_ASSET},{0x8f0,1,K_SOUND},{0x9a0,1,K_STRING},
    {0x9a8,1,K_POINTS},{0x9a4,1,K_STRING},{0x9ac,1,K_POINTS},
    {0x9fc,2,K_STRING},{0xa20,2,K_STRING},{0xaa0,2,K_STRING},
    {0xaa8,1,K_ASSET},{0xac4,1,K_SOUND},{0xac8,1,K_ASSET},
    {0xacc,1,K_STRING},{0xadc,9,K_SOUND},{0xb00,2,K_SOUND},
    {0xb48,6,K_SOUND},{0xb78,1,K_STRING},{0xb84,2,K_STRING},
    {0xb90,3,K_ASSET},{0xbb4,2,K_POINTS}
};

unsigned C1WeaponSlotKind(unsigned offset)
{
    unsigned i;
    for(i=0;i<sizeof(c1_weapon_slots)/sizeof(c1_weapon_slots[0]);++i)
        if(offset>=c1_weapon_slots[i].offset && offset<c1_weapon_slots[i].offset+4*c1_weapon_slots[i].count &&
           !((offset-c1_weapon_slots[i].offset)&3)) return c1_weapon_slots[i].kind;
    return K_NONE;
}

typedef struct c1_weapon_s {
    char *name;
    unsigned generation;
    uint8_t *record;            /* C1_WEAPON_SIZE bytes */
    void **allocations;         /* everything the record points at that it owns */
    size_t allocation_count,allocation_capacity;
    struct c1_weapon_s *next;
} c1_weapon_t;
static c1_weapon_t *c1_weapons;

static void *own(c1_weapon_t *w,void *p)
{
    if(!p) return NULL;
    if(w->allocation_count==w->allocation_capacity) {
        size_t n=w->allocation_capacity ? 2*w->allocation_capacity : 64;
        void **a=realloc(w->allocations,n*sizeof(*a));
        if(!a) { free(p); return NULL; }
        w->allocations=a; w->allocation_capacity=n;
    }
    w->allocations[w->allocation_count++]=p; return p;
}
static void free_weapon(c1_weapon_t *w)
{
    size_t i;
    for(i=0;i<w->allocation_count;++i) free(w->allocations[i]);
    free(w->allocations); free(w->record); free(w->name); free(w);
}
static char *copy_string(c1_weapon_t *w,const char *text)
{
    size_t n; char *p;
    if(!text) return NULL;
    n=strlen(text)+1;
    if(!(p=own(w,malloc(n)))) return NULL;
    memcpy(p,text,n); return p;
}
static void put(uint8_t *record,unsigned offset,const void *pointer)
{ memcpy(record+offset,&pointer,4); }

/* Release every record built under an older mount (the registry bumps its serial on each mount change). */
void ResetC1Weapons(void)
{
    while(c1_weapons) { c1_weapon_t *w=c1_weapons; c1_weapons=w->next; free_weapon(w); }
}

int LoadC1WeaponDef(const char *name,const void **out)
{
    weapon_source_image_t image={0};
    c1_weapon_t *w; size_t i; unsigned generation=AssetMountSerial(); int status;
    if(!name || !*name || !out) return -1;
    for(w=c1_weapons;w;w=w->next)
        if(w->generation==generation && !strcmp(w->name,name)) { *out=w->record; return 1; }
    status=BeginWeaponSourceImage(name,&image);
    if(status!=1) return status;
    if(!(w=calloc(1,sizeof(*w))) || !(w->record=malloc(C1_WEAPON_SIZE)) || !(w->name=malloc(strlen(name)+1))) {
        if(w) { free(w->record); free(w); }
        FreeWeaponSourceImage(&image); return -1;
    }
    memcpy(w->name,name,strlen(name)+1);
    w->generation=generation;
    memcpy(w->record,image.data,C1_WEAPON_SIZE);
    /* Script-string handles of the record's holder (zone-local indices when serialized, C1's own handles in C1):
     * the 32 hide tags at +0x3c and the four 16-entry name maps at +0x2bc..+0x33b (measured: the only scalar
     * bytes that differ from C1's records, p2passetcheck 28). The server reads their names from the zone. */
    memset(w->record+0x3c,0,0x40);
    memset(w->record+0x2bc,0,0x80);
    for(i=0;i<sizeof(c1_weapon_slots)/sizeof(c1_weapon_slots[0]);++i) {
        unsigned j;
        for(j=0;j<c1_weapon_slots[i].count;++j) {
            unsigned offset=c1_weapon_slots[i].offset+4*j,kind=c1_weapon_slots[i].kind;
            const char *slot_name=NULL;
            put(w->record,offset,NULL);
            if(kind==K_SOUND_ARRAY) {
                const char *const *names=image.sound_arrays[(offset-0x438)/4];
                unsigned k,any=0;
                void **array;
                for(k=0;k<31;++k) any|=names[k]!=NULL;
                if(!any) continue;
                if(!(array=own(w,calloc(31,sizeof(void *))))) goto fail;
                for(k=0;k<31;++k) {
                    if(!names[k]) continue;
                    array[k]=FindEngineSoundAlias(names[k]);
                    if(!array[k]) {
                        /* not mounted by the server (measured: semtex/throwing knife riot-shield bounce sounds) */
                        const char **named=own(w,malloc(sizeof(*named)));
                        char *text=copy_string(w,names[k]);
                        if(!named || !text) goto fail;
                        *named=text; array[k]=(void *)named;
                    }
                }
                put(w->record,offset,array);
                continue;
            }
            if(kind==K_POINTS) {
                unsigned t;
                for(t=0;t<image.tail.count;++t) {
                    const weapon_dependency_t *d=image.tail.entries+t;
                    if(d->kind!=WEAPON_DEP_POINTS || d->offset!=offset || !d->points || !d->point_count) continue;
                    {
                        void *points=own(w,malloc((size_t)d->point_count*8));
                        if(!points) goto fail;
                        memcpy(points,d->points,(size_t)d->point_count*8);
                        put(w->record,offset,points);
                    }
                }
                continue;
            }
            if(WeaponSourceName(&image,offset,&slot_name)!=1 || !slot_name) continue;
            if(kind==K_STRING) {
                char *text=copy_string(w,slot_name);
                if(!text) goto fail;
                put(w->record,offset,text);
            } else if(kind==K_SOUND && FindEngineSoundAlias(slot_name)) {
                put(w->record,offset,FindEngineSoundAlias(slot_name));
            } else if(kind==K_SOUND || kind==K_ASSET) {
                /* A sound whose alias is in a zone the server does not mount (measured: type95_mp's
                 * weap_type95_reload_npc, javelin/stinger's nightvision_wear_plr_default) keeps its name the same way. */
                /* A name-only record (see the header) until the slot's type is held in C1's layout. */
                const char **named=own(w,malloc(sizeof(*named)));
                char *text=copy_string(w,slot_name);
                if(!named || !text) goto fail;
                *named=text;
                put(w->record,offset,named);
            }
        }
    }
    FreeWeaponSourceImage(&image);
    w->next=c1_weapons; c1_weapons=w;
    *out=w->record; return 1;
fail:
    FreeWeaponSourceImage(&image);
    free_weapon(w); return -1;
}

int EnumerateMountedWeapons(int (*visit)(void *,const char *),void *context)
{
    return EnumerateMountedType(28,visit,context);
}
