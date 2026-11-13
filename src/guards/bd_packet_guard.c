/* Exact C1 1.1.1.13 bdPacket::deserialize top-level length guard.
 *
 * Its caller passes the received datagram length, but the original parser
 * subtracts the packet's two-byte chunk-area length without checking that it
 * fits. A malformed datagram can then make later reads use a logical limit
 * beyond the received buffer. This guard runs before the original method.
 * The type-2 DATA chunk needs its own inner payload bounds as well. Other
 * chunk deserializers require separate qualification. */
#ifndef BD_PACKET_DESERIALIZE_VA /* A test fixture supplies private entry pages. */
#define BD_PACKET_DESERIALIZE_VA 0x00B18510u
#define BD_DATA_DESERIALIZE_VA   0x00B16460u
#endif

typedef BYTE (__attribute__((thiscall)) *BdPacketDeserializeFn)(void *, const BYTE *, int);
typedef BYTE (__attribute__((thiscall)) *BdDataDeserializeFn)(void *, const BYTE *, int,
                                                               unsigned *, const BYTE *, int,
                                                               unsigned *);
static BdPacketDeserializeFn g_bdPacketDeserializeOriginal;
static BdDataDeserializeFn g_bdDataDeserializeOriginal;

static BYTE __attribute__((thiscall)) BdPacketDeserializeGuard(void *self, const BYTE *data, int size)
{
    unsigned chunk_area;
    if (!data || size <= 6 || !Readable(data, 2)) return 0;
    chunk_area = (unsigned)data[0] | ((unsigned)data[1] << 8);
    if (chunk_area > (unsigned)(size - 2)) return 0;
    return g_bdPacketDeserializeOriginal(self, data, size);
}

static BOOL BdReadVar16(const BYTE *base, unsigned limit, unsigned *cursor, unsigned *value)
{
    unsigned b;
    if (*cursor >= limit || !Readable(base + *cursor, 1)) return FALSE;
    b = base[(*cursor)++];
    if (b < 0x80) { *value = b; return TRUE; }
    if (*cursor >= limit || !Readable(base + *cursor, 1)) return FALSE;
    *value = ((b & 0x7f) << 8) | base[(*cursor)++];
    return TRUE;
}

static BYTE __attribute__((thiscall)) BdDataDeserializeGuard(void *self,
    const BYTE *area, int area_size, unsigned *area_cursor,
    const BYTE *tail, int tail_size, unsigned *tail_cursor)
{
    unsigned pos, tc, area_length = 0, tail_length = 0, flags;
    if (!area || !area_cursor || !tail_cursor || area_size < 0 || tail_size < 0 ||
        !Readable(area_cursor, 4) || !Readable(tail_cursor, 4)) return 0;
    pos = *area_cursor;
    tc = *tail_cursor;
    if (pos > (unsigned)area_size || tc > (unsigned)tail_size ||
        (unsigned)area_size - pos < 5 || !Readable(area + pos, 5)) return 0;
    if (area[pos] != 2) return 0; /* this method is the type-2 DATA handler */
    flags = area[pos + 1];
    pos += 5; /* type, flags, subtype, two-byte channel field */
    if ((flags & 2) && !BdReadVar16(area, (unsigned)area_size, &pos, &area_length)) return 0;
    if ((flags & 4) && !BdReadVar16(area, (unsigned)area_size, &pos, &tail_length)) return 0;
    if (area_length > (unsigned)area_size - pos ||
        tail_length > (unsigned)tail_size - tc || (tail_length && !tail)) return 0;
    return g_bdDataDeserializeOriginal(self, area, area_size, area_cursor,
                                       tail, tail_size, tail_cursor);
}

static BOOL BdInstallEntry(DWORD va, const BYTE expected[10], const void *hook,
                           void *original_slot, SIZE_T original_size)
{
    BYTE *const entry = (BYTE *)(DWORD_PTR)va;
    BYTE *trampoline;
    BYTE jump[5];
    DWORD rel, old;

    if (original_size != sizeof(trampoline) ||
        !Readable(entry, 10) || memcmp(entry, expected, 10)) {
        Log("bd packet guard: exact C1 parser signature differs");
        return FALSE;
    }
    trampoline = (BYTE *)VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!trampoline) return FALSE;
    memcpy(trampoline, expected, 5); /* complete prologue instructions */
    trampoline[5] = 0xE9;
    rel = (DWORD)(DWORD_PTR)(entry + 5) - (DWORD)(DWORD_PTR)(trampoline + 10);
    memcpy(trampoline + 6, &rel, 4);
    if (!VirtualProtect(trampoline, 10, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, 10);
    /* Function and data pointers are both 32 bits in this C1 build. Copy the
     * representation without aliasing a function-pointer object as void**. */
    memcpy(original_slot, &trampoline, sizeof(trampoline));
    jump[0] = 0xE9;
    rel = (DWORD)(DWORD_PTR)hook - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("bd packet guard: parser jump did not install");
        return FALSE; /* caller exits before game entry */
    }
    return TRUE;
}

static BOOL InstallBdPacketLengthGuard(void)
{
    static const BYTE packet_entry[10] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF,
                                           0x68, 0xC9, 0x10, 0xB3, 0x00 };
    static const BYTE data_entry[10] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF,
                                         0x68, 0xAB, 0x0D, 0xB3, 0x00 };
    /* Verify both call sites before changing either one. A mismatched client
     * must leave the original parser untouched and stop before game entry. */
    if (!Readable((const void *)(DWORD_PTR)BD_PACKET_DESERIALIZE_VA, 10) ||
        memcmp((const void *)(DWORD_PTR)BD_PACKET_DESERIALIZE_VA, packet_entry, 10) ||
        !Readable((const void *)(DWORD_PTR)BD_DATA_DESERIALIZE_VA, 10) ||
        memcmp((const void *)(DWORD_PTR)BD_DATA_DESERIALIZE_VA, data_entry, 10))
        return FALSE;
    if (!BdInstallEntry(BD_PACKET_DESERIALIZE_VA, packet_entry,
                        (const void *)BdPacketDeserializeGuard,
                        &g_bdPacketDeserializeOriginal,
                        sizeof(g_bdPacketDeserializeOriginal))) return FALSE;
    return BdInstallEntry(BD_DATA_DESERIALIZE_VA, data_entry,
                          (const void *)BdDataDeserializeGuard,
                          &g_bdDataDeserializeOriginal,
                          sizeof(g_bdDataDeserializeOriginal));
}
