#ifndef CODOL_P2P_PRODUCTION_RECEIVER_H
#define CODOL_P2P_PRODUCTION_RECEIVER_H
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <stdint.h>
/* Policy must be generated as read-only data IN THIS Kernel image after the
 * qualified Launcher is built. It never comes from a pipe/env/request. */
typedef struct {
    uint32_t format;
    unsigned char launcher_sha256[32],client_image_sha256[32],issuer_ed25519[32],host_sha256[32];
} p2pr_release_policy;
typedef struct {
    char map[64],gametype[32],category[16];
    int difficulty_present,difficulty;
    uint64_t uid,generation;
    unsigned role; /* 0 unknown; canonical RoleSelect commits once for intent */
} p2pr_selection;
typedef struct {
    int (__cdecl *scripts)(const void *,unsigned);
    int (__cdecl *principals)(const void *,unsigned);
    int (__cdecl *options)(const void *,unsigned);
    int (__cdecl *observe_options)(const void *,unsigned);
    int (__cdecl *active)(const void *,unsigned,uint64_t);
    int (__cdecl *observe)(const void *,unsigned,void *,unsigned);
    int (__cdecl *observe_connected)(const void *,unsigned,void *,unsigned);
    int (__cdecl *observe_server_socket)(void *,unsigned);
    int (__cdecl *current)(void);
    void (__cdecl *quarantine)(void);
    int (__cdecl *read_selection)(p2pr_selection *); /* actual C1 UID/room-intent read */
    int (__cdecl *send_probe)(const unsigned char[4],unsigned,const void *,unsigned,uint64_t);
    int (__cdecl *send_active_probe)(const unsigned char[4],unsigned,const void *,unsigned,uint64_t);
    int (__cdecl *send_initial_probe)(const unsigned char[4],unsigned,const void *,unsigned,uint64_t);
    /* These return 0 while the original bdSocket wrapper has not yet borrowed
     * the real owned socket, 1 on completion, -1 on identity/lifetime failure. */
    int (__cdecl *socket_witness)(const void *,unsigned,const unsigned char[32],uint64_t,void *,unsigned);
    int (__cdecl *guest_probe)(const unsigned char[4],unsigned,const void *,unsigned,uint64_t,
                              const unsigned char[4],unsigned,uint64_t,uint64_t);
    int (__cdecl *commit_role)(unsigned,uint64_t,uint64_t);
    int (__cdecl *running)(void); /* actual PI_RUNNING server-fiber boundary */
    int (__cdecl *install_boundary)(void); /* actual PI_INIT_PENDING server fiber */
} p2pr_native_api;
/* Capture selection only on original C1 main thread. No installer is run by
 * the IO worker. These calls do NOT call Ready, Init, Frame, connect or load DLLs. */
int P2PR_Begin(HANDLE inherited_original_parent,const wchar_t *parent_image,
               const wchar_t *pipe,const wchar_t *host_image,const p2pr_release_policy *,const p2pr_selection *,
               uint64_t original_work_deadline,uint64_t original_cleanup_deadline);
/* Called at safe native owner/fiber boundary before Init / between Frames.
 * actual is obtained again from real current C1 selection + UID/generation.
 * Returns 1 only for actual native installation/current permission, never Ready. */
int P2PR_OwnerTick(const p2pr_selection *actual,const p2pr_native_api *);
int P2PR_CurrentPermission(void);
enum { P2PR_NO_AUTHORITY,P2PR_HOST_INIT_ONLY,P2PR_INSTALLED,P2PR_ACTIVE };
unsigned P2PR_AuthorityScope(void); /* neither INSTALLED nor ACTIVE asserts Ready */
unsigned P2PR_EffectiveRole(void);
int P2PR_InitialHandoffCurrent(const p2pr_selection *);
int P2PR_GuestEndpoint(unsigned char[4],unsigned *,uint64_t *);
void P2PR_RequestStop(void);
int P2PR_Retire(uint64_t deadline); /* false => original IO/fiber resources retained */
DWORD P2PR_FirstError(void);
#endif
