/* Exact C1 generic inclocal/declocal producer correction. No VM instruction
 * semantics, opcode metadata, source movies or register ranges are changed.
 * Runs before the game entry, before any AS3 tracing can be in flight. */
#include "gfx_local_operands_policy.h"
static void PatchGfxLocalOperands(void)
{
    static const struct { uint32_t va, size, hash; } guards[] = {
        {0x009077d0u, 0x20u, 0xff82fbe7u}, /* original opcode-only writer */
        {0x00907870u, 0x20u, 0x86a57adau}, /* native opcode+operand writer */
        {0x00907e30u, 0x20u, 0x5d1b3c78u}, /* retained abstract-type assignment */
        {0x00937f4fu, 0x16u, 0x266f0f0cu}, /* caller decodes inclocal U30 */
        {0x00937f97u, 0x16u, 0x364d776cu}  /* caller decodes declocal U30 */
    };
    static const uint32_t entries[] = {0x009117a0u, 0x00911830u};
    uint8_t corrected[2][GFX_LOCAL_PRODUCER_SIZE];
    for (unsigned i = 0; i < sizeof(guards) / sizeof(guards[0]); ++i) {
        const uint8_t *source = (const uint8_t *)(DWORD_PTR)guards[i].va;
        if (!Readable(source, guards[i].size) ||
            GfxLocalOperandHash(source, guards[i].size) != guards[i].hash) {
            Log("AS3 local arithmetic: native guard differs; retained original producers");
            return;
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        const uint8_t *source = (const uint8_t *)(DWORD_PTR)entries[i];
        if (!Readable(source, GFX_LOCAL_PRODUCER_SIZE) ||
            !GfxBuildLocalOperandProducer(entries[i], source,
                                         GFX_LOCAL_PRODUCER_SIZE, corrected[i])) {
            Log("AS3 local arithmetic: producer differs; retained original producers");
            return;
        }
    }
    for (unsigned i = 0; i < 2; ++i)
        WriteCode((void *)(DWORD_PTR)entries[i], corrected[i], GFX_LOCAL_PRODUCER_SIZE);
    Log("AS3 local arithmetic: generic inclocal/declocal retain register operands");
}
