// vision_ui.cpp - the vision editor window (Dear ImGui 1.91.9b, imgui/), drawn by vision.c's VisDraw from
// console.c's Present hook on the render thread. All C1 state goes through vision_api.h; this file only
// draws, turns queued window messages into ImGui input, and owns ImGui's DX9 objects.
// Built by build-dev.sh with g++ (-static-libstdc++ via the static link), dev build only (-DK33_VISION).

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "vision_api.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// imgui_impl_win32.cpp (compiled in; its WndProc handler is not used: messages arrive on the main
// thread and ImGui runs on the render thread, so vision.c queues them).
ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam);

// ImGui_ImplWin32_EnableAlphaCompositing (unused) calls three DWM functions; build-dev.sh renames them to
// these stubs so the DLL does not import dwmapi.dll.
extern "C" HRESULT WINAPI K33_DwmIsCompositionEnabled(BOOL *enabled) { if (enabled) *enabled = FALSE; return E_NOTIMPL; }
extern "C" HRESULT WINAPI K33_DwmGetColorizationColor(DWORD *color, BOOL *opaque) { (void)color; (void)opaque; return E_NOTIMPL; }
extern "C" HRESULT WINAPI K33_DwmEnableBlurBehindWindow(HWND hwnd, const void *bb) { (void)hwnd; (void)bb; return E_NOTIMPL; }

static bool g_ctx;                 // ImGui context created
static IDirect3DDevice9 *g_dev;    // the device the DX9 backend was initialised with
static float g_scale = 1.0f;
static LARGE_INTEGER g_freq, g_last;

static char g_name[VIS_NAME_LEN], g_saveName[VIS_NAME_LEN], g_zoneFilter[VIS_NAME_LEN];
static int g_duration;
static char g_zone[VIS_MAX_LIST][VIS_NAME_LEN], g_disk[VIS_MAX_LIST][VIS_NAME_LEN];
static int g_zoneCount = -2, g_diskCount;
static char g_lastSet[VIS_NAME_LEN];
static char g_text[0x4000];

static void CreateContext(int height)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = VisApi_IniPath();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.BackendFlags &= ~ImGuiBackendFlags_HasSetMousePos;
    g_scale = height >= 1400 ? 1.6f : height >= 1000 ? 1.25f : 1.0f;
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    const char *faces[] = { "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\tahoma.ttf", "C:\\Windows\\Fonts\\arial.ttf" };
    bool loaded = false;
    for (const char *f : faces) {
        if (GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES) continue;
        if (io.Fonts->AddFontFromFileTTF(f, 15.0f * g_scale, &cfg)) { loaded = true; break; }
    }
    if (!loaded) {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = g_scale;
    }
    ImGui::StyleColorsDark();
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowRounding = 4.0f;
    st.FrameRounding = 3.0f;
    st.GrabRounding = 3.0f;
    st.Colors[ImGuiCol_WindowBg].w = 0.92f;
    st.ScaleAllSizes(g_scale);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_last);
    g_ctx = true;
}

static void FeedInput(int width, int height)
{
    ImGuiIO &io = ImGui::GetIO();
    VisInputEvent ev;
    while (VisApi_PopInput(&ev)) {
        switch (ev.type) {
        case VIS_EV_KEY: {
            io.AddKeyEvent(ImGuiMod_Ctrl, (ev.mods & 1) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (ev.mods & 2) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (ev.mods & 4) != 0);
            ImGuiKey key = ImGui_ImplWin32_KeyEventToImGuiKey((WPARAM)ev.a, (LPARAM)ev.lp);
            if (key != ImGuiKey_None) io.AddKeyEvent(key, ev.b != 0);
            break;
        }
        case VIS_EV_CHAR: io.AddInputCharacterUTF16((ImWchar16)ev.a); break;
        case VIS_EV_MOUSE: io.AddMouseButtonEvent(ev.a, ev.b != 0); break;
        case VIS_EV_WHEEL: io.AddMouseWheelEvent(0.0f, ev.a / 120.0f); break;
        case VIS_EV_HWHEEL: io.AddMouseWheelEvent(-ev.a / 120.0f, 0.0f); break;
        case VIS_EV_FOCUS: io.AddFocusEvent(ev.a != 0); break;
        case VIS_EV_MOUSEPOS: { // client pixels -> back-buffer pixels (a window's client area can differ)
            int cw = (int)(ev.lp & 0xFFFF), ch = (int)(ev.lp >> 16);
            if (cw > 0 && ch > 0) io.AddMousePosEvent((float)ev.a * width / cw, (float)ev.b * height / ch);
            break;
        }
        }
    }
    CURSORINFO ci = { sizeof(ci) };
    io.MouseDrawCursor = !(GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING));
}

static void LiveRow(const char *k, const char *fmt, ...) IM_FMTARGS(2);
static void LiveRow(const char *k, const char *fmt, ...)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(k);
    ImGui::TableNextColumn();
    va_list ap;
    va_start(ap, fmt);
    ImGui::TextV(fmt, ap);
    va_end(ap);
}

static void HelpMarker(const char *text)
{
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void FieldRow(int i, const BYTE *edit, const BYTE *loaded, bool canApply)
{
    const VisFieldInfo *f = VisApi_Field(i);
    ImGui::PushID(i);
    bool changed = false;
    BYTE value[12];
    int size = f->type == VIS_T_VEC3 ? 12 : f->type == VIS_T_FLOAT ? 4 : 1;
    memcpy(value, edit + f->offset, size);
    char label[96];
    snprintf(label, sizeof(label), "%s", f->label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.62f);
    if (f->type == VIS_T_BOOL) {
        bool b = value[0] != 0;
        if (ImGui::Checkbox(label, &b)) { value[0] = b ? 1 : 0; changed = true; }
    } else if (f->type == VIS_T_FLOAT) {
        float *v = (float *)value;
        changed = ImGui::SliderFloat(label, v, f->lo, f->hi, "%.4g", ImGuiSliderFlags_None);
    } else {
        float *v = (float *)value;
        changed = ImGui::ColorEdit3(label, v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_DisplayRGB |
                                                  ImGuiColorEditFlags_PickerHueWheel);
    }
    if (ImGui::BeginPopupContextItem("ctx")) {
        if (ImGui::MenuItem("Reset to the loaded value")) { memcpy(value, loaded + f->offset, size); changed = true; }
        ImGui::EndPopup();
    }
    if (changed) VisApi_SetField(i, value);
    ImGui::SameLine();
    bool modified = memcmp(edit + f->offset, loaded + f->offset, size) != 0;
    if (modified) ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s*", f->id);
    else ImGui::TextDisabled("%s", f->id);
    char tip[512];
    snprintf(tip, sizeof(tip), "key %s (IW4: %s)\n%s%s%s", f->id, f->iwName, f->help,
             f->live ? "" : "\nC1 parses and lerps it but never copies it into the refdef: no visible effect.",
             canApply ? "" : "\nNot applied: not in a match.");
    HelpMarker(tip);
    if (!f->live) {
        ImGui::SameLine();
        ImGui::TextDisabled("(not drawn)");
    }
    ImGui::PopID();
}

static void RefreshLists(void)
{
    g_zoneCount = VisApi_ListZone(g_zone, VIS_MAX_LIST);
    g_diskCount = VisApi_ListDisk(g_disk, VIS_MAX_LIST);
}

static bool NameList(const char *id, char (*names)[VIS_NAME_LEN], int count, const char *filter, float height)
{
    bool activated = false;
    if (ImGui::BeginListBox(id, ImVec2(-FLT_MIN, height))) {
        for (int i = 0; i < count; i++) {
            if (filter && filter[0] && !strstr(names[i], filter)) continue;
            bool sel = !strcmp(g_name, names[i]);
            if (ImGui::Selectable(names[i], sel, ImGuiSelectableFlags_AllowDoubleClick)) {
                snprintf(g_name, sizeof(g_name), "%s", names[i]);
                if (ImGui::IsMouseDoubleClicked(0)) activated = true;
            }
        }
        ImGui::EndListBox();
    }
    return activated;
}

static void DrawEditor(void)
{
    VisUiState st;
    VisApi_GetState(&st);
    BYTE edit[VIS_SET_BYTES], loaded[VIS_SET_BYTES];
    VisApi_GetValues(edit, loaded);
    if (strcmp(g_lastSet, st.setName)) { // a new set: offer its name for saving
        snprintf(g_lastSet, sizeof(g_lastSet), "%s", st.setName);
        snprintf(g_saveName, sizeof(g_saveName), "%s", st.setName);
        if (!g_name[0]) snprintf(g_name, sizeof(g_name), "%s", st.setName);
    }
    if (g_zoneCount == -2) RefreshLists();

    ImGuiViewport *vp = ImGui::GetMainViewport();
    static ImVec2 lastDisplay, winPos, winSize;
    bool resized = lastDisplay.x != vp->Size.x || lastDisplay.y != vp->Size.y;
    lastDisplay = vp->Size;
    float w = 600 * g_scale, h = vp->Size.y - 40 > 900 * g_scale ? 900 * g_scale : vp->Size.y - 40;
    ImGui::SetNextWindowPos(ImVec2(vp->Size.x - w - 20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);
    if (resized && winSize.x > 0 && (winPos.x + winSize.x > vp->Size.x || winPos.y + winSize.y > vp->Size.y)) {
        // a smaller back buffer (resolution change): keep the window on screen
        ImVec2 size(winSize.x < vp->Size.x - 20 ? winSize.x : vp->Size.x - 20, winSize.y < vp->Size.y - 20 ? winSize.y : vp->Size.y - 20);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(vp->Size.x - size.x - 10 > 0 ? vp->Size.x - size.x - 10 : 0, 10), ImGuiCond_Always);
    }
    bool open = true;
    if (!ImGui::Begin("Vision editor (Kernel33)###k33vision", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) VisApi_Close();
        return;
    }
    if (!open) VisApi_Close();
    winPos = ImGui::GetWindowPos();
    winSize = ImGui::GetWindowSize();
    if (!st.ok) {
        ImGui::TextWrapped("C1's vision code did not match this build: the editor is off (see Kernel33.log).");
        ImGui::End();
        return;
    }

    ImGui::Text("Set: %s%s", st.setName[0] ? st.setName : "(none)", st.dirty ? "  (edited)" : "");
    ImGui::TextDisabled("%s", st.source);
    if (!st.inMatch) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Not in a match: C1 keeps no vision state at the menu. Disk files load, edit and save here; "
                                                     "nothing is written into the game until a match.");
        ImGui::PopTextWrapPos();
    }

    // which vision type edits go to
    char preview[128];
    if (st.autoMode) snprintf(preview, sizeof(preview), "auto: %s", VisApi_ModeLabel(st.editMode));
    else snprintf(preview, sizeof(preview), "%s", VisApi_ModeLabel(st.editMode));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.62f);
    if (ImGui::BeginCombo("Vision type", preview)) {
        if (ImGui::Selectable("auto (the type C1 draws with)", st.autoMode)) VisApi_SetEditMode(-1);
        for (int m = 0; m < VIS_MODES; m++) {
            char item[160];
            snprintf(item, sizeof(item), "%s%s  %s%s", VisApi_ModeLabel(m), m == st.activeMode ? " [drawn]" : "",
                     st.modeName[m][0] ? st.modeName[m] : (st.inMatch && st.modeStyle[m] ? "(default)" : "-"),
                     st.inMatch && !st.modeStyle[m] ? "  (never set)" : "");
            if (ImGui::Selectable(item, !st.autoMode && st.editMode == m)) VisApi_SetEditMode(m);
        }
        ImGui::EndCombo();
    }
    HelpMarker("C1 keeps one vision set per type (naked, night, missilecam, thermal, pain and an unnamed sixth) and draws "
               "with one per frame. Auto edits whichever it draws with now.");
    if (st.inMatch) {
        ImGui::SameLine();
        ImGui::TextDisabled("drawn: %s", st.activeMode >= 0 ? VisApi_ModeLabel(st.activeMode) : "-");
    }
    if (st.filmTweaks || st.glowTweaks)
        ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "r_filmUseTweaks %d / r_glowUseTweaks %d: the renderer uses the r_*Tweak dvars, not this set",
                           st.filmTweaks, st.glowTweaks);
    if (st.gameChanged) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "C1 applied another set to this type: your edits were replaced in game.");
        if (ImGui::Button("Re-apply my edits")) VisApi_Reapply();
        ImGui::SameLine();
        if (ImGui::Button("Take C1's set")) VisApi_Reread();
    }

    if (ImGui::CollapsingHeader("Load / save", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        bool enter = ImGui::InputTextWithHint("##name", "vision name (no extension)", g_name, sizeof(g_name), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ImGui::BeginDisabled(!st.inMatch || st.pending);
        if (ImGui::Button("Load from zone") || (enter && st.inMatch)) VisApi_LoadZone(g_name, g_duration, 1);
        ImGui::EndDisabled();
        HelpMarker("Runs C1's own CG_SetVisionSetTransition on the edited type, as a VisionSet script call would, "
                   "from the rawfile vision/<name>.vision in the loaded zones.");
        ImGui::SameLine();
        if (ImGui::Button("Load from disk")) VisApi_LoadDisk(g_name);
        HelpMarker(VisApi_SaveDir());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::SliderInt("Transition (ms)", &g_duration, 0, 5000);
        if (ImGui::TreeNode("Vision files")) {
            if (ImGui::Button("Refresh")) RefreshLists();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##filter", "filter", g_zoneFilter, sizeof(g_zoneFilter));
            ImGui::Text("In the loaded zones (%d)", g_zoneCount < 0 ? 0 : g_zoneCount);
            if (NameList("##zone", g_zone, g_zoneCount < 0 ? 0 : g_zoneCount, g_zoneFilter, 7 * ImGui::GetTextLineHeightWithSpacing()) && st.inMatch)
                VisApi_LoadZone(g_name, g_duration, 1);
            ImGui::Text("On disk, %s (%d)", VisApi_SaveDir(), g_diskCount);
            if (NameList("##disk", g_disk, g_diskCount, g_zoneFilter, 4 * ImGui::GetTextLineHeightWithSpacing())) VisApi_LoadDisk(g_name);
            ImGui::TreePop();
        }
        if (ImGui::Button("Revert")) VisApi_Revert();
        HelpMarker("Back to the values the set was loaded with (and into the game).");
        ImGui::SameLine();
        ImGui::BeginDisabled(!st.inMatch);
        if (ImGui::Button("Re-read from C1")) VisApi_Reread();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::InputTextWithHint("##save", "file name", g_saveName, sizeof(g_saveName));
        ImGui::SameLine();
        if (ImGui::Button("Save .vision")) {
            char path[MAX_PATH];
            if (VisApi_Save(g_saveName, path, sizeof(path))) g_diskCount = VisApi_ListDisk(g_disk, VIS_MAX_LIST);
        }
        HelpMarker("Writes C1's .vision format (numeric keys) to main\\vision\\<name>.vision; copy it to "
                   "zones/<zone>/rawfile/vision/ to ship it in a zone.");
        bool keep = st.keepInCache != 0;
        if (ImGui::Checkbox("Keep edits when C1 re-applies this set (session cache)", &keep)) VisApi_SetKeepInCache(keep);
        HelpMarker("Also writes edits into C1's 4-entry vision cache under this set's name, so a later VisionSet call with "
                   "the same name keeps them until the cache entry is replaced. Nothing is written to the zones.");
    }

    for (int g = 0; g < VIS_GROUPS; g++) {
        if (!ImGui::CollapsingHeader(VisApi_GroupLabel(g), ImGuiTreeNodeFlags_DefaultOpen)) continue;
        for (int i = 0; i < VIS_FIELDS; i++)
            if (VisApi_Field(i)->group == g) FieldRow(i, edit, loaded, st.inMatch != 0);
    }

    if (ImGui::CollapsingHeader("What C1 renders this frame")) {
        VisLiveRefdef r;
        VisApi_GetLive(&r);
        if (!r.valid) ImGui::TextDisabled("no refdef (not in a match)");
        else if (ImGui::BeginTable("live", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            LiveRow("film", "%s%s", r.filmEnable ? "on" : "off", r.filmInvert ? ", inverted" : "");
            LiveRow("brightness / contrast", "%.4g / %.4g", r.filmBrightness, r.filmContrast);
            LiveRow("desaturation / dark", "%.4g / %.4g", r.filmDesaturation, r.filmDesaturationDark);
            LiveRow("light tint", "%.4g %.4g %.4g", r.filmLightTint[0], r.filmLightTint[1], r.filmLightTint[2]);
            LiveRow("medium tint", "%.4g %.4g %.4g", r.filmMediumTint[0], r.filmMediumTint[1], r.filmMediumTint[2]);
            LiveRow("dark tint", "%.4g %.4g %.4g", r.filmDarkTint[0], r.filmDarkTint[1], r.filmDarkTint[2]);
            LiveRow("glow", "%s", r.glowEnable ? "on" : "off");
            LiveRow("bloom cutoff / desat", "%.4g / %.4g", r.glowBloomCutoff, r.glowBloomDesaturation);
            LiveRow("bloom intensity (x fade)", "%.4g (fade %.3g)", r.glowBloomIntensity, r.glowFade);
            LiveRow("glow radius", "%.4g", r.glowRadius);
            LiveRow("char diffuse / specular", "%.4g / %.4g", r.charDiffuseScale, r.charSpecularScale);
            ImGui::EndTable();
        }
    }

    if (st.unknownCount && ImGui::CollapsingHeader("Keys C1 does not know (kept when saving)")) {
        for (int i = 0; i < st.unknownCount; i++) {
            char k[64], v[128];
            if (VisApi_Unknown(i, k, sizeof(k), v, sizeof(v))) ImGui::BulletText("%s \"%s\"", k, v);
        }
        ImGui::TextDisabled("C1 prints \"WARNING: unknown dvar\" for these and skips them.");
    }

    if (ImGui::CollapsingHeader(".vision text")) {
        VisApi_FormatFile(g_text, sizeof(g_text));
        ImGui::InputTextMultiline("##text", g_text, sizeof(g_text), ImVec2(-FLT_MIN, 14 * ImGui::GetTextLineHeight()),
                                  ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("Copy to clipboard")) ImGui::SetClipboardText(g_text);
    }

    ImGui::Separator();
    DWORD age = GetTickCount() - st.statusTick;
    if (st.status[0]) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(age < 4000 ? ImVec4(0.6f, 1.0f, 0.6f, 1) : ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", st.status);
        ImGui::PopTextWrapPos();
    }
    char keyName[32] = "?";
    int vk = VisApi_ToggleKey();
    if (vk >= VK_F1 && vk <= VK_F24) snprintf(keyName, sizeof(keyName), "F%d", vk - VK_F1 + 1);
    else snprintf(keyName, sizeof(keyName), "key %d", vk);
    ImGui::TextDisabled("%s toggles (k33_visionKey), Esc closes. Ctrl+click a slider to type any value; right-click resets.", keyName);
    ImGui::End();
}

extern "C" int VisGui_Render(IDirect3DDevice9 *dev, int width, int height)
{
    if (!dev) return 0;
    if (!g_ctx) CreateContext(height);
    if (g_dev != dev) {
        if (g_dev) ImGui_ImplDX9_Shutdown();
        g_dev = NULL;
        if (!ImGui_ImplDX9_Init(dev)) return 0;
        g_dev = dev;
    }
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_last.QuadPart) / (float)g_freq.QuadPart;
    io.DeltaTime = dt > 0.0f && dt < 1.0f ? dt : 1.0f / 60.0f;
    g_last = now;
    FeedInput(width, height);
    ImGui_ImplDX9_NewFrame();
    ImGui::NewFrame();
    DrawEditor();
    ImGui::Render();
    VisApi_SetWantText(io.WantTextInput ? 1 : 0);
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    return 1;
}

extern "C" void VisGui_InvalidateDevice(void)
{
    if (g_dev) ImGui_ImplDX9_InvalidateDeviceObjects();
}

extern "C" void VisGui_Shutdown(void)
{
    if (g_dev) ImGui_ImplDX9_Shutdown();
    g_dev = NULL;
}
