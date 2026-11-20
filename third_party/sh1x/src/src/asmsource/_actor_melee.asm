; Native cdecl entry to retained melee trace/damage implementation.
; The retained body accepts entity EAX, weaponParms EDX, dimensions XMM0..2.
; This is a linked native ABI adapter, not a runtime hook.
section .text
extern Weapon_Melee_internal
global ActorMeleeTrace
ActorMeleeTrace:
    mov eax, [esp+4]
    mov edx, [esp+8]
    movss xmm0, [esp+12]
    movss xmm1, [esp+16]
    movss xmm2, [esp+20]
    jmp Weapon_Melee_internal
