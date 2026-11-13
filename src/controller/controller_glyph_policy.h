#pragma once
/*
 * controller_glyph_policy.h - what button hints DISPLAY, as a pure function of the player's choice
 * (dvar gpad_glyphs), the detected pad type and the last-used input device. No Windows, no C1: the same
 * header is compiled into Kernel33.dll (controller.c) and into tests/controller_glyph_policy_test.c.
 *
 * It decides display only. The last-used device itself (g_lastDevice in controller.c: aim assist, the
 * userinfo dvar "gpad", the menu cursor's snap scan, input handling) is never an output of this policy.
 *
 *   gpad_glyphs   family shown          bindings shown                 pad keys drawn as
 *   0 automatic   detected pad type     last-used device               icons
 *   1 Xbox 360    Xbox                  pad, always                    icons
 *   2 PS3         PlayStation           pad, always                    icons
 *   3 Wii U       Nintendo              pad, always                    icons
 *   4 generic     generic               pad, always                    icons
 *   5 text names  detected pad type     last-used device               text names
 *   6 keyboard    detected pad type     keyboard / mouse, always       icons
 *
 * "Bindings shown" picks which device's keys a hint names first; a command bound on the other device only
 * still shows that key (controller.c CtlCommandKeys' second pass), which is where the family and the
 * icon / text choice still matter in mode 6. A value outside 0..6 is treated as 0.
 */

#define CGP_GLYPHS_AUTO     0
#define CGP_GLYPHS_XBOX     1
#define CGP_GLYPHS_PS       2
#define CGP_GLYPHS_NINTENDO 3
#define CGP_GLYPHS_GENERIC  4
#define CGP_GLYPHS_TEXT     5
#define CGP_GLYPHS_KBM      6
#define CGP_GLYPHS_MAX      6

#define CGP_FAMILY_XBOX     0   /* the values of g_padType and of the glyph sets (controller_glyphs.c) */
#define CGP_FAMILY_PS       1
#define CGP_FAMILY_NINTENDO 2
#define CGP_FAMILY_GENERIC  3

#define CGP_DEVICE_KBM      0   /* the values of g_lastDevice */
#define CGP_DEVICE_PAD      1

#define CGP_SERIAL_MAX      1000000000   /* k33_glyphSerial wraps to 1 after this; 0 = nothing published yet */
#define CGP_INFO_LEN        7            /* "f|d|t|g" */

typedef struct {
    int family;     /* CGP_FAMILY_*: the pad family whose names and icons are shown */
    int device;     /* CGP_DEVICE_*: the device whose bindings hints name first */
    int text_only;  /* 1: pad keys as text names, never icons */
    int forced;     /* 1: the family is the player's choice (gpad_glyphs 1..4), not the detected pad's */
} CtlGlyphDisplay;

static inline CtlGlyphDisplay CtlGlyphPolicy(int glyphs, int padType, int lastDevice)
{
    CtlGlyphDisplay d;
    int detected = padType >= CGP_FAMILY_XBOX && padType <= CGP_FAMILY_GENERIC ? padType : CGP_FAMILY_GENERIC;
    int last = lastDevice == CGP_DEVICE_PAD ? CGP_DEVICE_PAD : CGP_DEVICE_KBM;
    d.family = detected;
    d.device = last;
    d.text_only = 0;
    d.forced = 0;
    if (glyphs >= CGP_GLYPHS_XBOX && glyphs <= CGP_GLYPHS_GENERIC) {
        d.family = glyphs - CGP_GLYPHS_XBOX;
        d.device = CGP_DEVICE_PAD;
        d.forced = 1;
    } else if (glyphs == CGP_GLYPHS_TEXT) {
        d.text_only = 1;
    } else if (glyphs == CGP_GLYPHS_KBM) {
        d.device = CGP_DEVICE_KBM;
    }
    return d;
}

/* What a change of bumps k33_glyphSerial: family, device, text-only. (forced follows from the dvar alone and
 * cannot change a label on its own.) */
static inline int CtlGlyphDisplaySame(const CtlGlyphDisplay *a, const CtlGlyphDisplay *b)
{
    return a->family == b->family && a->device == b->device && a->text_only == b->text_only;
}

static inline int CtlGlyphSerialNext(int serial)
{
    return serial < 1 || serial >= CGP_SERIAL_MAX ? 1 : serial + 1;
}

/* k33_glyphinfo, the display state the Scaleform menus and HUD read: "family|device|textOnly|glyphs", one
 * digit each (family 0 Xbox, 1 PlayStation, 2 Nintendo, 3 generic; device 0 keyboard / mouse, 1 pad; glyphs =
 * the gpad_glyphs value in effect, 0..6). Writes CGP_INFO_LEN characters and a 0 into out
 * (n >= CGP_INFO_LEN + 1) and returns the length, or 0 when out is too small. */
static inline int CtlGlyphInfo(char *out, int n, const CtlGlyphDisplay *d, int glyphs)
{
    if (!out || n < CGP_INFO_LEN + 1) return 0;
    if (glyphs < 0 || glyphs > CGP_GLYPHS_MAX) glyphs = CGP_GLYPHS_AUTO;
    out[0] = (char)('0' + (d->family >= CGP_FAMILY_XBOX && d->family <= CGP_FAMILY_GENERIC ? d->family : CGP_FAMILY_GENERIC));
    out[1] = '|';
    out[2] = (char)('0' + (d->device == CGP_DEVICE_PAD));
    out[3] = '|';
    out[4] = (char)('0' + (d->text_only != 0));
    out[5] = '|';
    out[6] = (char)('0' + glyphs);
    out[7] = 0;
    return CGP_INFO_LEN;
}
