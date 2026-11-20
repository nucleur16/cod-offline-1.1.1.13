BITS 32
SECTION .text
global TestEvalXAnim16
extern XAnimCalcParts
; cdecl test adapter to the retained register ABI. No runtime patching.
TestEvalXAnim16:
    push ebp
    mov ebp, esp
    and esp, -16
    sub esp, 16
    mov eax, [ebp+28]
    mov [esp], eax
    mov eax, [ebp+8]
    mov edx, [ebp+12]
    movss xmm0, [ebp+16]
    movss xmm1, [ebp+20]
    mov ecx, [ebp+24]
    call XAnimCalcParts
    mov esp, ebp
    pop ebp
    ret
%ifidn __OUTPUT_FORMAT__,elf
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%endif
