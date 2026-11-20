; The four per-weapon playerState arrays, at the full 700-slot native width.
;
; The retained IW3 playerState_t holds them inline - weapons at 0x55c,
; weaponold at 0x56c and weaponrechamber at 0x57c (four dwords each, 128 bits)
; and weaponmodels at 0x81c (128 bytes) - and cannot be made wider: sizeof is
; 0x2f64 and 521 retained sites address gclient_t members at or past it. C1
; registers 700 weapon definitions (see weapon_slots.h for the
; measurements), so the arrays moved into the playerState extension, which is
; keyed by the *storage* the state lives in and travels with it through the
; snapshot ring, the archive and a spectator's follow.
;
; Every retained site that read one of those four offsets now calls through one
; of the macros below instead. A macro replaces exactly one instruction and
; leaves the surrounding index arithmetic alone: the callee takes the word index
; the site already computed (the value that was scaled by 4 in the displaced
; address) and the bit mask it already built.
;
; Two properties make this safe where a hand-written per-site rewrite was not:
;
;   * the playerState register is named explicitly in the macro call, so the
;     [A+B+disp] base/index ambiguity that caused the 2026-09-15 spawn crash is
;     stated rather than guessed; and
;   * CODOL_WeaponBits* validates the pointer against the three real playerState
;     storage arrays before touching anything. A site that names the wrong
;     register logs "unknown playerState" with its own file/line site tag and
;     behaves as an empty inventory, instead of corrupting memory.
;
; Every macro preserves all eight general registers. The test/and/or forms leave
; the flags the instruction they replace would have left; the read forms do not
; promise flags, and none of the sites they replaced had a flag consumer.
;
; Each file that uses these macros defines WB_FILE to its own small
; integer first, so a diagnostic names the exact site.

%ifndef WEAPON_BITS_INC
%define WEAPON_BITS_INC

%ifndef WB_FILE
%define WB_FILE 0
%endif
; NASM expands __LINE__ inside a macro body to the line in *this* file, so the
; site tag is an expansion ordinal instead: the Nth macro use in the file whose
; WB_FILE this is, counted in source order. A warning naming site 3/7 is
; the seventh CODOL_WB_* line in the file that defines WB_FILE 3. The
; helper also prints its own return address, which objdump resolves exactly.
%assign WB_SEQ 0
%macro WB_TAG 0
  %assign WB_SEQ WB_SEQ+1
%endmacro
%define WB_SITE (WB_FILE * 1000000 + WB_SEQ)

; Array selectors; must match the enum in weapon_bits.h.
%define WB_OWNED     0
%define WB_OLD       1
%define WB_RECHAMBER 2

extern WeaponBitsRead
extern WeaponBitsOr
extern WeaponBitsAnd
extern WeaponModelRead
extern WeaponModelWrite
extern weapbits_result

; The scratch registers the prologue uses. An argument may not be one of them.
%macro WB_GUARD 1
  %ifidni %1,ebp
    %error "weapon_bits: ebp cannot be an argument; it is the saved stack pointer"
  %endif
  %ifidni %1,esp
    %error "weapon_bits: esp cannot be an argument"
  %endif
%endmacro

; Open a call frame. All eight registers still hold their entry values.
%macro WB_ENTER 0
	pushad
	mov ebp, esp
	and esp, 0xfffffff0
	sub esp, 0x20
%endmacro

; Close it, keeping the 32-bit return value in weapbits_result.
%macro WB_LEAVE 0
	mov [weapbits_result], eax
	mov esp, ebp
	popad
%endmacro

; ---------------------------------------------------------------- bit reads
; %1 playerState register, %2 word index (register or immediate),
; %3 bit mask (register or immediate), %4 array selector.
; Replaces  test [ps + word*4 + <array>], mask  - ZF set exactly as test would.
%macro WB_TEST 4
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0xc], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0x8], %2
	mov dword [esp], %1
	; the mask has to be captured before the call: ecx and edx are
	; caller-saved and a site may well hold the mask in one of them.
	mov dword [esp+0x10], %3
	call WeaponBitsRead
	and eax, [esp+0x10]
	WB_LEAVE
	cmp dword [weapbits_result], 0x0
%endmacro

; %1 destination register, %2 playerState, %3 word index, %4 array selector.
; Replaces  mov dst, [ps + word*4 + <array>].
%macro WB_LOAD 4
	WB_TAG
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0xc], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0x8], %3
	mov dword [esp], %2
	call WeaponBitsRead
	WB_LEAVE
	mov %1, [weapbits_result]
%endmacro

; %1 destination register (also the left operand), %2 playerState,
; %3 word index, %4 array selector.
; Replaces  and dst, [ps + word*4 + <array>].
%macro WB_ANDREG 4
	WB_TAG
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0xc], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0x8], %3
	mov dword [esp], %2
	call WeaponBitsRead
	WB_LEAVE
	and %1, [weapbits_result]
%endmacro

; --------------------------------------------------------------- bit writes
; %1 playerState, %2 word index, %3 bits, %4 array selector.
; Replaces  or [ps + word*4 + <array>], bits.
%macro WB_OR 4
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0x10], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0xc], %3
	mov dword [esp+0x8], %2
	mov dword [esp], %1
	call WeaponBitsOr
	WB_LEAVE
%endmacro

; Replaces  and [ps + word*4 + <array>], mask.
%macro WB_AND 4
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0x10], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0xc], %3
	mov dword [esp+0x8], %2
	mov dword [esp], %1
	call WeaponBitsAnd
	WB_LEAVE
%endmacro

; ------------------------------------------------------------ weapon models
; %1 destination register, %2 playerState, %3 weapon index register.
; Replaces  movzx dst, byte [ps + weapon + 0x81c].
%macro WB_MODEL_LOAD 3
	WB_TAG
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0x8], WB_SITE
	mov dword [esp+0x4], %3
	mov dword [esp], %2
	call WeaponModelRead
	WB_LEAVE
	movzx %1, byte [weapbits_result]
%endmacro

; %1 playerState, %2 weapon index register, %3 value register (32-bit form of
; the byte the site stored). Replaces  mov [ps + weapon + 0x81c], <low byte>.
%macro WB_MODEL_STORE 3
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0xc], WB_SITE
	mov dword [esp+0x8], %3
	mov dword [esp+0x4], %2
	mov dword [esp], %1
	call WeaponModelWrite
	WB_LEAVE
%endmacro

; ------------------------------------------------- byte-offset (scale-1) forms
; Some sites hold the word index already scaled by four ("lea edx,[eax*4]" or
; "shl eax,2") and address [ps + bytes + disp]. These take that byte offset and
; divide it on the stack, so the site's arithmetic still stays untouched.
%macro WB_TEST_B 4
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0xc], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0x8], %2
	shr dword [esp+0x8], 0x2
	mov dword [esp], %1
	mov dword [esp+0x10], %3
	call WeaponBitsRead
	and eax, [esp+0x10]
	WB_LEAVE
	cmp dword [weapbits_result], 0x0
%endmacro

%macro WB_OR_B 4
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0x10], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0xc], %3
	mov dword [esp+0x8], %2
	shr dword [esp+0x8], 0x2
	mov dword [esp], %1
	call WeaponBitsOr
	WB_LEAVE
%endmacro

%macro WB_AND_B 4
	WB_TAG
	WB_GUARD %1
	WB_GUARD %2
	WB_GUARD %3
	WB_ENTER
	mov dword [esp+0x10], WB_SITE
	mov dword [esp+0x4], %4
	mov dword [esp+0xc], %3
	mov dword [esp+0x8], %2
	shr dword [esp+0x8], 0x2
	mov dword [esp], %1
	call WeaponBitsAnd
	WB_LEAVE
%endmacro

; ------------------------------------------------- usercmd weapon selection
; usercmd_t.weapon (+0x18) and .offHandIndex (+0x19) are IW3 bytes; the two
; high bits of each live in .index_hi (+0x23), weapon in bits 0..1 and offhand
; in bits 2..3 (player.h). %1 destination, %2 usercmd register, %3 scratch
; (preserved; must differ from %1 and %2). No flags are promised.
%macro CMD_WEAPON 3
	push %3
	movzx %3, byte [%2+0x23]
	and %3, 0x3
	shl %3, 0x8
	movzx %1, byte [%2+0x18]
	or %1, %3
	pop %3
%endmacro
%macro CMD_OFFHAND 3
	push %3
	movzx %3, byte [%2+0x23]
	shr %3, 0x2
	and %3, 0x3
	shl %3, 0x8
	movzx %1, byte [%2+0x19]
	or %1, %3
	pop %3
%endmacro
; Replaces  cmp byte [cmd+0x18], 0  followed by a jz/jnz: ZF is set exactly as
; a compare of the whole 10-bit weapon against zero would set it.
%macro CMD_WEAPON_ISZERO 1
	cmp byte [%1+0x18], 0x0
	jnz %%done
	test byte [%1+0x23], 0x3
%%done:
%endmacro

%endif
