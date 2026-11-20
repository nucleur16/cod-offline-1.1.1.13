; The item index: one number naming a weapon and its world-model variant.
;
; bg_itemlist is indexed by weapon + stride * model, and the same number is
; stored in gentity item[].index, in an item entity's s.index and in a pickup
; missile's s.index. The retained IW3 code used a stride of 128 (shl 7 to
; build it, and 7f / sar 7 to take it apart), the width of the IW3 weapon
; table. C1 uses 700, the width of its weapon table (weapon_slots.h):
;   0x0052FBC3 imul edi, 0x2bc      building an item's index
;   0x0043E94C idiv 0x2bc            the client taking s+0x98 apart (the
;                                    remainder is the weapon, 0x43E600)
;   0x0040C94D / 0x00529C9B idiv     hint and G_SpawnItem doing the same
; With 700 native weapon slots a stride of 128 is ambiguous: native weapon 130
; came back as weapon 2 (scrambler_mp), and every composed weapon a human
; drops was picked up as some other weapon. Registration also marked
; bg_itemlist[w + 128 * 15] for every weapon, which ran past the 0x2000-byte
; table into bg_numAmmoTypes and the arrays after it once w reached 128.
;
; Every retained site that took an index apart now uses the macros below.
; Each preserves every register except its destination; flags are undefined
; afterwards (the sign fix-up branches that followed the old "and" are gone -
; an item index is never negative).

%ifndef ITEM_INDEX_INC
%define ITEM_INDEX_INC

%ifdef OL_SERVER
%define ITEM_STRIDE 700
%else
%define ITEM_STRIDE 128
%endif
; Sixteen world-model variants per weapon, as before.
%define ITEM_MODELS 16
%define ITEM_COUNT (ITEM_STRIDE * ITEM_MODELS)

; %1 destination register, %2 source (register or memory not addressed
; through esp), %3 edx for the remainder (weapon) or eax for the quotient
; (model).
%macro ITEM_DIVMOD 3
  %ifidni %1,esp
    %error "item_index: esp cannot be the destination"
  %endif
	push ecx
	push edx
	push eax
	mov eax, %2
	cdq
	mov ecx, ITEM_STRIDE
	idiv ecx
  %ifidni %1,eax
	mov [esp], %3
  %elifidni %1,edx
	mov [esp+0x4], %3
  %elifidni %1,ecx
	mov [esp+0x8], %3
  %else
	mov %1, %3
  %endif
	pop eax
	pop edx
	pop ecx
%endmacro

; dst = weapon of the item index in src (replaces "and dst, 0x8000007f").
%macro ITEM_WEAPON 2
	ITEM_DIVMOD %1, %2, edx
%endmacro

; dst = world-model variant of the item index in src (replaces the signed
; divide-by-128 "lea/cmp/cmovle/sar 7" sequence).
%macro ITEM_MODEL 2
	ITEM_DIVMOD %1, %2, eax
%endmacro

%endif
