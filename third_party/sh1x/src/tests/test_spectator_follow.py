"""Run the real ordinary/forced follow assembly and direct-follow C selection.

Only SpectatorClientEndFrame's production assembly body is extracted, without
rewriting its instructions. Engine calls are bounded boundary doubles. This
checks copied playerstate ownership, not rendered C1 camera acceptance.
Run on the Linux build host: python3 tests/test_spectator_follow.py
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include "g_shared.h"
#include "g_sv_shared.h"
#include "playerstate_ext.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x); exit(1); } } while(0)
_Static_assert(sizeof(playerState_t)==0x2f64,"retained follow memcpy size");
_Static_assert(offsetof(gclient_t,spectatorClient)==0x3074,"follow target offset");
_Static_assert(offsetof(gclient_t,sess.forceSpectatorClient)==0x2f68,"forced target offset");
_Static_assert(offsetof(gentity_t,client)==0x15c,"entity client offset");
_Static_assert(offsetof(clientState_t,team)==4,"archive team offset");
level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
static gclient_t clients[3];
static playerState_t target;
static clientState_t target_cs;
static int stopped,copied,fetches;
extern void SpectatorClientEndFrame(gentity_t *ent);
extern qboolean Cmd_FollowClient_f(gentity_t *ent,int clientnum);
void Com_Printf(conChannel_t ch,const char *fmt,...) { (void)ch; (void)fmt; }
int SV_GetArchivedClientInfo(int number,int *at,playerState_t *ps,clientState_t *cs,float *origin,int *health,int *flags,ps_ext_t *ext)
{
    CHECK(number==1); (void)at;(void)origin;(void)ext;
    ++fetches; *ps=target;*cs=target_cs;*health=target.stats[0];*flags=target.otherFlags;return 1;
}
int PlayerStateExtApply(const playerState_t *ps,const ps_ext_t *ext)
{ (void)ps;(void)ext;++copied;return 1; }
void HudElem_UpdateClient(gclient_t *cl,int number,int flags)
{ (void)cl;(void)number;(void)flags; }
void StopFollowing(gentity_t *ent)
{ ++stopped;ent->client->spectatorClient=-1;ent->client->ps.otherFlags&=~POF_FOLLOW; }
int Cmd_FollowCycle_f(gentity_t *ent,int direction)
{ (void)ent;(void)direction;return 0; }
static void setup(int hp,int forced)
{
    memset(clients,0,sizeof(clients));memset(g_entities,0,sizeof(g_entities));
    memset(&target,0,sizeof(target));memset(&target_cs,0,sizeof(target_cs));
    level.clients=clients;level.maxclients=3;
    g_entities[0].client=clients;clients[0].sess.sessionState=SESS_STATE_SPECTATOR;
    clients[0].sess.forceSpectatorClient=forced?1:-1;
    clients[0].sess.killCamEntity=-1;clients[0].spectatorClient=1;
    clients[0].sess.archiveTime=forced?500:0;
    clients[0].ps.otherFlags=POF_FOLLOW;clients[0].ps.origin[2]=100;
    clients[1].sess.connected=CON_CONNECTED;clients[1].sess.cs.team=TEAM_BLUE;
    clients[1].sess.sessionState=hp>0?SESS_STATE_PLAYING:SESS_STATE_DEAD;
    clients[1].ps.stats[0]=hp;
    target.stats[0]=hp;target.pm_type=hp>0?0:8;target.clientNum=1;
    target.otherFlags=POF_PLAYER;target.origin[2]=hp>0?56:-500;
    target_cs.team=TEAM_BLUE;target_cs.clientIndex=1;
    stopped=copied=fetches=0;
}
int main(void)
{
    setup(100,0);SpectatorClientEndFrame(g_entities);
    CHECK(stopped==0&&copied==1&&clients[0].ps.clientNum==1&&clients[0].ps.origin[2]==56);
    puts("PASS ordinary live follow copies the target state");
    target.stats[0]=0;target.pm_type=8;target.origin[2]=-500;
    SpectatorClientEndFrame(g_entities);
    CHECK(stopped==1&&copied==1&&clients[0].spectatorClient==-1&&clients[0].ps.origin[2]==56);
    puts("PASS followed target death cannot copy dead camera state");
    setup(-1,0);SpectatorClientEndFrame(g_entities);
    CHECK(stopped==1&&copied==0&&clients[0].ps.origin[2]==100);
    puts("PASS negative-health ordinary target rejected before memcpy");
    setup(100,0);clients[0].sess.noSpectate=1<<TEAM_BLUE;
    SpectatorClientEndFrame(g_entities);CHECK(stopped==1&&copied==0);
    puts("PASS ordinary team permissions remain enforced");
    setup(0,1);SpectatorClientEndFrame(g_entities);
    CHECK(stopped==0&&copied==1&&fetches==1&&clients[0].ps.pm_type==8&&clients[0].ps.killCamEntity==ENTITYNUM_NONE);
    puts("PASS intentional forced killcam preserves archived dead target");
    setup(0,0);clients[0].spectatorClient=-1;
    CHECK(!Cmd_FollowClient_f(g_entities,1)&&clients[0].spectatorClient==-1);
    puts("PASS direct follow rejects a connected dead teammate");
    setup(100,0);clients[0].spectatorClient=-1;
    CHECK(Cmd_FollowClient_f(g_entities,1)&&clients[0].spectatorClient==1);
    puts("PASS direct follow accepts a connected living teammate");
    puts("ALL PASS: native ordinary follow and direct selection");
    return 0;
}
'''

def run(command):
    result = subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
    if result.returncode:
        raise SystemExit((result.stdout+result.stderr)[-10000:])
    return result.stdout


def main():
    source=(ROOT/'src/asmsource/_g_active_mp.asm').read_text()
    start=source.index('SpectatorClientEndFrame:\n')
    end=source.index(';Initialized global or static variables of g_active_mp:',start)
    defines='\n'.join(re.findall(r'^%define POF_.*$',source,re.M))
    prefix=('BITS 32\n'+defines+'\nsection .text\nglobal SpectatorClientEndFrame\n'
            'extern SV_GetArchivedClientInfo, StopFollowingOnDeath, Cmd_FollowCycle_f\n'
            'extern memcpy, PlayerStateExtApply, HudElem_UpdateClient\n')
    includes=['-I'+str(ROOT/'src')]+['-I'+str(p) for p in sorted((ROOT/'src/online').iterdir()) if p.is_dir()]
    with tempfile.TemporaryDirectory(prefix='codol-spectator-follow-') as temp:
        temp=Path(temp)
        (temp/'follow.asm').write_text(prefix+source[start:end])
        (temp/'follow.c').write_text(HARNESS)
        run(['nasm','-f','elf32','-DOL_SERVER',str(temp/'follow.asm'),'-o',str(temp/'follow.o')])
        flags=['-m32','-O1','-g','-D_GNU_SOURCE','-DOL_SERVER','-ffunction-sections','-fdata-sections']+includes
        run(['gcc']+flags+['-DNDEBUG','-c','src/g_sv_cmds.c','-o',str(temp/'direct.o')])
        run(['gcc']+flags+['-no-pie',str(temp/'follow.c'),str(temp/'follow.o'),str(temp/'direct.o'),'-Wl,--gc-sections','-o',str(temp/'follow')])
        print(run([str(temp/'follow')]),end='')

if __name__=='__main__':
    main()
