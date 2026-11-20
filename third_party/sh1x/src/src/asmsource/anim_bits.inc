; CoDOL animation-table ABI.
;
; CoDOL's animation tree is 1024 records wide, not IW3's 512, so the
; lerpFrame animationNumber toggle bit moved from 0x200 to 0x400 and the
; record array moved out of globalScriptData into AnimationRecords.
; Every site that masks an animationNumber or indexes the record array or
; the animtree MUST use these names: a hardcoded IW3 0x200/0xfd mask turns
; a live animation index N into N+512, which reads off the end of the
; animtree (see docs/MINIGUN_CRASH_20260920.md).
%ifndef ANIM_BITS_INC
%define ANIM_BITS_INC
%ifdef OL_SERVER
extern AnimationRecords
extern AnimationCount
extern BeginAnimationRecords
%define ANIM_COUNT(reg) AnimationCount
%define ANIM_TOGGLE 0x400
%define ANIM_HIGH_MASK 0xfb
%define ANIM_HIGH_TOGGLE 0x4
%define ANIM_INDEX_MASK 0xfffffbff
%else
%define ANIM_COUNT(reg) reg+0xc000
%define ANIM_TOGGLE 0x200
%define ANIM_HIGH_MASK 0xfd
%define ANIM_HIGH_TOGGLE 0x2
%define ANIM_INDEX_MASK 0xfffffdff
%endif
%macro ANIM_BASE 1
%ifdef OL_SERVER
mov %1, AnimationRecords
%else
mov %1, [globalScriptData]
%endif
%endmacro
%macro ANIM_ADD_BASE 1
%ifdef OL_SERVER
add %1, AnimationRecords
%else
add %1, [globalScriptData]
%endif
%endmacro
%endif
