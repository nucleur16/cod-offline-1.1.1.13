#include <stdio.h>
#include <string.h>
#include "../controller_glyph_policy.h"
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static int is(CtlGlyphDisplay d, int family, int device, int text, int forced)
{
    return d.family == family && d.device == device && d.text_only == text && d.forced == forced;
}
int main(void)
{
    char tail[16];
    CtlGlyphDisplay a, b;
    /* 0: family by the detected pad, device by the last input, icons */
    for (int type = 0; type < 4; type++) {
        CHECK(is(CtlGlyphPolicy(0, type, CGP_DEVICE_PAD), type, CGP_DEVICE_PAD, 0, 0));
        CHECK(is(CtlGlyphPolicy(0, type, CGP_DEVICE_KBM), type, CGP_DEVICE_KBM, 0, 0));
    }
    /* 1..4: that family and pad bindings, whatever is plugged in and whatever was used last */
    for (int g = 1; g <= 4; g++)
        for (int type = -1; type <= 4; type++)
            for (int last = 0; last <= 1; last++)
                CHECK(is(CtlGlyphPolicy(g, type, last), g - 1, CGP_DEVICE_PAD, 0, 1));
    CHECK(CtlGlyphPolicy(CGP_GLYPHS_XBOX, 1, 0).family == CGP_FAMILY_XBOX);
    CHECK(CtlGlyphPolicy(CGP_GLYPHS_PS, 0, 0).family == CGP_FAMILY_PS);
    CHECK(CtlGlyphPolicy(CGP_GLYPHS_NINTENDO, 0, 0).family == CGP_FAMILY_NINTENDO);
    CHECK(CtlGlyphPolicy(CGP_GLYPHS_GENERIC, 0, 0).family == CGP_FAMILY_GENERIC);
    /* 5: as 0, with text names */
    for (int type = 0; type < 4; type++) {
        CHECK(is(CtlGlyphPolicy(5, type, CGP_DEVICE_PAD), type, CGP_DEVICE_PAD, 1, 0));
        CHECK(is(CtlGlyphPolicy(5, type, CGP_DEVICE_KBM), type, CGP_DEVICE_KBM, 1, 0));
    }
    /* 6: keyboard / mouse bindings even while the pad is in use; the family stays the detected pad's */
    for (int type = 0; type < 4; type++)
        for (int last = 0; last <= 1; last++)
            CHECK(is(CtlGlyphPolicy(6, type, last), type, CGP_DEVICE_KBM, 0, 0));
    /* out of range: the dvar as 0, the pad type as generic, the device as keyboard / mouse */
    CHECK(is(CtlGlyphPolicy(7, 1, CGP_DEVICE_PAD), 1, CGP_DEVICE_PAD, 0, 0));
    CHECK(is(CtlGlyphPolicy(-1, 2, CGP_DEVICE_KBM), 2, CGP_DEVICE_KBM, 0, 0));
    CHECK(is(CtlGlyphPolicy(0, -1, CGP_DEVICE_PAD), CGP_FAMILY_GENERIC, CGP_DEVICE_PAD, 0, 0));
    CHECK(is(CtlGlyphPolicy(0, 4, CGP_DEVICE_PAD), CGP_FAMILY_GENERIC, CGP_DEVICE_PAD, 0, 0));
    CHECK(is(CtlGlyphPolicy(0, 0, 2), 0, CGP_DEVICE_KBM, 0, 0));
    CHECK(is(CtlGlyphPolicy(0, 0, -1), 0, CGP_DEVICE_KBM, 0, 0));
    /* the serial: bumps for family, device and text-only; not for the last device under an explicit choice */
    a = CtlGlyphPolicy(0, 0, CGP_DEVICE_KBM);
    b = CtlGlyphPolicy(0, 0, CGP_DEVICE_PAD);
    CHECK(!CtlGlyphDisplaySame(&a, &b));
    b = CtlGlyphPolicy(0, 1, CGP_DEVICE_KBM);
    CHECK(!CtlGlyphDisplaySame(&a, &b));
    b = CtlGlyphPolicy(5, 0, CGP_DEVICE_KBM);
    CHECK(!CtlGlyphDisplaySame(&a, &b));
    b = CtlGlyphPolicy(6, 0, CGP_DEVICE_KBM);
    CHECK(CtlGlyphDisplaySame(&a, &b));
    for (int g = 1; g <= 4; g++) {
        a = CtlGlyphPolicy(g, 0, CGP_DEVICE_KBM);
        b = CtlGlyphPolicy(g, 3, CGP_DEVICE_PAD);
        CHECK(CtlGlyphDisplaySame(&a, &b));
    }
    a = CtlGlyphPolicy(6, 2, CGP_DEVICE_KBM);
    b = CtlGlyphPolicy(6, 2, CGP_DEVICE_PAD);
    CHECK(CtlGlyphDisplaySame(&a, &b));
    a = CtlGlyphPolicy(1, 0, CGP_DEVICE_PAD);   /* forced Xbox against a detected Xbox pad in use: same labels */
    b = CtlGlyphPolicy(0, 0, CGP_DEVICE_PAD);
    CHECK(CtlGlyphDisplaySame(&a, &b) && a.forced != b.forced);
    CHECK(CtlGlyphSerialNext(0) == 1);
    CHECK(CtlGlyphSerialNext(1) == 2);
    CHECK(CtlGlyphSerialNext(-5) == 1);
    CHECK(CtlGlyphSerialNext(CGP_SERIAL_MAX - 1) == CGP_SERIAL_MAX);
    CHECK(CtlGlyphSerialNext(CGP_SERIAL_MAX) == 1);
    /* k33_glyphinfo */
    a = CtlGlyphPolicy(2, 0, CGP_DEVICE_KBM);
    CHECK(CtlGlyphInfo(tail, sizeof(tail), &a, 2) == CGP_INFO_LEN && !strcmp(tail, "1|1|0|2"));
    a = CtlGlyphPolicy(5, 3, CGP_DEVICE_KBM);
    CHECK(CtlGlyphInfo(tail, sizeof(tail), &a, 5) == CGP_INFO_LEN && !strcmp(tail, "3|0|1|5"));
    a = CtlGlyphPolicy(6, 2, CGP_DEVICE_PAD);
    CHECK(CtlGlyphInfo(tail, sizeof(tail), &a, 6) == CGP_INFO_LEN && !strcmp(tail, "2|0|0|6"));
    a = CtlGlyphPolicy(0, 0, CGP_DEVICE_PAD);
    CHECK(CtlGlyphInfo(tail, sizeof(tail), &a, 0) == CGP_INFO_LEN && !strcmp(tail, "0|1|0|0"));
    CHECK(CtlGlyphInfo(tail, sizeof(tail), &a, 9) == CGP_INFO_LEN && !strcmp(tail, "0|1|0|0"));
    a.family = 9;
    CHECK(CtlGlyphInfo(tail, sizeof(tail), &a, 0) == CGP_INFO_LEN && !strcmp(tail, "3|1|0|0"));
    CHECK(CtlGlyphInfo(tail, CGP_INFO_LEN, &a, 0) == 0);
    CHECK(CtlGlyphInfo(NULL, 16, &a, 0) == 0);
    printf("Controller glyph policy: %u checks passed\n", checks);
    return 0;
}
