#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "sv_state.h"
#include "server.h"
#include <stdio.h>
#include <string.h>

static int find_menu(const char *name)
{
    unsigned slot;
    if(!name || !*name) return -1;
    for(slot=0;slot<MENU_SLOTS;++slot) {
        const char *candidate=ServerMenuName(slot);
        if(candidate && !Q_stricmp(candidate,name)) return (int)slot;
    }
    return -1;
}
static gentity_t *menu_player(scr_entref_t ref)
{
    if(ref.classnum || ref.entnum>=MAX_GENTITIES) { Scr_ObjectError("Not an entity"); return NULL; }
    if(!g_entities[ref.entnum].client) { Scr_ObjectError("Entity is not a player"); return NULL; }
    return &g_entities[ref.entnum];
}
void Scr_PrecacheMenu(void)
{
    const char *name=Scr_GetString(0),*p; unsigned slot; int client;
    if(!name || !*name || strlen(name)>MAX_STRING_CHARS-32) { Scr_Error("Invalid menu name"); return; }
    for(p=name;*p;++p) if((unsigned char)*p<=32 || *p==127 || *p=='"' || *p==';' || *p=='\\') {
        Scr_Error("Invalid menu name"); return;
    }
    if(find_menu(name)>=0) return;
    for(slot=0;slot<MENU_SLOTS;++slot) {
        const char *existing=ServerMenuName(slot);
        if(existing && *existing) continue;
        if(!OL_SetConfigstring(ServerConfigstrings(),OL_CS_MENUS+slot,name)) {
            Scr_Error("Could not register menu"); return;
        }
        ((void)0);
        /* Initial gamestate reads the saved override. Only clients that have
         * received one need this reliable update, before subsequent q/u/v
         * commands resolve the menu slot in C1's4243..4274 bank. */
        if(!svs.clients || !sv_maxclients) return;
        for(client=0;client<sv_maxclients->integer;++client) {
            client_t *target=&svs.clients[client];
            if(target->state>=CS_PRIMED && target->gamestateSent)
                SV_SendServerCommandNoLoss(target,"d %u %s",OL_CS_MENUS+slot,name);
        }
        return;
    }
    Scr_Error("Too many precached menus");
}
static void indexed_menu(scr_entref_t ref,char operation,int no_mouse)
{
    gentity_t *entity=menu_player(ref); int slot; char command[32];
    if(!entity) return;
    if(entity->client->sess.connected!=CON_CONNECTED) { Scr_AddInt(0); return; }
    slot=find_menu(Scr_GetString(0));
    if(slot<0) { Scr_Error("Menu was not precached"); return; }
    snprintf(command,sizeof(command),no_mouse ? "%c %i 1" : "%c %i",operation,slot);
    SV_GameSendServerCommand(ref.entnum,1,command); Scr_AddInt(1);
}
void Scr_OpenMenu(scr_entref_t ref) { indexed_menu(ref,'q',0); }
void Scr_OpenMenuNoMouse(scr_entref_t ref) { indexed_menu(ref,'q',1); }
void Scr_OpenPopupMenu(scr_entref_t ref) { indexed_menu(ref,'u',0); }
/* Closing a menu that is not a script-menu file of its own.
 *
 * C1's 'v' handler 0x00467FC0 takes a menu-configstring SLOT, checks that the
 * slot's string (cs 4243 + slot) is not empty and hands that NAME to
 * 0x005F5790, which finds the menu by name among the loaded ones and closes
 * it. So 'v' can close any loaded menu -- field_order_get and field_order_win
 * live inside patch_common_mp's own menu files and are opened by CG_ShowSplash
 * -- but only through a slot that carries the name.
 *
 * precacheMenu cannot provide that slot for them: CG_Init (0x00450DFA ->
 * 0x00467020) loads every non-empty menu configstring in the gamestate as
 * "ui_mp/scriptmenus/<name>.menu" (0x005ECFE0, DB_FindXAssetHeader menufile)
 * and adds it to the UI (0x00607F80). For a name with no such file that is a
 * default-menufile load at every map start (PARITY asset: menufile
 * 'ui_mp/scriptmenus/field_order_get.menu' ..., 2026-10-13 19:49 round). The
 * menu configstrings are read only there (no other cgame code tests
 * 4243..4274), so a slot the GAMESTATE leaves empty is never loaded.
 *
 * A name that was not precached is closed through a slot set for this client
 * alone: d(name), v(slot), d(original name). Prefer an empty slot, but a full
 * bank may borrow its last slot because the name is restored in the same
 * ordered reliable stream. The server/inherited bank never changes, so the
 * temporary name never enters a gamestate or becomes a script-menu asset.
 * sub_530FC0 preserves menu-bank d updates and menu commands; coalescing a
 * restore across v would close the original slot's menu instead. A client whose configstrings are
 * being held back (loading, DeferConfigstringCommand) would get the 'v'
 * before the 'd', so it is skipped there; such a client has no splash up. */
static int transient_menu_slot(const char *name)
{
    int slot;
    for(slot=MENU_SLOTS-1;slot>=0;--slot) {
        const char *existing=ServerMenuName((unsigned)slot);
        if(!existing || !*existing) return slot;
    }
    (void)name;
    return MENU_SLOTS-1;
}
static void close_menu_by_name(scr_entref_t ref,const char *name)
{
    gentity_t *entity=menu_player(ref); client_t *client; int slot; char command[MAX_STRING_CHARS];
    const char *p,*original;
    if(!entity) return;
    if(!name || !*name || strlen(name)>MAX_STRING_CHARS-32) { Scr_Error("Invalid menu name"); return; }
    for(p=name;*p;++p) if((unsigned char)*p<=32 || *p==127 || *p=='"' || *p==';' || *p=='\\') {
        Scr_Error("Invalid menu name"); return;
    }
    if(entity->client->sess.connected!=CON_CONNECTED) { Scr_AddInt(0); return; }
    client=&svs.clients[ref.entnum];
    if(client->state!=CS_ACTIVE || HasDeferredConfigstrings(client)) { Scr_AddInt(0); return; }
    slot=transient_menu_slot(name);
    original=ServerMenuName((unsigned)slot);
    snprintf(command,sizeof(command),"d %u %s",(unsigned)(OL_CS_MENUS+slot),name);
    SV_GameSendServerCommand(ref.entnum,1,command);
    snprintf(command,sizeof(command),"v %i",slot);
    SV_GameSendServerCommand(ref.entnum,1,command);
    snprintf(command,sizeof(command),"d %u %s",(unsigned)(OL_CS_MENUS+slot),original ? original : "");
    SV_GameSendServerCommand(ref.entnum,1,command);
    Scr_AddInt(1);
}

/* IW4's closePopupMenu takes no menu. C1's 'v' always closes a slot's menu
 * (a bare "v" reads slot atoi("") = 0), so a name is the useful form: a
 * precached one goes by its slot, any other through close_menu_by_name. */
void Scr_ClosePopupMenu(scr_entref_t ref)
{
    gentity_t *entity;
    if(Scr_GetNumParam()) {
        const char *name=Scr_GetString(0);
        if(find_menu(name)>=0) indexed_menu(ref,'v',0);
        else close_menu_by_name(ref,name);
        return;
    }
    entity=menu_player(ref);
    if(!entity) return;
    if(entity->client->sess.connected!=CON_CONNECTED) { Scr_AddInt(0); return; }
    SV_GameSendServerCommand(ref.entnum,1,"v"); Scr_AddInt(1);
}
/* C1 tears a script menu down in two passes, so one "r" is not a full close.
 * The 'r' handler (0x00468060) validates the slot and then always takes the
 * close-everything tail at 0x004680F9, which for menu mode 6 calls 0x005F4F20:
 *
 *   top = Menus_TopVisible(0x00606E00)
 *   if (top)  { if (!anyVisibleFullScreen(0x00606FB0)) clearOpenMenuList(); }
 *   else      { if (keyCatchers & 0x10) keyCatchers &= ~0x10; }
 *
 * i.e. pass one empties the open-menu list (which is what C1's own HUD
 * predicate ShouldDrawHud/HudFade tests) and pass two releases KEYCATCH_UI,
 * which is what draws the UI cursor over the game (UI_Refresh 0x005ECE96..
 * 0x005ECEF7 requires that bit).  Emitting only the first leaves the cursor up
 * until the player happens to press a key (UI_KeyEvent 0x005F38AC).  Both
 * passes are no-ops when nothing is open and when a fullscreen menu is up, and
 * the Scaleform close at 0x006B7D60 returns immediately on the second pass, so
 * the pair is safe to send unconditionally.
 *
 * Both passes only reach the client because SV_AddServerCommand's IW3
 * replacement scan (sv_main.c sub_530FC0) skips 'q' and 'r'; it used to fold
 * the second identical unsent 'r' into the first, so the wire carried one.
 *
 * C1 ignores the slot argument beyond validating it, so closeMenu("x") and
 * closeMenu() do the same thing; the name is still resolved so a script typo
 * is reported instead of silently closing everything. */
void Scr_CloseMenu(scr_entref_t ref)
{
    char command[32]; int slot;
    if(!menu_player(ref)) return;
    if(!Scr_GetNumParam()) {
        SV_GameSendServerCommand(ref.entnum,1,"r");
        SV_GameSendServerCommand(ref.entnum,1,"r");
        return;
    }
    slot=find_menu(Scr_GetString(0));
    if(slot<0) { Scr_Error("Menu was not precached"); return; }
    snprintf(command,sizeof(command),"r %i",slot);
    SV_GameSendServerCommand(ref.entnum,1,command);
    SV_GameSendServerCommand(ref.entnum,1,command);
}
void Scr_CloseInGameMenu(scr_entref_t ref)
{
    if(menu_player(ref)) SV_GameSendServerCommand(ref.entnum,1,"H");
}
#endif
