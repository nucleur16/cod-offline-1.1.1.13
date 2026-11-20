/* Actual replay writer/reached-history regression and optional C1 fixture.
 * No engine services, gameplay hooks, or native clip-camera commands.
 */
#include "history.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set(player_fields_t *p,unsigned offset,uint32_t value)
{ unsigned i;for(i=0;i<PLAYER_FIELDS;++i)if(OL_PlayerField(i)->offset==offset){p->field[i]=value;return;}assert(0); }
static uint32_t get(const player_fields_t *p,unsigned offset)
{ unsigned i;for(i=0;i<PLAYER_FIELDS;++i)if(OL_PlayerField(i)->offset==offset)return p->field[i];assert(0);return 0; }
static uint32_t word(float f){uint32_t w;memcpy(&w,&f,4);return w;}
static float real(uint32_t w){float f;memcpy(&f,&w,4);return f;}
static uint32_t angle_word(float value)
{
    /* Independent transcription of C1 signed absolute-short read (59F750).
     * Inputs here avoid half-step ties; native encoder uses nearest/even. */
    double encoded=nearbyint((double)value*182.04444885253906);
    uint16_t bits=(uint16_t)(int64_t)encoded;
    return word((float)((int16_t)bits*0.0054931640625));
}
static void put32(FILE *f,uint32_t value)
{ unsigned char b[4]={(unsigned char)value,(unsigned char)(value>>8),(unsigned char)(value>>16),(unsigned char)(value>>24)};assert(fwrite(b,1,4,f)==4); }
static void record(FILE *f,unsigned kind,unsigned sequence,unsigned time,const void *data,unsigned length)
{put32(f,kind);put32(f,sequence);put32(f,time);put32(f,length);assert(fwrite(data,1,length,f)==length);}

static void regression(void)
{
    player_fields_t base={0},requested={0},held;
    delta_context_t live={20000,{0,0,0},0,0,0},replay=live;
    ol_msg_t message;unsigned char bytes[4096];unsigned axis,follow;
    const unsigned offsets[]={0x14c,0x150,0x154};
    const float values[]={-17.25f,271.5f,8.125f};
    replay.replay_view_angles=1;
    set(&base,0x1b0,2047);requested=base;
    for(axis=0;axis<3;++axis)set(&requested,offsets[axis],word(values[axis]));
    assert(ReachPlayerFields(&held,&base,&requested,1,&live));
    for(axis=0;axis<3;++axis)assert(get(&held,offsets[axis])==0);
    for(follow=0;follow<2;++follow){
        snapshot_state_t state={0};history_t *history=CreateHistory();
        const snapshot_state_t *pending,*previous;
        assert(history);
        set(&requested,0x004,follow ? 6:0);
        state.player=&requested;state.sequence=1;
        assert(PrepareHistory(history,&state,0,1,&replay));
        pending=HistoryPendingState(history);assert(pending);
        for(axis=0;axis<3;++axis)assert(get(pending->player,offsets[axis])==angle_word(values[axis]));
        MsgWriteInit(&message,bytes,sizeof(bytes));
        assert(OL_WriteSnapshot(&message,pending,NULL,NULL,NULL,0,8,1,&replay));
        assert(FinishHistory(history,1,1));
        for(axis=0;axis<3;++axis)set(&requested,offsets[axis],word(-values[axis]*0.5f));
        state.sequence=2;
        assert(PrepareHistory(history,&state,1,1,&replay));
        pending=HistoryPendingState(history);previous=FindHistoryBase(history,1,2);
        assert(pending&&previous);
        for(axis=0;axis<3;++axis)assert(get(pending->player,offsets[axis])==angle_word(-values[axis]*0.5f));
        MsgWriteInit(&message,bytes,sizeof(bytes));
        assert(OL_WriteSnapshot(&message,pending,previous,NULL,NULL,0,0,1,&replay));
        assert(FinishHistory(history,2,1));
        DestroyHistory(history);
        for(axis=0;axis<3;++axis)set(&requested,offsets[axis],word(values[axis]));
    }
    puts("PASS: replay pitch/yaw/roll reach signed-short values in full/delta normal/follow history; live omission preserved");
}

int main(int argc,char **argv)
{
    if(argc==1) regression();
    if(argc>1){
        FILE *input,*output;long length;unsigned char *gamestate;
        unsigned started,sequence=2,i,axis,live_mode,mode;
        unsigned char bytes[16384];ol_msg_t message;
        player_fields_t player={0};snapshot_state_t state={0};
        delta_context_t context={0};history_t *history=CreateHistory();
        const unsigned offsets[]={0x14c,0x150,0x154};
        const float angles[4][3]={{-15,30,5},{20,120,-7},{-25,210,9},{10,300,-3}};
        assert(argc==9);mode=(unsigned)strtoul(argv[8],NULL,0);live_mode=mode&1u;
        input=fopen(argv[1],"rb");assert(input);assert(!fseek(input,0,SEEK_END));length=ftell(input);rewind(input);
        assert(length>4&&length<0x20000);gamestate=malloc((size_t)length);assert(gamestate);
        assert(fread(gamestate,1,(size_t)length,input)==(size_t)length);fclose(input);
        output=fopen(argv[2],"wb");assert(output);started=(unsigned)strtoul(argv[3],NULL,0);
        assert(fwrite("C1CAP001",1,8,output)==8);put32(output,0);put32(output,started);
        record(output,1,1,started,gamestate,(unsigned)length);free(gamestate);
        set(&player,0x004,(mode&2u)?6:0);
        set(&player,0x1b0,2047);set(&player,0x34c,(unsigned)strtoul(argv[7],NULL,0));
        set(&player,0x1c,(unsigned)strtoul(argv[4],NULL,0));
        set(&player,0x20,(unsigned)strtoul(argv[5],NULL,0));
        set(&player,0x24,(unsigned)strtoul(argv[6],NULL,0));
        state.player=&player;context.replay_view_angles=!live_mode;
        for(i=0;i<120;++i,++sequence){
            const snapshot_state_t *pending,*base;
            context.server_time=started+50+i*100;state.sequence=sequence;
            set(&player,0,context.server_time);
            for(axis=0;axis<3;++axis)set(&player,offsets[axis],word(angles[(i/30)%4][axis]));
            assert(PrepareHistory(history,&state,i?sequence-1:0,1,&context));
            pending=HistoryPendingState(history);base=i?FindHistoryBase(history,sequence-1,sequence):NULL;
            if(!live_mode)for(axis=0;axis<3;++axis)assert(get(pending->player,offsets[axis])==angle_word(angles[(i/30)%4][axis]));
            MsgWriteInit(&message,bytes,sizeof(bytes));WriteLong(&message,0);
            assert(OL_WriteSnapshot(&message,pending,base,NULL,NULL,0,i?0:8,1,&context));
            record(output,2,sequence,context.server_time,bytes,(unsigned)message.size);
            assert(FinishHistory(history,sequence,1));
        }
        record(output,0,sequence,started+12000,"stopped",7);assert(!fclose(output));DestroyHistory(history);
        puts("PASS: actual native full/delta snapshot fixture written; no native clip-camera commands");
    }
    return 0;
}
