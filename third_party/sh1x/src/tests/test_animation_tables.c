#include "animation_tables.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned errors;
struct bgs_s *bgs;
void Com_Error(int code,const char *format,...) { assert(code==ERR_DROP && format); ++errors; }
void QDECL Com_DPrintf(conChannel_t channel,const char *format,...)
{ (void)channel; assert(format); }
int main(void)
{
    static unsigned char owner[0x9a9d0], other[4];
    anim_script_slot_t *slots[2][55];
    memset(owner,0xa5,sizeof(owner));
    ResetAnimMoveScripts(owner);
    for(int canned=0;canned<2;++canned) for(int move=0;move<55;++move) {
        anim_script_slot_t *slot=AnimMoveScript(owner,0,move,canned);
        assert(slot); slots[canned][move]=slot;
        if(move<43) assert((unsigned char *)slot==owner+(canned ? 0x116b0 : 0xc004)+516*move);
        else {
            assert(slot->count==0 && !slot->items[127]);
            slot->count=128; slot->items[0]=owner; slot->items[127]=other;
        }
        for(int c=0;c<=canned;++c) for(int m=0;m<(c==canned ? move : 55);++m)
            assert(slot!=slots[c][m]);
    }
    /* New movements cannot overwrite the old canned/state/event tables. */
    for(unsigned i=0;i<sizeof(owner);++i) assert(owner[i]==0xa5);
    assert(!AnimMoveScript(NULL,0,51,0));
    assert(!AnimMoveScript(other,0,51,0));
    assert(!AnimMoveScript(owner,1,51,0));
    assert(!AnimMoveScript(owner,-1,51,0));
    assert(!AnimMoveScript(owner,0,-1,0));
    assert(!AnimMoveScript(owner,0,55,0));
    assert(!AnimMoveScript(owner,0,51,2));
    assert(!RequireAnimMoveScript(owner,0,55,0) && errors==1);
    assert(RequireAnimMoveScript(owner,0,51,0)==slots[0][51]);
    ResetAnimMoveScripts(owner);
    for(int c=0;c<2;++c) for(int m=43;m<55;++m) {
        assert(slots[c][m]->count==0 && !slots[c][m]->items[0] && !slots[c][m]->items[127]);
    }
    ResetAnimMoveScripts(NULL);
    assert(!AnimMoveScript(owner,0,51,0));
    ResetAnimMoveScripts(owner);
    for(int event=0;event<24;++event) {
        anim_script_slot_t *slot=AnimEventScript(owner,event);
        assert(slot);
        if(event<21) assert((unsigned char *)slot==owner+0x16f60+516*event);
        else {
            assert(!slot->count && !slot->items[127]);
            slot->count=128; slot->items[127]=owner;
            for(int prior=21;prior<event;++prior) assert(slot!=AnimEventScript(owner,prior));
            for(int c=0;c<2;++c) for(int m=43;m<55;++m) assert(slot!=slots[c][m]);
        }
    }
    for(unsigned i=0;i<sizeof(owner);++i) assert(owner[i]==0xa5);
    assert(!AnimEventScript(owner,-1) && !AnimEventScript(owner,24));
    assert(!AnimEventScript(other,21) && !AnimEventScript(NULL,21));
    unsigned before_event_error=errors;
    assert(!RequireAnimEventScript(owner,24) && errors==before_event_error+1);
    ResetAnimMoveScripts(owner);
    for(int event=21;event<24;++event) {
        anim_script_slot_t *slot=RequireAnimEventScript(owner,event);
        assert(!slot->count && !slot->items[127]);
    }
    ResetAnimMoveScripts(NULL);
    assert(!AnimEventScript(owner,21));
    {
        static unsigned char state[0xae000];
        anim_condition_t condition={13,0,0};
        void *client=state+0x99a04+63*0x4cc;
        bgs=(struct bgs_s *)state;
        memset(state,0xa5,sizeof(state));
        ResetAnimMoveScripts(state);
        unsigned before=errors;
        /* An unset CoDOL-only condition reads zero for ordinary animations. */
        assert(AnimConditionMatches(client,&condition) && errors==before);
        SetAnimConditionValue(63,13,0);
        assert(AnimConditionMatches(client,&condition));
        condition.low=1; assert(!AnimConditionMatches(client,&condition));
        SetAnimConditionValue(63,13,1);
        assert(AnimConditionMatches(client,&condition));
        condition.index=16; condition.low=0; condition.high=0x80000000u;
        SetAnimConditionBit(63,16,63);
        assert(AnimConditionMatches(client,&condition));
        SetAnimConditionBit(63,16,0);
        assert(!AnimConditionMatches(client,&condition));
        for(unsigned i=0;i<sizeof(state);++i) assert(state[i]==0xa5);
        condition.index=3;
        SetAnimConditionBit(63,3,63);
        assert(AnimConditionMatches(client,&condition));
        uint32_t words[2];
        memcpy(words,state+0x99e5c+63*0x4cc+3*8,sizeof(words));
        assert(words[0]==0 && words[1]==0x80000000u);
        before=errors;
        SetAnimConditionBit(63,16,64);
        SetAnimConditionValue(64,13,0);
        SetAnimConditionValue(63,19,0);
        assert(errors==before+3);
        condition.index=13;
        assert(!AnimConditionMatches((unsigned char *)client+1,&condition));
        ResetAnimMoveScripts(state);
        before=errors;
        assert(AnimConditionMatches(client,&condition) && errors==before);
        ResetAnimMoveScripts(NULL);
    }
    {
        ResetAnimMoveScripts(owner);
        memset(owner,0xa5,sizeof(owner));
        unsigned char *item=owner+0x199b4+2047*256;
        uint32_t *count=(uint32_t *)(item+0x7c);
        *count=0;
        assert(!AnimCommands(item));
        for(unsigned i=0;i<129;++i) {
            uint32_t *cmd=AppendAnimCommand(item);
            assert(cmd && !cmd[0] && !cmd[1] && !cmd[2] && !cmd[3]);
            cmd[0]=i+1; cmd[3]=0x12345678;
            uint32_t *all=AnimCommands(item);
            for(unsigned j=0;j<=i;++j) assert(all[j*4]==j+1 && all[j*4+3]==0x12345678);
        }
        assert(*count==129);
        *count=0xa5a5a5a5;
        for(unsigned i=0;i<sizeof(owner);++i) assert(owner[i]==0xa5);
        unsigned before=errors;
        assert(!AnimCommands(item) && errors==before+1);
        assert(!AppendAnimCommand(item+1));
        assert(!AppendAnimCommand(item+256));
        ResetAnimMoveScripts(owner);
        *count=0;
        assert(!AnimCommands(item));
        assert(AppendAnimCommand(item));
        ResetAnimMoveScripts(NULL);
        assert(!AppendAnimCommand(item));
    }
    {
        memset(owner,0xa5,sizeof(owner));
        ResetAnimMoveScripts(owner);
        BeginAnimationRecords(1024);
        assert(AnimationCount==1024);
        for(unsigned i=0;i<1024;++i) {
            uint32_t *flags=(uint32_t *)(AnimationRecords+i*0x60+0x50);
            assert(!*flags); *flags=i;
        }
        for(unsigned i=0;i<1024;++i)
            assert(*(uint32_t *)(AnimationRecords+i*0x60+0x50)==i);
        for(unsigned i=0;i<sizeof(owner);++i) assert(owner[i]==0xa5);
        ResetAnimMoveScripts(owner); /* second parser pass preserves records */
        assert(AnimationCount==1024 && *(uint32_t *)(AnimationRecords+1023*0x60+0x50)==1023);
        unsigned before=errors;
        BeginAnimationRecords(0); BeginAnimationRecords(1025);
        assert(errors==before+2 && AnimationCount==1024);
        BeginAnimationRecords(873);
        for(unsigned i=0;i<sizeof(AnimationRecords);++i) assert(!AnimationRecords[i]);
        ResetAnimMoveScripts(NULL);
        assert(!AnimationCount);
    }
    puts("PASS: 1024 isolated animation records, count bounds, parser-pass preservation and shutdown reset");
    puts("PASS: dynamic animation commands preserve values across growth and reject invalid items/counts");
    puts("PASS: 55 movement slots, original ABI offsets, nonoverlapping extensions, bounds and owner reset");
    puts("PASS: native condition slots, scalar/bitset matching, high bits, explicit unknown state and bounds");
    puts("PASS: 24 event slots, unchanged legacy offsets, isolated storage, bounds and reset");
}
