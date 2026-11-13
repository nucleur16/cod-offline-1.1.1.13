/* C1 1.1.1.13 retained AS3 tracer: emit generic local arithmetic operands.
 * Keep both native entrypoints and all original abstract-type/refcount code.
 * The three added bytes fit each function's six bytes of alignment padding. */
#ifndef K33_GFX_LOCAL_OPERANDS_POLICY_H
#define K33_GFX_LOCAL_OPERANDS_POLICY_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define GFX_LOCAL_PRODUCER_SIZE 0x90u
static uint32_t GfxLocalOperandHash(const uint8_t *bytes, size_t size)
{
    uint32_t hash = UINT32_C(2166136261);
    while (size--) { hash ^= *bytes++; hash *= UINT32_C(16777619); }
    return hash;
}
static void GfxLocalOperandCall(uint8_t *output, uint32_t base, unsigned at, uint32_t target)
{
    uint32_t displacement = target - (base + at + 5u);
    output[at] = 0xe8;
    memcpy(output + at + 1u, &displacement, sizeof(displacement));
}
/* Source/output must be distinct 0x90-byte buffers. Refuse another build or
 * an already changed producer; the installer validates both before any write. */
static int GfxBuildLocalOperandProducer(uint32_t base, const uint8_t *source,
                                      size_t size, uint8_t *output)
{
    uint32_t expected;
    if (!source || !output || source == output || size != GFX_LOCAL_PRODUCER_SIZE) return 0;
    if (base == UINT32_C(0x009117a0)) expected = UINT32_C(0x4f98ee89);
    else if (base == UINT32_C(0x00911830)) expected = UINT32_C(0xc9003ee5);
    else return 0;
    if (GfxLocalOperandHash(source, size) != expected) return 0;
    memcpy(output, source, 0x0b);
    /* push dword [ebp+8]: register index already decoded by the caller. */
    output[0x0b] = 0xff; output[0x0c] = 0x75; output[0x0d] = 0x08;
    memcpy(output + 0x0e, source + 0x0b, 0x8a - 0x0b);
    memset(output + 0x8d, 0xcc, 3);
    /* All internal branches move with their destinations. Only direct calls
     * need rebasing. Use the original native (opcode, operand) word writer. */
    GfxLocalOperandCall(output, base, 0x13, UINT32_C(0x00907870));
    GfxLocalOperandCall(output, base, 0x4a, UINT32_C(0x00907e30));
    GfxLocalOperandCall(output, base, 0x82, UINT32_C(0x009058d0));
    return 1;
}
#endif
