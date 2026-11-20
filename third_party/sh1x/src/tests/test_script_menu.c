#include "scr_vm_functions.h"
#include "sv_state.h"
#include "server.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

gentity_t g_entities[MAX_GENTITIES];
serverStatic_t svs;
static gclient_t player;
static cvar_t maxclients;
cvar_t *sv_maxclients=&maxclients;

static const char *argument="menu";
static int parameters=1,errors,result=-1,recipient,game_sends,reads,updates;
static int update_recipients[8];
static char command[1100],previous_command[1100],update_commands[8][1100],game_commands[128][1100];
static int deferred;

int HasDeferredConfigstrings(const client_t *client) { (void)client; return deferred; }

int Q_stricmp(const char *a,const char *b) { return strcasecmp(a,b); }
void Scr_ObjectError(const char *s) { assert(s); ++errors; }
void Scr_Error(const char *s) { assert(s); ++errors; }
char *Scr_GetString(unsigned i) { assert(!i); ++reads; return (char *)argument; }
int Scr_GetNumParam(void) { return parameters; }
void Scr_AddInt(int value) { result=value; }
void QDECL Com_DPrintf(conChannel_t channel,const char *format,...)
{ (void)channel; (void)format; }
int RefreshMapSunSampleSize(void) { return 1; }
void ResetMapSunSampleSize(void) {}
void SV_GameSendServerCommand(int client,int type,const char *text)
{
    assert(type==1 && strlen(text)<sizeof(command));
    recipient=client;
    strcpy(previous_command,command);
    strcpy(command,text);
    assert(game_sends<128);strcpy(game_commands[game_sends],text);
    ++game_sends;
}
void QDECL SV_SendServerCommandNoLoss(client_t *client,const char *format,...)
{
    va_list args;
    assert(client>=svs.clients && client<svs.clients+maxclients.integer);
    assert(updates<(int)(sizeof(update_recipients)/sizeof(update_recipients[0])));
    update_recipients[updates]=(int)(client-svs.clients);
    va_start(args,format);
    assert(vsnprintf(update_commands[updates],sizeof(update_commands[updates]),format,args)>0);
    va_end(args);
    ++updates;
}
static void expect_update(int index,int client,const char *text)
{
    assert(index<updates && update_recipients[index]==client);
    assert(!strcmp(update_commands[index],text));
}

int main(void)
{
    scr_entref_t ref={3,0};
    char menu_index[16],second_index[16];
    const char *cells[4];
    int previous;

    snprintf(menu_index,sizeof(menu_index),"%u",OL_CS_MENUS);
    snprintf(second_index,sizeof(second_index),"%u",OL_CS_MENUS+1);
    cells[0]=menu_index; cells[1]="menu";
    cells[2]=second_index; cells[3]="already";
    assert(SetServerInheritedConfigstrings("mp",2,cells));
    assert(!strcmp(ServerMenuName(0),"menu"));
    assert(!strcmp(ServerMenuName(1),"already"));
    assert(ServerMenuName(MENU_SLOTS)==NULL);

    g_entities[3].client=&player;
    Scr_OpenPopupMenu(ref);
    assert(result==0 && !game_sends && !reads);
    player.sess.connected=CON_CONNECTED;
    Scr_OpenPopupMenu(ref);
    assert(result==1 && recipient==3 && !strcmp(command,"u 0"));
    Scr_ClosePopupMenu(ref); assert(!strcmp(command,"v 0"));
    Scr_OpenMenu(ref); assert(!strcmp(command,"q 0"));
    Scr_OpenMenuNoMouse(ref); assert(!strcmp(command,"q 0 1"));
    previous=game_sends;
    Scr_CloseMenu(ref);
    assert(game_sends==previous+2 && !strcmp(command,"r 0"));
    parameters=0; previous=game_sends;
    Scr_CloseMenu(ref);
    assert(game_sends==previous+2 && !strcmp(command,"r"));
    Scr_CloseInGameMenu(ref); assert(!strcmp(command,"H"));

    maxclients.integer=4;
    svs.clients[0].state=CS_ACTIVE; svs.clients[0].gamestateSent=qtrue;
    svs.clients[1].state=CS_PRIMED; svs.clients[1].gamestateSent=qtrue;
    svs.clients[2].state=CS_PRIMED; svs.clients[2].gamestateSent=qfalse;
    svs.clients[3].state=CS_CONNECTED; svs.clients[3].gamestateSent=qtrue;
    parameters=1;
    argument="MENU";
    Scr_PrecacheMenu(); assert(!updates);
    argument="new_menu";
    Scr_PrecacheMenu();
    assert(updates==2 && !strcmp(ServerMenuName(2),"new_menu"));
    expect_update(0,0,"d 4245 new_menu");
    expect_update(1,1,"d 4245 new_menu");
    Scr_ClosePopupMenu(ref); assert(!strcmp(command,"v 2"));

    assert(OL_SetConfigstring(ServerConfigstrings(),OL_CS_MENUS,""));
    assert(!strcmp(ServerMenuName(0),""));
    argument="replacement";
    Scr_PrecacheMenu();
    assert(updates==4 && !strcmp(ServerMenuName(0),"replacement"));
    expect_update(2,0,"d 4243 replacement");
    expect_update(3,1,"d 4243 replacement");
    previous=updates; argument="REPLACEMENT";
    Scr_PrecacheMenu(); assert(updates==previous);

    /* A menu that was not precached (field_order_get lives inside another
     * menu file) is closed through a free slot set for this client alone,
     * the top one, and never through the server's configstrings: the
     * gamestate would make C1's CG_Init load it as a menu file. */
    previous=game_sends; argument="field_order_get"; result=-1;
    Scr_ClosePopupMenu(ref);            /* client 3 is still connecting */
    assert(!errors && game_sends==previous && result==0);
    svs.clients[3].state=CS_ACTIVE;
    deferred=1;
    Scr_ClosePopupMenu(ref);            /* its configstrings are held back */
    assert(!errors && game_sends==previous && result==0);
    deferred=0;
    Scr_ClosePopupMenu(ref);
    assert(!errors && game_sends==previous+3 && result==1 && recipient==3);
    assert(!strcmp(game_commands[previous],"d 4274 field_order_get") && !strcmp(game_commands[previous+1],"v 31"));
    assert(!strcmp(command,"d 4274 "));
    assert((!ServerMenuName(31) || !*ServerMenuName(31)) && updates==4);
    /* Survival's MP constant menu bank can fill all 32 C1 slots. Loading
     * clients need no close, and an active close borrows/restores the last
     * slot without changing server or inherited configstrings. */
    { char name[40];unsigned i;
      for(i=3;i<MENU_SLOTS;++i) {
          snprintf(name,sizeof(name),"full_menu_%u",i);
          assert(OL_SetConfigstring(ServerConfigstrings(),OL_CS_MENUS+i,name));
      }
      assert(!strcmp(ServerMenuName(31),"full_menu_31"));
      previous=game_sends;svs.clients[3].state=CS_CONNECTED;
      Scr_ClosePopupMenu(ref);
      assert(!errors && game_sends==previous && result==0);
      svs.clients[3].state=CS_ACTIVE;deferred=1;
      Scr_ClosePopupMenu(ref);
      assert(!errors && game_sends==previous && result==0);
      deferred=0;Scr_ClosePopupMenu(ref);
      assert(!errors && result==1 && game_sends==previous+3);
      assert(!strcmp(game_commands[previous],"d 4274 field_order_get"));
      assert(!strcmp(game_commands[previous+1],"v 31"));
      assert(!strcmp(game_commands[previous+2],"d 4274 full_menu_31"));
      assert(!strcmp(ServerMenuName(31),"full_menu_31"));
      argument="field_order_win";previous=game_sends;
      Scr_ClosePopupMenu(ref);
      assert(!errors && game_sends==previous+3 && !strcmp(game_commands[previous],"d 4274 field_order_win"));
      assert(!strcmp(game_commands[previous+2],"d 4274 full_menu_31"));
      argument="extra_script_menu";
      Scr_PrecacheMenu();
      assert(errors==1 && !strcmp(ServerMenuName(31),"full_menu_31"));
    }
    previous=game_sends;
    argument="bad\nname";
    Scr_ClosePopupMenu(ref); assert(errors==2 && game_sends==previous);
    Scr_PrecacheMenu(); assert(errors==3 && updates==4);
    argument="menu";
    ref.classnum=1;
    Scr_ClosePopupMenu(ref); assert(errors==4 && game_sends==previous);
    ref.classnum=0; ref.entnum=4;
    Scr_ClosePopupMenu(ref); assert(errors==5 && game_sends==previous);

    ResetServerConfigstrings();
    puts("PASS: production menu lookup, client gated precache, and native menu commands");
}
