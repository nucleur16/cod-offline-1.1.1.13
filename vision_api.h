/*
 * vision_api.h - the interface between vision.c (C, the vision-set model: C1's cg vision state, .vision
 * files, main-thread jobs, input capture) and vision_ui.cpp (C++, Dear ImGui: the editor window and the
 * DX9 drawing). Only built with -DK33_VISION (build-dev.sh); see vision.c for the C1 addresses.
 */
#ifndef K33_VISION_API_H
#define K33_VISION_API_H

#include <windows.h>
#include <d3d9.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIS_MODES      6     /* C1's vision set types: naked, night, missilecam, thermal, pain, an unnamed sixth */
#define VIS_FIELDS     21    /* visionDefFields 0xCD67D8: the keys a .vision file can set */
#define VIS_SET_BYTES  0x70  /* visionSetVars_t */
#define VIS_NAME_LEN   64
#define VIS_MAX_LIST   512
#define VIS_MAX_UNKNOWN 32

enum { VIS_T_BOOL = 0, VIS_T_FLOAT = 1, VIS_T_VEC3 = 2 };          /* C1's own field types ("%i", "%f", "%f %f %f") */
enum { VIS_G_GLOW = 0, VIS_G_FILM = 1, VIS_G_CHAR = 2, VIS_GROUPS = 3 };

typedef struct {
    const char *id;      /* the key C1's parser matches (numeric, as C1 renamed every dvar-named string) */
    const char *iwName;  /* the IW4 visionDefFields name in the same slot */
    const char *label;
    int offset;          /* in visionSetVars_t */
    int type;            /* VIS_T_* */
    int group;           /* VIS_G_* */
    float lo, hi;        /* slider range (the IW4 tweak dvar domain); Ctrl+click types any value */
    int live;            /* 1: C1 copies it into the refdef the renderer draws with (0x47A5D0) */
    const char *help;
} VisFieldInfo;

typedef struct {
    int ok;                       /* every C1 site matched: the editor can read and write vision state */
    int inMatch;                  /* cgame has a snapshot: vision sets are live */
    int activeMode;               /* the type C1 draws with this frame (0x47A5D0's choice), -1 unknown */
    int editMode;                 /* the type edits go to */
    int autoMode;                 /* editMode follows activeMode */
    int modeStyle[VIS_MODES];     /* lerp style: 0 never set, 1 steady, 2/3 transition */
    char modeName[VIS_MODES][VIS_NAME_LEN];
    char setName[VIS_NAME_LEN];   /* what the edit buffer was loaded from */
    char source[200];
    int dirty;                    /* edited since loaded */
    int gameChanged;              /* C1 switched the edited type's set since: edits were replaced */
    int keepInCache;              /* edits also go into C1's 4-entry vision cache under setName */
    int filmTweaks, glowTweaks;   /* r_filmUseTweaks / r_glowUseTweaks: the renderer ignores film/glow sets */
    int pending;                  /* a main-thread job is queued */
    int unknownCount;             /* keys C1 does not know, kept from a disk file */
    char status[240];
    DWORD statusTick;
} VisUiState;

/* What C1 handed the renderer this frame (cg refdef, written by 0x47A5D0 from the selected set). */
typedef struct {
    int valid;
    BYTE filmEnable, filmInvert, glowEnable, pad;
    float filmBrightness, filmContrast, filmDesaturation, filmDesaturationDark;
    float filmLightTint[3], filmMediumTint[3], filmDarkTint[3];
    float glowBloomCutoff, glowBloomDesaturation, glowBloomIntensity, glowRadius;
    float charDiffuseScale, charSpecularScale;
    float glowFade;               /* set+0x6C: the glow fade C1 multiplies bloom intensity with */
} VisLiveRefdef;

enum { VIS_EV_KEY = 1, VIS_EV_CHAR, VIS_EV_MOUSE, VIS_EV_WHEEL, VIS_EV_HWHEEL, VIS_EV_FOCUS, VIS_EV_MOUSEPOS };
/* mods: 1 ctrl, 2 shift, 4 alt. MOUSEPOS: a, b client pixels, lp the client size (w | h << 16). */
typedef struct { int type, a, b; unsigned lp; int mods; } VisInputEvent;

/* vision.c -> vision_ui.cpp */
const VisFieldInfo *VisApi_Field(int i);
const char *VisApi_GroupLabel(int g);
const char *VisApi_ModeLabel(int m);
void VisApi_GetState(VisUiState *s);
void VisApi_GetValues(BYTE *edit, BYTE *loaded);
void VisApi_GetLive(VisLiveRefdef *r);
void VisApi_SetField(int i, const void *value);
void VisApi_SetEditMode(int mode);           /* -1: follow the active type */
void VisApi_SetKeepInCache(int on);
void VisApi_Revert(void);
void VisApi_Reread(void);
void VisApi_Reapply(void);
void VisApi_LoadZone(const char *name, int durationMs, int viaGame);
int VisApi_LoadDisk(const char *name);
int VisApi_Save(const char *name, char *pathOut, int pathSize);
int VisApi_ListZone(char (*names)[VIS_NAME_LEN], int max);
int VisApi_ListDisk(char (*names)[VIS_NAME_LEN], int max);
int VisApi_FormatFile(char *buf, int size);
int VisApi_Unknown(int i, char *key, int keySize, char *value, int valueSize);
void VisApi_Close(void);
int VisApi_PopInput(VisInputEvent *ev);
HWND VisApi_Hwnd(void);
void VisApi_SetWantText(int on);
const char *VisApi_SaveDir(void);
const char *VisApi_IniPath(void);
int VisApi_ToggleKey(void);

/* vision_ui.cpp -> vision.c (render thread, under vision.c's GUI lock) */
int VisGui_Render(IDirect3DDevice9 *dev, int width, int height);
void VisGui_InvalidateDevice(void);
void VisGui_Shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
