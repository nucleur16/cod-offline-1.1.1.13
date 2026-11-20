/* Player-hosted servers: end with the client that hosts them.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Kernel33 (the CoDOL client loader, feat/p2p-hosting, docs/P2P_HOSTING.md
 * there) runs this server as a child of the host's game client and passes
 * +set hostParentPid <client pid>. The client also puts the server in a
 * job object that ends it with the client, but a job is a hard kill; and a
 * server started by hand for a client (or a job assignment that failed, when
 * the client itself runs in a job that forbids nesting) has no such guard.
 * Once a second this checks that the host process still runs. When it has
 * ended, the server quits through the normal path (Com_Quit_f: the match
 * report spool, sessions and logs are written as on an rcon quit), so a
 * player-hosted match never outlives its host and never keeps the port.
 *
 * 0 (the default) disables the check: a dedicated server has no parent. */
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#endif
#include <stdio.h>
#include "q_shared.h"
#include "cvar.h"
#include "cmd.h"
#include "qcommon_io.h"
#include "sys_main.h"
#include "host_parent.h"

static cvar_t *host_parent;
static int watched_pid;
static int last_check;
static int quitting;
#ifdef _WIN32
static HANDLE watched;
#endif

void HostParentInit(void)
{
    host_parent = Cvar_RegisterInt("hostParentPid", 0, 0, 0x7fffffff, CVAR_INIT,
        "Player-hosted server: quit when the host client with this process id ends (0 = dedicated, no check)");
}

/* 1 = the host still runs, 0 = it has ended, -1 = cannot tell (not checked). */
static int parent_alive(int pid)
{
#ifdef _WIN32
    if (pid != watched_pid) {
        if (watched) CloseHandle(watched);
        watched = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
        watched_pid = pid;
        if (!watched) {
            /* No such process (it ended before we looked) is the only failure
             * that means "gone"; anything else (access) means we cannot tell. */
            return GetLastError() == ERROR_INVALID_PARAMETER ? 0 : -1;
        }
    }
    if (!watched) return -1;
    return WaitForSingleObject(watched, 0) == WAIT_OBJECT_0 ? 0 : 1;
#else
    watched_pid = pid;
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno == ESRCH ? 0 : -1;
#endif
}

void HostParentFrame(void)
{
    int now, pid, alive;
    if (!host_parent || !host_parent->integer || quitting) return;
    now = Sys_Milliseconds();
    if (last_check && now - last_check < 1000) return;
    last_check = now;
    pid = host_parent->integer;
    alive = parent_alive(pid);
    if (alive == -1 && watched_pid == pid) {
        static int warned;
        if (!warned) {
            warned = 1;
            Com_Printf(CON_CHANNEL_SERVER, "hostParentPid: cannot watch process %d; this server will not follow its host\n", pid);
        }
        return;
    }
    if (alive) return;
    quitting = 1;
    Com_Printf(CON_CHANNEL_SERVER, "hostParentPid: host client %d has ended; quitting\n", pid);
    Cbuf_AddText("quit\n");
}
