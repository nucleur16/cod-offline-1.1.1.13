/*
 * sys_udpowner.c - which local process owns a UDP port (sh1x diagnostics).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The live 17733 server was shut down three times on 2026-10-27 and the log
 * could only say "Rcon from 127.0.0.1:<port>" for the rcon traffic around it:
 * every sender on this PC (the lobby, Kernel33's P2P host and dev console,
 * agent scripts) shares the loopback address and one rcon password.  A UDP
 * datagram carries no process, but the sender's socket is usually still bound
 * when the server reads the packet, so the IPv4 UDP owner table maps the
 * source port back to a PID.  Sys_UdpPortOwner formats
 *     pid 1234 python.exe (parent 5678 cmd.exe)
 * or says why it could not.  A sender that closes its socket right after
 * sendto() (fire-and-forget rcon) can be gone before the lookup; the line then
 * says so instead of guessing.
 *
 * Everything is resolved at run time (GetExtendedUdpTable and
 * QueryFullProcessImageNameA are Vista+, the build targets WINVER 0x501).
 */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	DWORD dwLocalAddr;
	DWORD dwLocalPort;
	DWORD dwOwningPid;
} sh1x_udprow_owner_pid_t;

typedef struct {
	DWORD dwNumEntries;
	sh1x_udprow_owner_pid_t table[1];
} sh1x_udptable_owner_pid_t;

typedef DWORD (WINAPI *GetExtendedUdpTable_t)(PVOID, PDWORD, BOOL, ULONG, int, ULONG);
typedef BOOL (WINAPI *QueryFullProcessImageNameA_t)(HANDLE, DWORD, LPSTR, PDWORD);

#define SH1X_UDP_TABLE_OWNER_PID 1

static void UdpOwner_ImageName(DWORD pid, char *out, int outSize, DWORD *parentPid)
{
	static QueryFullProcessImageNameA_t qfpin;
	static int qfpinLooked;
	HANDLE snap;
	PROCESSENTRY32 pe;

	out[0] = 0;
	*parentPid = 0;

	/* Toolhelp gives the exe name and the parent in one pass. */
	snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap != INVALID_HANDLE_VALUE) {
		pe.dwSize = sizeof(pe);
		if (Process32First(snap, &pe)) {
			do {
				if (pe.th32ProcessID == pid) {
					_snprintf(out, outSize, "%s", pe.szExeFile);
					out[outSize - 1] = 0;
					*parentPid = pe.th32ParentProcessID;
					break;
				}
			} while (Process32Next(snap, &pe));
		}
		CloseHandle(snap);
	}

	/* The full path tells two copies of the same exe apart (a probe install vs the live one). */
	if (!qfpinLooked) {
		HMODULE k32 = GetModuleHandleA("kernel32.dll");
		qfpin = k32 ? (QueryFullProcessImageNameA_t)GetProcAddress(k32, "QueryFullProcessImageNameA") : NULL;
		qfpinLooked = 1;
	}
	if (qfpin) {
		HANDLE h = OpenProcess(0x1000 /* PROCESS_QUERY_LIMITED_INFORMATION */, FALSE, pid);
		if (h) {
			char path[MAX_PATH];
			DWORD len = sizeof(path);
			if (qfpin(h, 0, path, &len) && len > 0)
				_snprintf(out, outSize, "%s", path);
			out[outSize - 1] = 0;
			CloseHandle(h);
		}
	}
	if (!out[0])
		_snprintf(out, outSize, "%s", "?");
}

static void UdpOwner_ParentName(DWORD pid, char *out, int outSize)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe;

	_snprintf(out, outSize, "%s", "exited");
	if (snap == INVALID_HANDLE_VALUE)
		return;
	pe.dwSize = sizeof(pe);
	if (Process32First(snap, &pe)) {
		do {
			if (pe.th32ProcessID == pid) {
				_snprintf(out, outSize, "%s", pe.szExeFile);
				break;
			}
		} while (Process32Next(snap, &pe));
	}
	out[outSize - 1] = 0;
	CloseHandle(snap);
}

/* portNetOrder: the sender's UDP source port as netadr_t keeps it (network byte order).
 * Returns 1 when an owner was found. */
int Sys_UdpPortOwner(unsigned short portNetOrder, char *out, int outSize)
{
	static GetExtendedUdpTable_t getTable;
	static int looked;
	sh1x_udptable_owner_pid_t *table = NULL;
	DWORD size = 0, ret, i, pid = 0;
	int found = 0;

	if (outSize <= 0)
		return 0;
	out[0] = 0;

	if (!looked) {
		HMODULE ip = LoadLibraryA("iphlpapi.dll");
		getTable = ip ? (GetExtendedUdpTable_t)GetProcAddress(ip, "GetExtendedUdpTable") : NULL;
		looked = 1;
	}
	if (!getTable) {
		_snprintf(out, outSize, "%s", "owner unknown (GetExtendedUdpTable unavailable)");
		out[outSize - 1] = 0;
		return 0;
	}

	/* The table grows between the size query and the read when sockets open; retry a few times. */
	for (i = 0; i < 4; i++) {
		ret = getTable(table, &size, FALSE, AF_INET, SH1X_UDP_TABLE_OWNER_PID, 0);
		if (ret == NO_ERROR)
			break;
		if (ret != ERROR_INSUFFICIENT_BUFFER) {
			free(table);
			_snprintf(out, outSize, "owner unknown (GetExtendedUdpTable error %lu)", (unsigned long)ret);
			out[outSize - 1] = 0;
			return 0;
		}
		free(table);
		size += 16 * sizeof(sh1x_udprow_owner_pid_t);
		table = (sh1x_udptable_owner_pid_t *)malloc(size);
		if (!table) {
			_snprintf(out, outSize, "%s", "owner unknown (out of memory)");
			out[outSize - 1] = 0;
			return 0;
		}
	}
	if (ret == NO_ERROR && table) {
		for (i = 0; i < table->dwNumEntries; i++) {
			if ((unsigned short)table->table[i].dwLocalPort == portNetOrder) {
				pid = table->table[i].dwOwningPid;
				found = 1;
				break;
			}
		}
	}
	free(table);

	if (!found) {
		_snprintf(out, outSize, "owner unknown (source port %u no longer bound: the sender closed its socket)",
			(unsigned)(((portNetOrder & 0xff) << 8) | (portNetOrder >> 8)));
		out[outSize - 1] = 0;
		return 0;
	}

	{
		char image[MAX_PATH], parent[MAX_PATH];
		DWORD ppid;
		UdpOwner_ImageName(pid, image, sizeof(image), &ppid);
		if (ppid)
			UdpOwner_ParentName(ppid, parent, sizeof(parent));
		else
			_snprintf(parent, sizeof(parent), "%s", "?");
		parent[sizeof(parent) - 1] = 0;
		_snprintf(out, outSize, "pid %lu %s (parent %lu %s)", (unsigned long)pid, image,
			(unsigned long)ppid, parent);
		out[outSize - 1] = 0;
	}
	return 1;
}
