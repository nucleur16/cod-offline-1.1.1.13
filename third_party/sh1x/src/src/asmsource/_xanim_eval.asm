BITS 32
SECTION .text
global DispatchXAnimLeaf
extern XAnimCalc
; cdecl(info, weight, calcInfo, output) to the retained leaf evaluator ABI.
DispatchXAnimLeaf:
    push ebp
    mov ebp, esp
    and esp, -16
    sub esp, 16
    mov dword [esp], 0
    mov eax, [ebp+16]
    mov [esp+4], eax
    mov dword [esp+8], 0
    mov eax, [ebp+20]
    mov [esp+12], eax
    xor eax, eax
    mov edx, [ebp+8]
    xor ecx, ecx
    movss xmm0, [ebp+12]
    call XAnimCalc
    mov esp, ebp
    pop ebp
    ret
