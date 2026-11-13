/* Console CPU frames only. D3D objects never cross this mailbox.
 * Full-fence ownership transitions publish all metadata and pixel writes. */
#ifndef K33_CONSOLE_MAILBOX_H
#define K33_CONSOLE_MAILBOX_H
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
typedef LONG ConFrameState;
static inline ConFrameState ConFrameCas(volatile ConFrameState *p, ConFrameState value, ConFrameState old)
{ return InterlockedCompareExchange(p, value, old); }
static inline void ConFrameSet(volatile ConFrameState *p, ConFrameState value)
{ InterlockedExchange(p, value); }
#else
typedef int32_t ConFrameState;
static inline ConFrameState ConFrameCas(volatile ConFrameState *p, ConFrameState value, ConFrameState old)
{ __atomic_compare_exchange_n(p, &old, value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return old; }
static inline void ConFrameSet(volatile ConFrameState *p, ConFrameState value)
{ __atomic_store_n(p, value, __ATOMIC_SEQ_CST); }
#endif
enum { CON_FREE, CON_WRITING, CON_READY, CON_READING };
typedef struct {
    volatile ConFrameState state;
    uint32_t *pixels;
    size_t capacity;
    int w, h, used;
    uint32_t serial;
} ConRasterFrame;
static inline ConRasterFrame *ConFrameAcquire(ConRasterFrame frames[2])
{
    for (int i = 0; i < 2; i++)
        if (ConFrameCas(&frames[i].state, CON_WRITING, CON_FREE) == CON_FREE) return &frames[i];
    for (int i = 0; i < 2; i++)
        if (ConFrameCas(&frames[i].state, CON_WRITING, CON_READY) == CON_READY) return &frames[i];
    return NULL;
}
static inline int ConFrameRead(ConRasterFrame *frame)
{ return ConFrameCas(&frame->state, CON_READING, CON_READY) == CON_READY; }
static inline void ConFrameRelease(ConRasterFrame *frame)
{ ConFrameSet(&frame->state, CON_FREE); }
static inline void ConFramePublish(ConRasterFrame *frame)
{ ConFrameSet(&frame->state, CON_READY); }
#endif
