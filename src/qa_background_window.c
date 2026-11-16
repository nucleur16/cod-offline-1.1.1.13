/* Explicit QA-only background window policy. No game/room/input state writes.
 * The existing exact-build import loader and guarded borderless creation path
 * call this layer. The external receipt-bound launcher moves the off-screen,
 * non-activating window before setting its per-launch placement property. */
#if K33_LOCAL_TESTING
__declspec(dllexport) volatile LONG QbgTelemetry[16];
static BOOL g_qbgChecked, g_qbgEnabled;
static char g_qbgProperty[64];
static HWND g_qbgWindow;
static DWORD g_qbgImports;
static BOOL (WINAPI *g_qbgShowWindow)(HWND, int);
static BOOL (WINAPI *g_qbgSetWindowPos)(HWND, HWND, int, int, int, int, UINT);

static BOOL QbgEnabled(void)
{
    char value[8];
    if (g_qbgChecked) return g_qbgEnabled;
    g_qbgChecked = TRUE;
    if (GetEnvironmentVariableA("CODOL_QA_BACKGROUND", value, sizeof(value)) != 1 || value[0] != '1') return FALSE;
    DWORD n = GetEnvironmentVariableA("CODOL_QA_BACKGROUND_PROPERTY", g_qbgProperty, sizeof(g_qbgProperty));
    if (n != 44 || memcmp(g_qbgProperty, "CODOL_QA_BG_", 12)) {
        QbgTelemetry[1] = 1;
        g_qbgEnabled = TRUE; /* Invalid opt-in must fail startup, never activate. */
        return TRUE;
    }
    for (DWORD i = 12; i < n; ++i) {
        char c = g_qbgProperty[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) QbgTelemetry[1] = 1;
    }
    g_qbgEnabled = TRUE;
    QbgTelemetry[0] = 1;
    return TRUE;
}

static BOOL QbgOwnWindow(HWND window)
{
    DWORD owner = 0;
    return window && GetWindowThreadProcessId(window, &owner) && owner == GetCurrentProcessId();
}

static BOOL WINAPI QbgSetForegroundWindow(HWND window)
{
    (void)window;
    InterlockedIncrement(&QbgTelemetry[4]);
    return FALSE;
}

static HWND WINAPI QbgSetFocus(HWND window)
{
    (void)window;
    InterlockedIncrement(&QbgTelemetry[5]);
    return GetFocus();
}

static BOOL WINAPI QbgShowWindow(HWND window, int command)
{
    if (!QbgOwnWindow(window)) return FALSE;
    InterlockedIncrement(&QbgTelemetry[6]);
    return g_qbgShowWindow(window, command == SW_HIDE ? SW_HIDE : SW_SHOWNOACTIVATE);
}

static BOOL WINAPI QbgSetWindowPos(HWND window, HWND after, int x, int y, int w, int h, UINT flags)
{
    if (!QbgOwnWindow(window)) return FALSE;
    if (window == g_qbgWindow && GetPropA(window, g_qbgProperty) != (HANDLE)(ULONG_PTR)1) {
        x = y = -32000;
        flags &= ~SWP_NOMOVE;
        InterlockedIncrement(&QbgTelemetry[7]);
    }
    return g_qbgSetWindowPos(window, after, x, y, w, h, flags | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

static FARPROC QbgImport(const C1Import *import, FARPROC original)
{
    if (!QbgEnabled() || _stricmp(import->dll, "USER32.dll")) return original;
    if (import->slot == 0x00B423A0u && !strcmp(import->name, "ShowWindow")) {
        g_qbgShowWindow = (BOOL(WINAPI *)(HWND, int))(void *)original;
        g_qbgImports |= 1;
        return (FARPROC)(void *)QbgShowWindow;
    }
    if (import->slot == 0x00B423FCu && !strcmp(import->name, "SetFocus")) {
        g_qbgImports |= 2;
        return (FARPROC)(void *)QbgSetFocus;
    }
    if (import->slot == 0x00B42400u && !strcmp(import->name, "SetForegroundWindow")) {
        g_qbgImports |= 4;
        return (FARPROC)(void *)QbgSetForegroundWindow;
    }
    if (import->slot == 0x00B42490u && !strcmp(import->name, "SetWindowPos")) {
        g_qbgSetWindowPos = (BOOL(WINAPI *)(HWND, HWND, int, int, int, int, UINT))(void *)original;
        g_qbgImports |= 8;
        return (FARPROC)(void *)QbgSetWindowPos;
    }
    return original;
}

static void QbgPrepareWindow(DWORD *extended, int *x, int *y)
{
    if (!QbgEnabled()) return;
    /* APPWINDOW keeps a non-activating main window discoverable by the
     * virtual-desktop shell before the launcher releases its placement. */
    *extended = (*extended | WS_EX_NOACTIVATE | WS_EX_APPWINDOW) & ~WS_EX_TOPMOST;
    *x = *y = -32000;
    g_qbgWindow = NULL;
    InterlockedIncrement(&QbgTelemetry[2]);
}

static void QbgCreatedWindow(HWND window)
{
    if (!QbgEnabled()) return;
    g_qbgWindow = window;
    QbgTelemetry[3] = (LONG)(LONG_PTR)window;
}

static BOOL QbgStartupReady(BOOL guarded_creation_installed)
{
    if (!QbgEnabled()) return TRUE;
    QbgTelemetry[8] = (LONG)g_qbgImports;
    return !QbgTelemetry[1] && g_qbgImports == 15 && guarded_creation_installed;
}
#else
#define QbgImport(import, original) (original)
#define QbgPrepareWindow(extended, x, y) ((void)0)
#define QbgCreatedWindow(window) ((void)0)
#define QbgStartupReady(guarded_creation_installed) TRUE
#endif
