#ifdef OL_SERVER
#include "profile_skill.h"
#include "script_profile_vm.h"
#include "profile_updates.h"
#include "skill_rating.h"
#include "scoreboard_stats.h"
#include "zlib/zlib.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const structured_definition_t *definition;
    structured_field_t field;
    size_t capacity;
    uint8_t *staged, *destination;
    int32_t rating;
} skill_edit_t;

static int read_skill(unsigned client,const char *mode,skill_edit_t *edit)
{
    structured_path_t path[2]={{"skills",0},{mode,0}};
    const uint8_t *record,*blob;
    uint32_t raw;
    float number;
    if(!ScriptProfileView(client,PROFILE_PUBLIC,
        &edit->definition,&record,&edit->capacity) ||
       !ResolveStructuredField(edit->definition,path,2,&edit->field) ||
       !StructuredReadScalar(&edit->definition->layout,edit->field.type.kind,
           edit->field.logical_index,record,edit->capacity,&raw)) return 0;
    /* Same scalar coercion used by C1's skills/tdm scoreboard reader. */
    switch(edit->field.type.kind) {
    case 0: memcpy(&edit->rating,&raw,sizeof(raw)); break;
    case 1: case 2: edit->rating=(int32_t)raw; break;
    case 9: edit->rating=(raw&0x8000u)?(int32_t)(raw&0xffffu)-65536:(int32_t)raw; break;
    case 8: memcpy(&number,&raw,sizeof(number)); edit->rating=ScoreInteger(number); break;
    default: return 0;
    }
    blob=ScriptProfileBlob(client,PROFILE_PUBLIC);
    if(!blob || !(edit->staged=malloc(STATS_SIZE))) return 0;
    memcpy(edit->staged,blob,STATS_SIZE);
    return 1;
}
static int stage_skill(skill_edit_t *edit)
{
    uint32_t raw=(uint32_t)edit->rating,crc;
    unsigned i;
    float number;
    switch(edit->field.type.kind) {
    case 0: break;
    case 1: if(edit->rating<0 || edit->rating>255) return 0; break;
    case 2: raw=edit->rating!=0; break;
    case 9: raw&=0xffffu; break;
    case 8: number=(float)edit->rating; memcpy(&raw,&number,sizeof(raw)); break;
    default: return 0;
    }
    if(!StructuredWriteScalar(&edit->definition->layout,edit->field.type.kind,
        edit->field.logical_index,edit->staged+4,edit->capacity,raw)) return 0;
    crc=(uint32_t)crc32(0,edit->staged+4,STATS_SIZE-4);
    for(i=0;i<4;++i) edit->staged[i]=(uint8_t)(crc>>(8*i));
    return 1;
}
int UpdateProfileSkills(unsigned winner,unsigned loser,const char *mode,float weight)
{
    skill_edit_t edits[2]={{0},{0}};
    unsigned clients[2]={winner,loser},i;
    int success=0;
    if(!mode || !read_skill(winner,mode,&edits[0]) ||
       !read_skill(loser,mode,&edits[1])) goto done;
    UpdateSkillRatings(&edits[0].rating,&edits[1].rating,weight);
    if(!stage_skill(&edits[0]) || !stage_skill(&edits[1])) goto done;
    for(i=0;i<2;++i) {
        edits[i].destination=ClientStatsWritable(clients[i]);
        if(!edits[i].destination || !BeginProfileUpdate(clients[i])) goto done;
    }
    /* Both reads precede both writes in IW4 5f97b4..5f985c. If the same
     * player is supplied twice, the loser write follows the winner write;
     * preserve that ordering and both dirty-map transitions. */
    for(i=0;i<2;++i) {
        memcpy(edits[i].destination,edits[i].staged,STATS_SIZE);
        MarkProfileField(clients[i],&edits[i].definition->layout,&edits[i].field,0);
    }
    success=1;
done:
    free(edits[0].staged); free(edits[1].staged);
    return success;
}
#endif
