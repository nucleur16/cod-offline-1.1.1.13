/* C1's axial brush phase, the part of CM_TraceThroughBrush that differs
 * from CoD4.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure functions, no engine state, so the retained collision assembly and
 * the unit test share them. Addresses are C1.exe 1.1.1.13.
 *
 * C1 CM_TraceThroughBrush 0x5854f0 tests the brush's six axial faces one
 * AXIS at a time from the trace's midpoint form (IW4 TraceExtents:
 * midPoint, halfDelta, halfDeltaAbs, invDeltaAbs; set by 0x586a10/0x588780)
 * and the brush's own midPoint/halfSize bounds. Its side planes are then
 * tested exactly as CoD4 does (0x585600..0x585907 matches the retained
 * loop: min(d1, 0.125), d1 - 0.125 entry, d1 / (d1 - d2) exit).
 *
 * The axial difference that matters is the STATIC axis. 0x586b32..0x586b75
 * flags x and y (never z: tw+0xbe is stored 0) as static when the trace's
 * delta on that axis is exactly zero, and 0x585573..0x5855e2 then only
 * asks whether the box is 0.125 or more clear of the brush on it:
 *
 *     fabs(mid[i] - brushMid[i]) - (size[i] + brushHalf[i]) >= 0.125
 *         -> the brush is missed
 *     otherwise the axis is ignored (no entry, no exit, no allsolid change)
 *
 * CoD4 instead misses as soon as the separation is positive. So a box
 * resting 0.124 units from a wall, traced straight down (PM_GroundTrace,
 * the step-up and step-down traces: x and y static), is STARTSOLID and
 * ALLSOLID in C1 and clear in CoD4. C1's PM_GroundTrace then runs
 * PM_CorrectAllSolid 0x411600 and the player pops about one unit off the
 * wall; its PM_SlideMove 0x416f80 meets the allsolid trace by zeroing only
 * velocity[2] and keeping the velocity into the wall. That is the client's
 * "about 1 unit off the wall, vy -47 into it" against a server that stayed
 * flush and clipped vy to 0.
 *
 * The moving axes use C1's arithmetic too (one face per axis, the entry
 * fraction (D - 0.125) * invDeltaAbs, the exit fraction
 * (2 * halfDeltaAbs - C) * invDeltaAbs), every step rounded to float as the
 * client's 24-bit x87 does, so a hit lands on the same side of 0.125 that
 * C1's prediction lands on.
 */
#ifndef C1_BRUSH_TRACE_H
#define C1_BRUSH_TRACE_H

#include <stdint.h>

#define C1_BRUSH_MISS (-1)
#define C1_BRUSH_NO_LEAD 0
#define C1_BRUSH_AXIAL_LEAD 1

/* start/end: the trace's box centres (start + boundsMid, as both engines
 * store them); size: the per-axis box extent C1 keeps at tw+0xa0 and the
 * retained code at tw->radiusOffset; bmins/bmaxs: the brush bounds;
 * material: cbrush_t axialMaterialNum ([0] = mins faces, [1] = maxs).
 * enterFrac/leaveFrac/allsolid carry the per-brush state in and out exactly
 * as 0x5854f0 keeps it ([ebp-0x34], [ebp-0x30], [ebp-0x1d]). On an axial
 * lead, leadNormal/leadMaterial receive the entered face. */
int C1BrushAxialPhase(const float start[3], const float end[3],
                            const float size[3], const float bmins[3],
                            const float bmaxs[3], const int16_t material[2][3],
                            float *enterFrac, float *leaveFrac,
                            unsigned char *allsolid, float leadNormal[3],
                            int *leadMaterial);

/* Called from the retained CM_TraceThroughLeafBrushNode_r (_cm_trace.asm)
 * in place of the CoD4 axial loop. tw is the retained traceWork_t
 * (extents.start +0x00, extents.end +0x0c, radiusOffset +0x90); brush is a
 * cbrush_t (mins +0x00, maxs +0x10, axialMaterialNum +0x24); fracs is
 * {enterFrac, leaveFrac}. Returns C1_BRUSH_MISS, _NO_LEAD or
 * _AXIAL_LEAD. */
int __attribute__((cdecl)) C1TraceBrushAxial(const float *tw,
    const unsigned char *brush, float *fracs, unsigned char *allsolid,
    float *leadNormal, int *leadMaterial);

#endif
