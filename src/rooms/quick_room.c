/*
 * quick_room.c - ui_quickNativeRoomContext, ported from codol-classic's d3d9.dll (NativeRoomContext.cpp).
 * Included by kernel33.c after k33tick.c.
 *
 * The shared front-end movie (voice_ui_mp: MatchController.as, LobbyController.as) reads the string dvar
 * ui_quickNativeRoomContext to recognise "the same native room" across a match: Quick Play's return to the
 * room after a match, the summary screen and the host-ready acknowledgement all compare its key. Its format is
 *     v1|<serial>|<room id as 16 lowercase hex digits, high dword first>
 * and "" when the client is in no room. The movie never writes it; codol-classic's d3d9.dll published it and
 * without it every one of those checks fails (the movie then treats each room as new).
 *
 * The room: C1's main room object at 0x82D5470 (an embedded object, not a pointer), vtable +0x44 IsInRoom
 * (0x70E9F0, thiscall), +0x40 membership (0x70E9B0); its record pointer at +0x10: active byte +1, room id
 * +0x10 (low) / +0x14 (high). Sampled every 250 ms on the main thread (k33tick.c), only when both vtable entries
 * still point at those functions and both functions carry their 1.1.1.13 bytes.
 *
 * The serial counts room changes (1 for the first room of the session). A changed value is set through the command
 * buffer (C1QueueSet), so the movie sees it from the next frame on; when the dvar does not show the value 2 s later
 * (a set lost in a level change), it is set again with the same serial. Read only otherwise: no room state changes.
 */

#define QR_DVAR            "ui_quickNativeRoomContext"
#define QR_MAIN_ROOM_VA    0x082D5470u
#define QR_IS_IN_ROOM_VA   0x0070E9F0u
#define QR_MEMBERSHIP_VA   0x0070E9B0u
#define QR_SAMPLE_MS       250
#define QR_RETRY_MS        2000

static BOOL g_qrOk;
static unsigned g_qrSerial;
static DWORD g_qrLow, g_qrHigh;          /* the room the current value names (0/0: none) */
static char g_qrValue[48];               /* what the dvar should say */
static DWORD g_qrQueuedAt;

/* The current room id; FALSE when not in a room or the object does not look like 1.1.1.13's. */
static BOOL QuickRoomSample(DWORD *low, DWORD *high)
{
    const DWORD *obj = (const DWORD *)(DWORD_PTR)QR_MAIN_ROOM_VA;
    if (!Readable(obj, 0x14)) return FALSE;
    const DWORD *vt = (const DWORD *)(DWORD_PTR)obj[0];
    if (!vt || !Readable(vt, 0x48) || vt[0x44 / 4] != QR_IS_IN_ROOM_VA || vt[0x40 / 4] != QR_MEMBERSHIP_VA) return FALSE;
    const BYTE *rec = (const BYTE *)(DWORD_PTR)obj[0x10 / 4];
    if (!rec || !Readable(rec, 0x18) || !rec[1]) return FALSE;
    if (!((BYTE(__attribute__((thiscall)) *)(const void *))(DWORD_PTR)QR_IS_IN_ROOM_VA)(obj)) return FALSE;
    /* the record may be replaced while IsInRoom ran: read it again */
    rec = (const BYTE *)(DWORD_PTR)obj[0x10 / 4];
    if (!rec || !Readable(rec, 0x18) || !rec[1]) return FALSE;
    *low = *(const DWORD *)(rec + 0x10);
    *high = *(const DWORD *)(rec + 0x14);
    return *low || *high;
}

static void QuickRoomTick(void)
{
    static DWORD next;
    DWORD now = GetTickCount();
    if ((LONG)(now - next) < 0) return;
    next = now + QR_SAMPLE_MS;
    if (!C1DvarsReady()) return;

    DWORD low = 0, high = 0;
    if (!QuickRoomSample(&low, &high)) low = high = 0;
    if (low != g_qrLow || high != g_qrHigh) {
        g_qrLow = low;
        g_qrHigh = high;
        if (low || high) {
            if (g_qrSerial == 0xFFFFFFFFu) return;
            _snprintf(g_qrValue, sizeof(g_qrValue), "v1|%u|%08lx%08lx", ++g_qrSerial, (unsigned long)high,
                      (unsigned long)low);
        } else {
            g_qrValue[0] = 0;
        }
        g_qrQueuedAt = 0;
        Log("quick room: %s", g_qrValue[0] ? g_qrValue : "(no room)");
    }
    char cur[64];
    C1DvarString(QR_DVAR, cur, sizeof(cur));
    if (!strcmp(cur, g_qrValue)) return;
    if (g_qrQueuedAt && now - g_qrQueuedAt < QR_RETRY_MS) return;
    if (C1QueueSet(QR_DVAR, g_qrValue)) g_qrQueuedAt = now ? now : 1;
}

static void InstallQuickRoom(void)
{
    static const BYTE kIsInRoom[] = { 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x8B, 0x50, 0x40, 0xFF, 0xD2, 0x84, 0xC0, 0x74, 0x3C };
    static const BYTE kMembership[] = { 0x8B, 0x41, 0x10, 0x85, 0xC0, 0x74, 0x06, 0x80, 0x78, 0x01, 0x00, 0x75, 0x0E };
    C1GameInit();
    g_qrOk = Readable((void *)(DWORD_PTR)QR_IS_IN_ROOM_VA, sizeof(kIsInRoom)) &&
             !memcmp((void *)(DWORD_PTR)QR_IS_IN_ROOM_VA, kIsInRoom, sizeof(kIsInRoom)) &&
             Readable((void *)(DWORD_PTR)QR_MEMBERSHIP_VA, sizeof(kMembership)) &&
             !memcmp((void *)(DWORD_PTR)QR_MEMBERSHIP_VA, kMembership, sizeof(kMembership)) && g_c1CbufOk;
    if (!g_qrOk) {
        Log("quick room: room functions or Cbuf_AddText differ; %s not published", QR_DVAR);
        return;
    }
    K33OnMainTick(QuickRoomTick);
    Log("quick room: publishing %s", QR_DVAR);
}
