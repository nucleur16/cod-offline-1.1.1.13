; Native complete-knob operation for the C1 actor animation owner.
; cdecl(obj, ref, weight, blend, rate, restart); notification-free.
; Retained XAnimSetGoalWeightKnob owns allocation/restart/sibling fading.
; Complete the actual selected info's ancestors, matching the parent loop in
; retained XAnimSetCompleteGoalWeight. Never look ancestors up by anim ref:
; restart clones may have identical refs but distinct native info ownership.
; No independent tick. Staged until parent reviews/integrates.
SECTION .text
global XAnimSetCompleteGoalWeightKnob
extern XAnimSetGoalWeightKnob
extern XAnimGetInfoIndex_r
extern XAnimSetGoalWeightNode
extern g_xAnimInfo
XAnimSetCompleteGoalWeightKnob:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    sub esp, 0x2c
    mov eax, [ebp+8]
    mov [esp], eax
    mov eax, [ebp+12]
    mov [esp+4], eax
    mov eax, [ebp+16]
    mov [esp+8], eax
    mov eax, [ebp+20]
    mov [esp+12], eax
    mov eax, [ebp+24]
    mov [esp+16], eax
    mov dword [esp+20], 0
    mov eax, [ebp+28]
    mov [esp+24], eax
    call XAnimSetGoalWeightKnob
    mov ebx, eax
    test ebx, ebx
    jnz .done
    mov edi, [ebp+8]
    mov edi, [edi]               ; actual native DObj::tree
    movzx ecx, word [edi+4]      ; actual native tree::children
    test ecx, ecx
    jz .missing
    mov eax, edi
    mov edx, [ebp+12]
    call XAnimGetInfoIndex_r     ; native register ABI: tree,ref,root info
    test eax, eax
    jz .missing
    mov esi, eax
.parent:
    imul eax, esi, 56
    movzx esi, word [eax+g_xAnimInfo+0x0e]
    test esi, esi
    jz .done
    imul eax, esi, 56
    pxor xmm0, xmm0
    ucomiss xmm0, [eax+g_xAnimInfo+0x28] ; native state.goalWeight
    jp .parent
    jne .parent
    mov eax, 0x3f800000
    movd xmm0, eax
    movaps xmm2, xmm0
    movss xmm1, [ebp+20]
    mov dword [esp], 0           ; notifyType
    xor ecx, ecx                ; notifyName
    mov edx, esi
    mov eax, edi
    call XAnimSetGoalWeightNode
    test eax, eax
    jz .parent
    mov ebx, eax
    jmp .done
.missing:
    mov ebx, 1                  ; caller treats violated native invariant as fatal
.done:
    mov eax, ebx
    add esp, 0x2c
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; Same complete-knob operation retaining the native notifier argument.
; cdecl(obj, ref, weight, blend, rate, restart, notifyName).
global XAnimSetCompleteGoalWeightKnobNotified
XAnimSetCompleteGoalWeightKnobNotified:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    sub esp, 0x2c
    mov eax, [ebp+8]
    mov [esp], eax
    mov eax, [ebp+12]
    mov [esp+4], eax
    mov eax, [ebp+16]
    mov [esp+8], eax
    mov eax, [ebp+20]
    mov [esp+12], eax
    mov eax, [ebp+24]
    mov [esp+16], eax
    mov eax, [ebp+32]           ; interned native notification channel
    mov [esp+20], eax
    mov eax, [ebp+28]
    mov [esp+24], eax
    call XAnimSetGoalWeightKnob
    mov ebx, eax
    test ebx, ebx
    jnz .done
    mov edi, [ebp+8]
    mov edi, [edi]               ; actual native DObj::tree
    movzx ecx, word [edi+4]      ; actual native tree::children
    test ecx, ecx
    jz .missing
    mov eax, edi
    mov edx, [ebp+12]
    call XAnimGetInfoIndex_r     ; native register ABI: tree,ref,root info
    test eax, eax
    jz .missing
    mov esi, eax
.parent:
    imul eax, esi, 56
    movzx esi, word [eax+g_xAnimInfo+0x0e]
    test esi, esi
    jz .done
    imul eax, esi, 56
    pxor xmm0, xmm0
    ucomiss xmm0, [eax+g_xAnimInfo+0x28] ; native state.goalWeight
    jp .parent
    jne .parent
    mov eax, 0x3f800000
    movd xmm0, eax
    movaps xmm2, xmm0
    movss xmm1, [ebp+20]
    mov dword [esp], 0           ; notifyType
    xor ecx, ecx                ; notifyName
    mov edx, esi
    mov eax, edi
    call XAnimSetGoalWeightNode
    test eax, eax
    jz .parent
    mov ebx, eax
    jmp .done
.missing:
    mov ebx, 1                  ; caller treats violated native invariant as fatal
.done:
    mov eax, ebx
    add esp, 0x2c
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; Knob-all plus the completion walk that T5's XAnimSetCompleteGoalWeightKnobAll
; performs through XAnimSetCompleteGoalWeightNode (OpenBLOPS xanim.cpp:3989,
; :3906): after the retained knob-all, every ancestor of the anim's record
; whose goal weight is exactly 0 or 1 is driven to goal 1 at rate 1 over the
; same blend -- the root anim's OWN record and everything above it included.
; The retained XAnimSetGoalWeightKnobAll (_xanim.asm:12467, T5 :3943) stops
; one level BELOW the root anim, and the root anim's record is created by
; XAnimEnsureGoalWeightParent with a zeroed state (goal 0, weight 0, rate 0).
; Without this walk that record never blends in, XAnimUpdateTimeAndNotetrack
; returns at it (weight 0 -> XAnimCheckFreeInfo only) and no descendant ever
; receives time: the zombie walk clip sat at w 1.00 t 0.000 forever
; (docs/ZOMBIE_ANIM_SYNC_20260921.md).
; cdecl(obj, anim, rootAnim, weight, blend, rate, notifyName, restart): the
; retained argument order, forwarded verbatim.
global XAnimSetCompleteGoalWeightKnobAll
extern XAnimSetGoalWeightKnobAll
XAnimSetCompleteGoalWeightKnobAll:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    sub esp, 0x2c
    mov eax, [ebp+8]
    mov [esp], eax
    mov eax, [ebp+12]
    mov [esp+4], eax
    mov eax, [ebp+16]
    mov [esp+8], eax
    mov eax, [ebp+20]
    mov [esp+12], eax
    mov eax, [ebp+24]
    mov [esp+16], eax
    mov eax, [ebp+28]
    mov [esp+20], eax
    mov eax, [ebp+32]
    mov [esp+24], eax
    mov eax, [ebp+36]
    mov [esp+28], eax
    call XAnimSetGoalWeightKnobAll
    mov ebx, eax
    test ebx, ebx
    jnz .done
    mov edi, [ebp+8]
    mov edi, [edi]               ; actual native DObj::tree
    movzx ecx, word [edi+4]      ; actual native tree::children
    test ecx, ecx
    jz .missing
    mov eax, edi
    mov edx, [ebp+12]
    call XAnimGetInfoIndex_r     ; native register ABI: tree,ref,root info
    test eax, eax
    jz .missing
    mov esi, eax
.parent:
    imul eax, esi, 56
    movzx esi, word [eax+g_xAnimInfo+0x0e]
    test esi, esi
    jz .done
    imul eax, esi, 56
    movss xmm0, [eax+g_xAnimInfo+0x28] ; native state.goalWeight
    pxor xmm1, xmm1
    ucomiss xmm0, xmm1
    jp .parent
    je .set                      ; goal 0: T5 completes it
    mov eax, 0x3f800000
    movd xmm1, eax
    ucomiss xmm0, xmm1
    jne .parent                  ; goal mid-fade to another value: leave it
.set:
    mov eax, 0x3f800000
    movd xmm0, eax
    movaps xmm2, xmm0
    movss xmm1, [ebp+24]
    mov dword [esp], 0           ; notifyType
    xor ecx, ecx                ; notifyName
    mov edx, esi
    mov eax, edi
    call XAnimSetGoalWeightNode
    test eax, eax
    jz .parent
    mov ebx, eax
    jmp .done
.missing:
    mov ebx, 1                  ; caller treats violated native invariant as fatal
.done:
    mov eax, ebx
    add esp, 0x2c
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
