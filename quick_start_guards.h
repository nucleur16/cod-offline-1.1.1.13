#pragma once
#include <stdint.h>
/* Exact shared native regions verified in shipping a5a7 and retained d821.
 * Guard every region before either patch; no private image is included. */
typedef struct { uint32_t address, size, fnv1a; } QsgGuard;
static const QsgGuard qsg_guards[] = {
    {0x006AEFB0u,63u,0x5492F85Cu}, /* AS_handler */
    {0x006B0350u,128u,0xFA00C7C4u}, /* AS_registration */
    {0x00C770B8u,8u,0x0C0608ADu}, /* AS_handler_vtable */
    {0x0071AD60u,1801u,0x24BCE39Au}, /* Quick_frame */
    {0x0071B437u,7u,0x777FB4EBu}, /* Quick_state5_commit */
    {0x00C7D66Cu,224u,0x19250A38u}, /* Main_room_vtable */
    {0x00C7DE84u,44u,0x232799F0u}, /* Quick_controller_vtable */
    {0x0070EA40u,7u,0xBFD077B9u}, /* Room_info_accessor */
    {0x0070E520u,273u,0x8086590Cu}, /* Native_StartGame_search */
    {0x0070B710u,140u,0x5F3F7889u}, /* Native_SearchServers */
    {0x0070F080u,25u,0x2025794Eu}, /* FinalVoting_setter */
    {0x006AFCF0u,85u,0x9D043866u}, /* FinalVoting_AS_handler */
    {0x00696500u,152u,0xACA9D387u}, /* Room_info_exporter */
    {0x0070FC40u,42u,0x87D2ACAEu}, /* mmbook_next_entry_consumer */
};
