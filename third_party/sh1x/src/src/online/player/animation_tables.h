#ifndef ANIMATION_TABLES_H
#define ANIMATION_TABLES_H
#include <stdint.h>
/* server.h leaks a bare #pragma pack(1) into every translation unit that
 * includes it, so a CoDOL header whose structs are not wrapped gets a
 * different layout in those units than in the ones that define them. Wrap
 * the whole header, as actors.h does. Verified with
 * obj/claude-workers/packcheck.py. */
#pragma pack(push, 8)
#define ANIM_MOVE_COUNT 55
#define ANIM_LEGACY_MOVE_COUNT 43
#define ANIM_EVENT_COUNT 24
#define ANIM_LEGACY_EVENT_COUNT 21
#define ANIM_RECORD_COUNT 1024
#define ANIM_CONDITION_COUNT 19
#define ANIM_LEGACY_CONDITION_COUNT 10
typedef struct { int index; uint32_t low,high; } anim_condition_t;
typedef struct {
    uint32_t count;
    void *items[128];
} anim_script_slot_t;
#ifdef __cplusplus
extern "C" {
#endif
/* The native parser has one active globalScriptData owner. Extended slots
 * borrow its script-item pointers and are reset for each parsing pass. */
void ResetAnimMoveScripts(void *owner);
anim_script_slot_t *AnimMoveScript(void *owner,int state,int movement,int canned);
anim_script_slot_t *RequireAnimMoveScript(void *owner,int state,int movement,int canned);
anim_script_slot_t *AnimEventScript(void *owner,int event);
anim_script_slot_t *RequireAnimEventScript(void *owner,int event);
void SetAnimConditionValue(int client,int condition,uint64_t value);
void SetAnimConditionBit(int client,int condition,int bit);
struct playerState_s;
void UpdateAnimConditionsFromPlayerState(int client,const struct playerState_s *ps);
/* C1 PM_MantleMove 0x0040a881..0x0040a8b3: the "fastmantle" condition is
 * the mantle's anim-set bit, written just before the mantle animation. */
void SetFastMantleAnimCondition(int client,int fast);
int AnimConditionMatches(const void *client_info,const anim_condition_t *condition);
/* Native command ABI: two body parts, two animation indices, two durations,
 * and a sound-alias pointer. Storage may move only on append. */
void *AppendAnimCommand(void *item);
void *AnimCommands(void *item);
extern unsigned char AnimationRecords[ANIM_RECORD_COUNT*0x60];
extern unsigned AnimationCount;
void BeginAnimationRecords(unsigned count);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
