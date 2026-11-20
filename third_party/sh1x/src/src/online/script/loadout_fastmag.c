/* Existing C1 asset recipe adaptation. See CODOL_FASTMAG_FOUR_PHASE_REVIEW. */
#ifdef OL_SERVER
#include "loadout_fastmag.h"
#include "weapon_attachment.h"
#include "qcommon.h"
#include <string.h>

static int same_text(const char *a,const char *b)
{ return a && b ? !strcmp(a,b) : a==b; }
static const char *source_name(const weapon_source_image_t *image,unsigned offset)
{
    const char *name=NULL;
    WeaponSourceName(image,offset,&name);
    return name;
}
/* Compare all header bytes, resolving the pointer fields represented by names.
 * Only internal identity and the two absent quick slots may differ. */
static int same_definition(const weapon_source_image_t *event,
    const weapon_source_image_t *ordinary)
{
    unsigned char a[0xbfc],b[0xbfc];size_t i,j;
    if(event->size<sizeof(a) || ordinary->size<sizeof(b) ||
       event->name_count!=ordinary->name_count)return 0;
    memcpy(a,event->data,sizeof(a));memcpy(b,ordinary->data,sizeof(b));
    for(i=0;i<event->name_count;++i) {
        unsigned offset=event->names[i].offset;
        const char *other=NULL;
        if(!WeaponSourceName(ordinary,offset,&other))return 0;
        if(offset!=0 && offset!=0xa4 && offset!=0xac &&
           !same_text(event->names[i].name,other)) {
            Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL fastmag compare: name +0x%x event '%s' ordinary '%s'\n",offset,event->names[i].name ? event->names[i].name : "<null>",other ? other : "<null>");
            return 0;
        }
        if(offset<=sizeof(a)-4) { memset(a+offset,0,4);memset(b+offset,0,4); }
    }
    if(memcmp(a,b,sizeof(a))) {
        for(i=0;i<sizeof(a);++i)if(a[i]!=b[i]) {
            Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL fastmag compare: scalar +0x%x event %u ordinary %u\n",(unsigned)i,a[i],b[i]);break;
        }
        return 0;
    }
    for(i=0;i<WEAPON_SOURCE_TAGS;++i)
        if(!same_text(event->hide_tags[i],ordinary->hide_tags[i]))return 0;
    for(i=0;i<2;++i)for(j=0;j<31;++j)
        if(!same_text(event->sound_arrays[i][j],ordinary->sound_arrays[i][j]))return 0;
    return 1;
}
static int same_models(const loadout_recipe_t *recipe,const char *ordinary)
{
    const char *ew=NULL,*ev=NULL,*ow=NULL,*ov=NULL;unsigned i;
    if(ComposedBaseModelRow(recipe->base_name,recipe->flag,&ew,&ev)!=1 ||
       ComposedBaseModelRow(ordinary,recipe->flag,&ow,&ov)!=1 ||
       !same_text(ew,ow) || !same_text(ev,ov))return 0;
    for(i=0;i<recipe->count;++i) {
        const loadout_part_t *part=&recipe->parts[i];
        const char *et=NULL,*ot=NULL;int es,os;
        ew=ev=ow=ov=NULL;
        es=ComposedItemModelRow(recipe->base_name,part->name,part->group,
            recipe->flag,&ew,&ev,&et);
        os=ComposedItemModelRow(ordinary,part->name,part->group,
            recipe->flag,&ow,&ov,&ot);
        if(es<0 || es!=os || !same_text(ew,ow) || !same_text(ev,ov) ||
           !same_text(et,ot)) {
            Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL fastmag compare: model part %s camo %u status %d/%d world '%s'/'%s' view '%s'/'%s' tag '%s'/'%s'\n",part->name,recipe->flag,es,os,ew ? ew : "<null>",ow ? ow : "<null>",ev ? ev : "<null>",ov ? ov : "<null>",et ? et : "<null>",ot ? ot : "<null>");
            return 0;
        }
    }
    return 1;
}
int EventFastMagCompositionBase(const loadout_recipe_t *recipe,unsigned *out)
{
    const char *ordinary_name="m4a1tech_mp";
    weapon_source_image_t event={0},ordinary={0};
    weapon_attachment_t loaded[COMPOSED_ATTACHMENTS]={0};
    const weapon_attachment_t *parts[COMPOSED_ATTACHMENTS];
    unsigned count=0,i,index;int result=-1,has_fastmag=0;
    const char *stage="source/model prerequisites";
    if(!recipe || !out || !recipe->base_name ||
       strcmp(recipe->base_name,"m4a1techevent_mp"))return 0;
    if(recipe->count>COMPOSED_ATTACHMENTS)return -1;
    for(i=0;i<recipe->count;++i)
        if(recipe->parts[i].name && !strcmp(recipe->parts[i].name,"mag_dualar15"))has_fastmag=1;
    if(!has_fastmag)return 0;
    if(LoadoutWeaponIndex(ordinary_name,&index)!=1 || !index ||
       !same_models(recipe,ordinary_name) ||
       BeginWeaponSourceImage(recipe->base_name,&event)!=1 ||
       BeginWeaponSourceImage(ordinary_name,&ordinary)!=1)goto done;
    stage="base quick animation seeds";
    for(i=0;i<2;++i) {
        unsigned normal=0xa0+8*i,quick=normal+4;
        const char *missing=source_name(&event,quick),*seed=source_name(&ordinary,quick);
        if(!missing || *missing || !seed || !*seed ||
           !same_text(seed,source_name(&ordinary,normal)))goto done;
    }
    stage="base definition equivalence";
    if(!same_definition(&event,&ordinary))goto done;
    stage="attachment asset equivalence";
    for(i=0;i<recipe->count;++i) {
        const char *ea=NULL,*oa=NULL;
        if(ComposedAttachmentAssetName(recipe->base_name,recipe->parts[i].item,&ea)!=1 ||
           ComposedAttachmentAssetName(ordinary_name,recipe->parts[i].item,&oa)!=1 ||
           !same_text(ea,oa))goto done;
        if(!ea || !*ea)goto done;
        if(ReadWeaponAttachment(ea,&loaded[count])!=1)goto done;
        parts[count]=&loaded[count];++count;
    }
    stage="applied definition equivalence";
    /* Both constructors consume exactly the event's pairs. Comparing after
     * overrides also rejects a <w> context substitution that changes behavior. */
    if(ApplyWeaponAttachments(&event,parts,count)!=1 ||
       ApplyWeaponAttachments(&ordinary,parts,count)!=1 ||
       !same_definition(&event,&ordinary) || ordinary.data[0xbc7]!=1)goto done;
    stage="final quick animation slots";
    for(i=0;i<2;++i) {
        const char *normal=source_name(&ordinary,0xa0+8*i);
        const char *quick=source_name(&ordinary,0xa4+8*i);
        if(!normal || !*normal || !quick || !*quick || !strcmp(normal,quick))goto done;
    }
    *out=index;result=1;
done:
    if(result<0)Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL fastmag verification stopped: %s (part/slot %u)\n",stage,i);
    for(i=0;i<count;++i)FreeWeaponAttachment(&loaded[i]);
    FreeWeaponSourceImage(&event);FreeWeaponSourceImage(&ordinary);
    return result;
}
#endif
