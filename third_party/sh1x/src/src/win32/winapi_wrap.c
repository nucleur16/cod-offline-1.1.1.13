/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm
    Copyright (C) 1999-2005 Id Software, Inc.

    This file is part of CoD4X18-Server source code.

    CoD4X18-Server source code is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    CoD4X18-Server source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
===========================================================================
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>


/* The last _VirtualAlloc that failed, for Sys_DescribeMemory: every allocation of the retained com_memory
 * assembly comes through here, and its out-of-memory error says neither which one failed nor why. */
static struct { LPVOID address; SIZE_T size; DWORD type; DWORD error; void *caller; unsigned count; } s_allocFailure;

LPVOID __cdecl _VirtualAlloc(LPVOID address, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
{
    LPVOID p = VirtualAlloc(address, dwSize, flAllocationType, flProtect);
    if (!p)
    {
        s_allocFailure.error = GetLastError();
        s_allocFailure.address = address;
        s_allocFailure.size = dwSize;
        s_allocFailure.type = flAllocationType;
        s_allocFailure.caller = __builtin_return_address(0);
        ++s_allocFailure.count;
    }
    return p;
}

/* One line on why an allocation cannot be had: the last failed _VirtualAlloc (address, size, type, error), the
 * process's address space (64 KB reservable units: total, largest, and the largest under 2 GB) and what the
 * system can still commit. */
void Sys_DescribeMemory(char *out, int outSize)
{
    MEMORY_BASIC_INFORMATION m;
    MEMORYSTATUSEX st;
    char failed[128];
    ULONG_PTR at = 0, lo, hi;
    SIZE_T total = 0, largest = 0, largestLow = 0, used;
    unsigned blocks = 0;

    while (VirtualQuery((LPCVOID)at, &m, sizeof(m)) == sizeof(m))
    {
        lo = (ULONG_PTR)m.BaseAddress;
        hi = lo + m.RegionSize;
        if (hi <= lo) break;
        if (m.State == MEM_FREE)
        {
            lo = (lo + 0xffff) & ~(ULONG_PTR)0xffff;
            used = hi & ~(ULONG_PTR)0xffff;
            if (lo && used > lo)
            {
                ++blocks;
                total += used - lo;
                if (used - lo > largest) largest = used - lo;
                if (lo < 0x80000000u && used - lo > largestLow) largestLow = used - lo;
            }
        }
        at = hi;
    }
    failed[0] = 0;
    if (s_allocFailure.count)
        _snprintf(failed, sizeof(failed) - 1, "VirtualAlloc(%p, 0x%lx, 0x%lx) from %p failed with %lu (%u so far); ",
                  s_allocFailure.address, (unsigned long)s_allocFailure.size, s_allocFailure.type, s_allocFailure.caller,
                  s_allocFailure.error, s_allocFailure.count);
    failed[sizeof(failed) - 1] = 0;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) memset(&st, 0, sizeof(st));
    _snprintf(out, outSize - 1, "%saddress space free %lu MB in %u blocks, largest %lu KB (%lu KB under 2 GB); "
              "system commit free %lu MB, physical free %lu MB", failed, (unsigned long)(total >> 20), blocks,
              (unsigned long)(largest >> 10), (unsigned long)(largestLow >> 10),
              (unsigned long)(st.ullAvailPageFile >> 20), (unsigned long)(st.ullAvailPhys >> 20));
    out[outSize - 1] = 0;
}

BOOL __cdecl _VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
{
    return VirtualFree(lpAddress, dwSize, dwFreeType);
}

DWORD __cdecl _SleepEx(DWORD dwMilliseconds, BOOL bAlertable)
{
    return SleepEx(dwMilliseconds, bAlertable);
}

BOOL __cdecl _CloseHandle(HANDLE hObject)
{
    return CloseHandle(hObject);
}

DWORD __cdecl _GetLastError()
{
    return GetLastError();
}

void __cdecl _SetLastError(DWORD dwErrCode)
{
    return SetLastError(dwErrCode);
}

BOOL __cdecl _ReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    return ReadFileEx(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped, lpCompletionRoutine);
}

HANDLE __cdecl _CreateFileA(char *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

DWORD __cdecl _GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
    return GetFileSize(hFile, lpFileSizeHigh);
}

DWORD __cdecl _GetFileAttributesA(const char *lpFileName)
{
    return GetFileAttributesA(lpFileName);
}

BOOL __cdecl _SetFileAttributesA(const char *lpFileName, DWORD dwFileAttributes)
{
    return SetFileAttributesA(lpFileName, dwFileAttributes);
}

