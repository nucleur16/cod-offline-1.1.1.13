#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* See weapon_input.h for why this is bound structurally and not by time. */
#ifdef OL_SERVER
#include "weapon_input.h"
#include <string.h>

typedef struct {
    input_kind_t kind;
    ol_usercmd_t raw;
    unsigned char raw_valid;
    unsigned raw_buttons;
    unsigned char remote_angles[2];
    unsigned char remote_angles_valid;
} staged_input_t;

static staged_input_t staged[MAX_CLIENTS];
static weapon_input_t executed[MAX_CLIENTS];
static unsigned skipped_raw_buttons[MAX_CLIENTS];
static unsigned char skipped_raw_valid[MAX_CLIENTS];

static void stage(unsigned client, input_kind_t kind, unsigned raw)
{
    if(client >= MAX_CLIENTS) return;
    staged[client].kind = kind;
    memset(&staged[client].raw, 0, sizeof(staged[client].raw));
    staged[client].raw_valid = 0;
    staged[client].raw_buttons = raw;
    memset(staged[client].remote_angles, 0, sizeof(staged[client].remote_angles));
    staged[client].remote_angles_valid = 0;
}

void StageNetworkWeaponInput(unsigned client, unsigned raw_buttons)
{
    stage(client, INPUT_NETWORK, raw_buttons);
}
void StageNetworkCommandInput(unsigned client, const ol_usercmd_t *command)
{
    if(client >= MAX_CLIENTS) return;
    if(!command) { stage(client, INPUT_NONE, 0); return; }
    stage(client, INPUT_NETWORK, command->buttons);
    staged[client].raw = *command;
    staged[client].raw_valid = 1;
    memcpy(staged[client].remote_angles, command->remoteAngles, 2);
    staged[client].remote_angles_valid = 1;
}
void StageBotCommandInput(unsigned client,const ol_usercmd_t *command)
{
    if(client>=MAX_CLIENTS)return;
    stage(client,INPUT_BOT,command?command->buttons:0);
    if(command){staged[client].raw=*command;staged[client].raw_valid=1;}
}
void StageBotWeaponInput(unsigned client)
{
    stage(client, INPUT_BOT, 0);
}
void StageSyntheticWeaponInput(unsigned client)
{
    /* A synthetic producer does not reach the acceptance point, so it publishes
     * its executed input directly and leaves nothing staged behind it. */
    if(client >= MAX_CLIENTS) return;
    stage(client, INPUT_NONE, 0);
    memset(&executed[client], 0, sizeof(executed[client]));
    executed[client].kind = INPUT_SYNTHETIC;
    executed[client].raw_buttons = 0;
    executed[client].native_buttons = 0;
    executed[client].previous_raw_buttons = 0;
    executed[client].previous_raw_valid = 0;
}

void BindExecutedWeaponInput(unsigned client, const usercmd_t *native)
{
    weapon_input_t previous;
    if(client >= MAX_CLIENTS) return;
    /* Nothing staged means the command came from a producer that does not send
     * a CoDOL command -- the bot path, or an internal caller. It is recorded as
     * such rather than inheriting the last network command's buttons. */
    /* The command this one follows keeps its raw word here, so an edge on a
     * CoDOL-only button can be measured. A previous command that carried no
     * CoDOL word is recorded as having none, not as a zero word. */
    previous = executed[client];
    executed[client].kind = staged[client].kind == INPUT_NETWORK ?
        INPUT_NETWORK : INPUT_BOT;
    executed[client].raw = staged[client].raw;
    executed[client].raw_valid =
        staged[client].raw_valid;
    executed[client].raw_buttons = (staged[client].kind == INPUT_NETWORK || staged[client].raw_valid) ?
        staged[client].raw_buttons : 0u;
    executed[client].native_buttons = native ? (unsigned)native->buttons : 0u;
    executed[client].previous_raw_buttons =
        (previous.kind == INPUT_NETWORK || previous.raw_valid) ? previous.raw_buttons : 0u;
    executed[client].previous_raw_valid =
        (previous.kind == INPUT_NETWORK || previous.raw_valid) ? 1u : 0u;
    /* An equal-millisecond duplicate received after that command is the
     * newer previous word (see NoteSkippedNetworkCommandInput). */
    if(skipped_raw_valid[client]) {
        executed[client].previous_raw_buttons = skipped_raw_buttons[client];
        executed[client].previous_raw_valid = 1u;
        skipped_raw_valid[client] = 0;
    }
    executed[client].remote_angles_valid =
        staged[client].kind == INPUT_NETWORK && staged[client].remote_angles_valid;
    memset(executed[client].remote_angles, 0, sizeof(executed[client].remote_angles));
    if(executed[client].remote_angles_valid)
        memcpy(executed[client].remote_angles, staged[client].remote_angles, 2);
    stage(client, INPUT_NONE, 0);
}

void BeginSyntheticWeaponInput(unsigned client, weapon_input_scope_t *saved)
{
    if(client >= MAX_CLIENTS || !saved) return;
    saved->executed = executed[client];
    saved->staged_kind = staged[client].kind;
    saved->staged_raw = staged[client].raw;
    saved->staged_raw_valid = staged[client].raw_valid;
    saved->staged_raw_buttons = staged[client].raw_buttons;
    memcpy(saved->staged_remote_angles, staged[client].remote_angles, 2);
    saved->staged_remote_angles_valid = staged[client].remote_angles_valid;
    StageSyntheticWeaponInput(client);
}
void EndSyntheticWeaponInput(unsigned client, const weapon_input_scope_t *saved)
{
    if(client >= MAX_CLIENTS || !saved) return;
    executed[client] = saved->executed;
    staged[client].kind = saved->staged_kind;
    staged[client].raw = saved->staged_raw;
    staged[client].raw_valid = saved->staged_raw_valid;
    staged[client].raw_buttons = saved->staged_raw_buttons;
    memcpy(staged[client].remote_angles, saved->staged_remote_angles, 2);
    staged[client].remote_angles_valid = saved->staged_remote_angles_valid;
}

void NoteSkippedNetworkCommandInput(unsigned client, const ol_usercmd_t *command)
{
    if(client >= MAX_CLIENTS || !command) return;
    skipped_raw_buttons[client] = command->buttons;
    skipped_raw_valid[client] = 1;
}

void DiscardStagedWeaponInput(unsigned client)
{
    if(client >= MAX_CLIENTS) return;
    stage(client, INPUT_NONE, 0);
}

int ExecutedWeaponInput(unsigned client, weapon_input_t *out)
{
    if(client >= MAX_CLIENTS || !out) return 0;
    if(executed[client].kind == INPUT_NONE) return 0;
    *out = executed[client];
    return 1;
}

int ExecutedUsercmd(unsigned client, ol_usercmd_t *out)
{
    if(client >= MAX_CLIENTS || !out ||
       !executed[client].raw_valid)
        return 0;
    *out = executed[client].raw;
    return 1;
}

void ResetWeaponInput(unsigned client)
{
    if(client >= MAX_CLIENTS) return;
    memset(staged + client, 0, sizeof(staged[client]));
    memset(executed + client, 0, sizeof(executed[client]));
    skipped_raw_valid[client] = 0;
}
#endif

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentWeaponInputEmpty(void) {return resident_static_zero(staged,sizeof(staged)) && resident_static_zero(executed,sizeof(executed)) && resident_static_zero(skipped_raw_valid,sizeof(skipped_raw_valid));}
#endif
