/* Same-directory replacement for session state: readers see a complete old
 * file or a complete new file, never a partially written one.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "atomic_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

int AtomicWriteFile(const char *path, const void *data, size_t length)
{
    char *temporary;
    FILE *file;
    size_t path_length;
    int ok = 0;
    if(!path || !*path || (!data && length)) return 0;
    path_length = strlen(path);
    if(path_length > (size_t)-1 - 5) return 0;
    temporary = malloc(path_length + 5);
    if(!temporary) return 0;
    memcpy(temporary, path, path_length);
    memcpy(temporary + path_length, ".tmp", 5);
    file = fopen(temporary, "wb");
    if(!file) goto done;
#ifndef _WIN32
    /* Some mounted home directories do not support chmod. Keep the server
     * usable there; a normal POSIX filesystem stores this state as 0600. */
    (void)fchmod(fileno(file), 0600);
#endif
    if(length && fwrite(data, 1, length, file) != length) goto close;
    /* No fsync/_commit and no MOVEFILE_WRITE_THROUGH. The replacement is
     * still atomic for every reader and survives a process exit or crash
     * (the OS writes its cache back on its own); only a power loss inside
     * the cache window could bring back the previous file, and the client's
     * association does not survive that either. The forced flush cost
     * 20-130 ms per save on the server's own disk, measured 2026-10-13, and
     * the session file is saved from the main thread every few seconds. */
    if(fflush(file)) goto close;
    if(fclose(file)) { file = NULL; goto done; }
    file = NULL;
#ifdef _WIN32
    ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    ok = rename(temporary, path) == 0;
#endif
    goto done;
close:
    fclose(file);
done:
    if(!ok) remove(temporary);
    free(temporary);
    return ok;
}

#ifdef OL_SERVER
#include "filesystem.h"
#include "cvar.h"

int AtomicWriteHomeFile(const char *qpath, const void *data, size_t length)
{
    char path[MAX_OSPATH];
    size_t n;
    if(!qpath || !*qpath || !fs_homepath || !fs_homepath->string) return 0;
    FS_BuildOSPathForThread(fs_homepath->string, qpath, "", path, 0);
    n = strlen(path);
    if(!n || (path[n-1] != '/' && path[n-1] != '\\')) return 0;
    path[n-1] = 0;
    return AtomicWriteFile(path, data, length);
}
#else
int AtomicWriteHomeFile(const char *qpath, const void *data, size_t length)
{ (void)qpath; (void)data; (void)length; return 0; }
#endif
