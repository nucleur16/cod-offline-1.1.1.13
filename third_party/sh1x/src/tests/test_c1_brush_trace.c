/* C1's axial brush phase (src/online/collision/c1_brush_trace.c), and a
 * replay of C1's PM_GroundTrace / PM_CorrectAllSolid beside a wall built on
 * it, against results worked out by hand from C1.exe 0x5854f0, 0x584da0,
 * 0x411910 and 0x411600. */
#include "c1_brush_trace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float player_size[3] = { 15.0f, 15.0f, 35.0f };
static const int16_t materials[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };

typedef struct { float mins[3], maxs[3]; } box_brush_t;

/* Wall face at y = -312 (the killhouse wall, normal +y), floor top at z = 0. */
static const box_brush_t world[2] = {
    { { -1000.0f, -1000.0f, -64.0f }, { 1000.0f, 1000.0f, 0.0f } },
    { { -1000.0f, -400.0f, 0.0f }, { 1000.0f, -312.0f, 200.0f } },
};

typedef struct {
    float fraction, normal[3];
    int startsolid, allsolid, material;
} c1_trace_t;

static int axial(const float start[3], const float end[3], const box_brush_t *b,
                 float *enter, float *leave, unsigned char *allsolid,
                 float normal[3], int *material)
{
    *enter = 0.0f;
    *allsolid = 1;
    return C1BrushAxialPhase(start, end, player_size, b->mins, b->maxs,
                                   materials, enter, leave, allsolid, normal,
                                   material);
}

/* C1 position test 0x584da0 for a sideless brush: inside only while every
 * axis overlaps (size + half > |centre - mid|). */
static int position_inside(const float c[3], const box_brush_t *b)
{
    int i;
    for(i = 0; i < 3; ++i) {
        float mid = (b->mins[i] + b->maxs[i]) * 0.5f;
        float half = (b->maxs[i] - b->mins[i]) * 0.5f;
        float diff = c[i] - mid;
        if(player_size[i] + half <= fabsf(diff)) return 0;
    }
    return 1;
}

/* A player trace of origins (mins z 0, maxs z 70: centre = origin + 35). */
static c1_trace_t trace(const float o0[3], const float o1[3])
{
    c1_trace_t tr;
    float s[3] = { o0[0], o0[1], o0[2] + 35.0f };
    float e[3] = { o1[0], o1[1], o1[2] + 35.0f };
    unsigned i;
    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.0f;
    if(!memcmp(o0, o1, sizeof(float[3]))) {
        for(i = 0; i < 2; ++i)
            if(position_inside(s, &world[i])) {
                tr.fraction = 0.0f; tr.startsolid = tr.allsolid = 1;
            }
        return tr;
    }
    for(i = 0; i < 2; ++i) {
        float enter, leave = tr.fraction, normal[3];
        unsigned char allsolid;
        int material, r = axial(s, e, &world[i], &enter, &leave, &allsolid,
                                normal, &material);
        if(r == C1_BRUSH_MISS) continue;
        if(r == C1_BRUSH_AXIAL_LEAD) {
            tr.fraction = enter;
            memcpy(tr.normal, normal, sizeof(normal));
            tr.material = material;
        } else {
            tr.startsolid = 1;
            if(allsolid) { tr.allsolid = 1; tr.fraction = 0.0f; }
        }
    }
    return tr;
}

/* C1 PM_CorrectAllSolid 0x411600 (table 0xc3ee40). */
static int correct_allsolid(float origin[3])
{
    static const signed char delta[26][3] = {
        {0,0,1},{-1,0,1},{0,-1,1},{1,0,1},{0,1,1},
        {-1,0,0},{0,-1,0},{1,0,0},{0,1,0},{0,0,-1},
        {-1,0,-1},{0,-1,-1},{1,0,-1},{0,1,-1},
        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1},
        {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1}
    };
    unsigned i, k;
    for(i = 0; i < 26; ++i) {
        float point[3], end[3];
        c1_trace_t tr;
        for(k = 0; k < 3; ++k) point[k] = origin[k] + delta[i][k];
        tr = trace(point, point);
        if(tr.startsolid) continue;
        memcpy(end, point, sizeof(end));
        end[2] = (end[2] - 1.0f) - 0.25f;
        tr = trace(point, end);
        if(tr.startsolid) continue;
        for(k = 0; k < 3; ++k) origin[k] = point[k] + (end[k] - point[k]) * tr.fraction;
        return 1;
    }
    return 0;
}

/* C1 PM_GroundTrace 0x411910 down to the allsolid correction. */
static c1_trace_t ground_trace(float origin[3])
{
    float s[3] = { origin[0], origin[1], origin[2] + 0.25f };
    float e[3] = { origin[0], origin[1], origin[2] - 0.25f };
    c1_trace_t tr = trace(s, e);
    if(tr.allsolid) assert(correct_allsolid(origin));
    return tr;
}

int main(void)
{
    float enter, leave, normal[3];
    unsigned char allsolid;
    int material, r;

    /* 1. Straight down, 0.12 clear of the wall's +y face: x and y are static,
     *    0.12 < 0.125, so C1 rejects nothing and the box is all solid. CoD4
     *    misses this brush (any positive gap on a static axis). */
    {
        float s[3] = { 0.0f, -312.0f + 15.12f, 35.375f };
        float e[3] = { 0.0f, -312.0f + 15.12f, 34.875f };
        leave = 1.0f;
        r = axial(s, e, &world[1], &enter, &leave, &allsolid, normal, &material);
        assert(r == C1_BRUSH_NO_LEAD);
        assert(allsolid == 1);
    }
    /* 2. Exactly 0.125 clear: the static test is >= 0.125, so C1 misses. */
    {
        float s[3] = { 0.0f, -312.0f + 15.125f, 35.375f };
        float e[3] = { 0.0f, -312.0f + 15.125f, 34.875f };
        leave = 1.0f;
        r = axial(s, e, &world[1], &enter, &leave, &allsolid, normal, &material);
        assert(r == C1_BRUSH_MISS);
    }
    /* 3. Walking into the wall from 0.5 clear, one unit of -y travel: the
     *    entry is (0.5 - 0.125) / 1 = 0.375 on the maxs face, normal +y. */
    {
        float s[3] = { 0.0f, -312.0f + 15.5f, 35.125f };
        float e[3] = { 0.0f, -312.0f + 14.5f, 35.125f };
        leave = 1.0f;
        r = axial(s, e, &world[1], &enter, &leave, &allsolid, normal, &material);
        assert(r == C1_BRUSH_AXIAL_LEAD);
        assert(enter == 0.375f);
        assert(normal[0] == 0.0f && normal[1] == 1.0f && normal[2] == 0.0f);
        assert(material == materials[1][1]);
        assert(allsolid == 1);   /* the end stays outside the far face */
    }
    /* 4. Within 0.125 and still moving in: entry clamps to 0, same face. */
    {
        float s[3] = { 3.0f, -312.0f + 15.1f, 35.125f };
        float e[3] = { 2.0f, -312.0f + 14.9f, 35.125f };
        leave = 1.0f;
        r = axial(s, e, &world[1], &enter, &leave, &allsolid, normal, &material);
        assert(r == C1_BRUSH_AXIAL_LEAD);
        assert(enter == 0.0f && normal[1] == 1.0f);
    }
    /* 5. Moving away from the wall misses it. */
    {
        float s[3] = { 0.0f, -312.0f + 15.1f, 35.125f };
        float e[3] = { -1.0f, -312.0f + 16.0f, 35.125f };
        leave = 1.0f;
        r = axial(s, e, &world[1], &enter, &leave, &allsolid, normal, &material);
        assert(r == C1_BRUSH_MISS);
    }
    /* 6. A level move resting 0.125 above the floor misses it (z is never
     *    static: zero delta counts as rising, the floor is behind). */
    {
        float s[3] = { 0.0f, 0.0f, 35.125f };
        float e[3] = { 4.0f, 3.0f, 35.125f };
        leave = 1.0f;
        r = axial(s, e, &world[0], &enter, &leave, &allsolid, normal, &material);
        assert(r == C1_BRUSH_MISS);
    }
    /* 7. ...but a ceiling 0.1 above the head of a level move is entered at
     *    fraction 0 through its mins face. */
    {
        box_brush_t ceiling = { { -100.0f, -100.0f, 70.225f }, { 100.0f, 100.0f, 90.0f } };
        float s[3] = { 0.0f, 0.0f, 35.125f };
        float e[3] = { 4.0f, 3.0f, 35.125f };
        enter = 0.0f; leave = 1.0f; allsolid = 1;
        r = C1BrushAxialPhase(s, e, player_size, ceiling.mins, ceiling.maxs,
                                    materials, &enter, &leave, &allsolid,
                                    normal, &material);
        assert(r == C1_BRUSH_AXIAL_LEAD);
        assert(enter == 0.0f && normal[2] == -1.0f && material == materials[0][2]);
    }
    /* 8. The observed behaviour. A player sliding along the wall rests
     *    0.12 clear of it (origin y = -312 + 15.12). C1's ground trace is
     *    all solid, PM_CorrectAllSolid walks its table: (0,0,+1), (-1,0,+1)
     *    and (+1,0,+1) are clear but drop into the same wall gap (start
     *    solid), (0,-1,+1) is inside the wall, and (0,+1,+1) is the first
     *    that lands. The player ends one unit off the wall, back on the
     *    floor at z 0.125. */
    {
        float origin[3] = { 100.0f, -312.0f + 15.12f, 0.125f };
        c1_trace_t tr = ground_trace(origin);
        assert(tr.allsolid);
        assert(origin[0] == 100.0f);
        assert(origin[1] == (-312.0f + 15.12f) + 1.0f);
        assert(origin[2] == 0.125f);
        /* From there the next ground trace is clean: on the floor. */
        tr = ground_trace(origin);
        assert(!tr.startsolid && tr.fraction == 0.5f && tr.normal[2] == 1.0f);
    }
    /* 9. Resting exactly 0.125 clear (or more) never pops. */
    {
        float origin[3] = { 100.0f, -312.0f + 15.125f, 0.125f };
        c1_trace_t tr = ground_trace(origin);
        assert(!tr.startsolid);
        assert(origin[1] == -312.0f + 15.125f);
    }
    /* 10. A trace that ends inside the brush's far side clears allsolid and
     *     caps the leave fraction (C1 0x585712..0x585747). */
    {
        box_brush_t post = { { -10.0f, -10.0f, 0.0f }, { 10.0f, 10.0f, 100.0f } };
        float s[3] = { 0.0f, 0.0f, 50.0f };
        float e[3] = { 40.0f, 0.0f, 50.0f };
        enter = 0.0f; leave = 1.0f; allsolid = 1;
        r = C1BrushAxialPhase(s, e, player_size, post.mins, post.maxs,
                                    materials, &enter, &leave, &allsolid,
                                    normal, &material);
        /* start inside: x leaves the far face at (25 - 0) / 40 = 0.625 */
        assert(r == C1_BRUSH_NO_LEAD);
        assert(allsolid == 0);
        assert(leave == 0.625f);
    }
    /* 11. Session 9's wall pop is C1's own arithmetic, not a server one.
     *     mp_killhouse_sh brush (3612, 812, 144) +- (148, 4, 12) as the zone
     *     stores it: face y = 816, so a player 0.125 clear rests at
     *     y = 831.125. C1 PM_SlideMove 0x4170c6..0x417174 builds
     *     end = origin + velocity * time and lands at
     *     (end - origin) * fraction + origin, every step a float (x87 at 24
     *     bits); the trace's entry fraction is (D - 0.125) * invDeltaAbs
     *     (0x585761..0x585797). For this approach the three roundings land
     *     ONE float step inside the 0.125 band (831.12494), and C1's next
     *     ground trace (x, y static) is all solid and pops +1 y -- exactly
     *     the server's t2322703 line (831.1249 -> 832.1249). The same code
     *     lands on 831.125 for most approaches (about 97% in a sweep of
     *     wall hits), so which side a client and a server land on follows
     *     the sub-0.001 drift between them, not a difference in the math. */
    {
        box_brush_t ledge = { { 3464.0f, 808.0f, 132.0f }, { 3760.0f, 816.0f, 156.0f } };
        float o[3] = { 3654.4f, 831.1309204101562f, 132.125f };
        float v[3] = { 64.7f, -18.262584686279297f, 0.0f };
        float t = 0.016f, end[3], s[3], e[3], landed;
        unsigned k;
        for(k = 0; k < 3; ++k) end[k] = o[k] + v[k] * t;
        for(k = 0; k < 3; ++k) { s[k] = o[k]; e[k] = end[k]; }
        s[2] += 35.0f; e[2] += 35.0f;
        enter = 0.0f; leave = 1.0f; allsolid = 1;
        r = C1BrushAxialPhase(s, e, player_size, ledge.mins, ledge.maxs,
                                    materials, &enter, &leave, &allsolid,
                                    normal, &material);
        assert(r == C1_BRUSH_AXIAL_LEAD && normal[1] == 1.0f);
        landed = (end[1] - o[1]) * enter + o[1];
        assert(landed == nextafterf(831.125f, 0.0f));
        /* The ground trace from there: x and y static, 0.12494 < 0.125. */
        {
            float gs[3] = { end[0], landed, 132.375f + 35.0f };
            float ge[3] = { end[0], landed, 131.875f + 35.0f };
            enter = 0.0f; leave = 1.0f; allsolid = 1;
            r = C1BrushAxialPhase(gs, ge, player_size, ledge.mins, ledge.maxs,
                                        materials, &enter, &leave, &allsolid,
                                        normal, &material);
            assert(r != C1_BRUSH_MISS && allsolid == 1);
        }
    }
    puts("test_c1_brush_trace: ok");
    return 0;
}
