#ifndef CODOL_P2P_WIN_PIPE_H
#define CODOL_P2P_WIN_PIPE_H
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct p2pt_owner p2pt_owner;
typedef DWORD (WINAPI *p2pt_work)(void *);
/* One original session per process. All contexts/events/buffers are enrolled
 * before any asynchronous API. Unknown retirement blocks subsequent sessions. */
p2pt_owner *P2PT_New(HANDLE original_peer,DWORD original_pid,
                     const wchar_t *original_image,uint64_t cleanup_deadline);
int P2PT_Server(p2pt_owner *,const wchar_t *name);
/* Before worker creation: replace the provisional own-parent reference with
 * the exact original suspended CreateProcess child HANDLE, never a PID open. */
int P2PT_BindOriginalPeer(p2pt_owner *,HANDLE original,DWORD pid,const wchar_t *image);
HANDLE P2PT_CancellationEvent(p2pt_owner *); /* borrowed, worker lifetime only */
int P2PT_PeerIdentity(p2pt_owner *,DWORD *pid,uint64_t *birth);
int P2PT_Client(p2pt_owner *,const wchar_t *name,uint64_t deadline);
int P2PT_Connect(p2pt_owner *,uint64_t deadline);
int P2PT_Start(p2pt_owner *,p2pt_work work,void *context);
int P2PT_Read(p2pt_owner *,void *bytes,DWORD size,uint64_t deadline);
int P2PT_Write(p2pt_owner *,const void *bytes,DWORD size,uint64_t deadline);
int P2PT_Current(p2pt_owner *);
uint64_t P2PT_Now(void);
void P2PT_RequestStop(p2pt_owner *);
/* Caller must keep work/context alive until this returns 1. It never kills or
 * adopts a thread, closes a pending operation or retries a failed close. */
int P2PT_JoinClose(p2pt_owner *,uint64_t deadline);
int P2PT_Destroy(p2pt_owner **); /* only after physical retirement */
DWORD P2PT_FirstError(const p2pt_owner *);
int P2PT_RetirementBlocked(void);
#ifdef __cplusplus
}
#endif
#endif
