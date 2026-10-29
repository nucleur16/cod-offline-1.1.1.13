/* Exercise the production console mailbox with concurrent producer/reclaimer
 * and a deliberately slow reader. No game/device/window is required. */
#include "../console_mailbox.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define THREADRET DWORD WINAPI
#define RETURN_THREAD return 0
static void pause_thread(void) { SwitchToThread(); }
#else
#include <pthread.h>
#include <sched.h>
#define THREADRET void *
#define RETURN_THREAD return NULL
static void pause_thread(void) { sched_yield(); }
#endif
static ConRasterFrame slots[2];
static volatile ConFrameState done;
static THREADRET produce(void *unused)
{
    (void)unused;
    for (uint32_t seq = 1; seq <= 100000; seq++) {
        ConRasterFrame *f;
        while (!(f = ConFrameAcquire(slots))) pause_thread();
        size_t count = 16 + seq % 512;
        if (count * 4 > f->capacity) {
            f->pixels = realloc(f->pixels, count * 4);
            assert(f->pixels); f->capacity = count * 4;
        }
        f->serial = seq; f->w = (int)count; f->h = 1; f->used = 1;
        for (size_t i = 0; i < count; i++) f->pixels[i] = seq ^ (uint32_t)i;
        ConFramePublish(f);
        if (!(seq % 8)) pause_thread();
    }
    ConFrameSet(&done, 1);
    RETURN_THREAD;
}
int main(void)
{
    /* A READING frame cannot be reclaimed, even if both slots are occupied. */
    ConRasterFrame *a = ConFrameAcquire(slots), *b = ConFrameAcquire(slots);
    assert(a && b && a != b);
    ConFramePublish(a); ConFramePublish(b);
    assert(ConFrameRead(a) && ConFrameRead(b));
    assert(!ConFrameAcquire(slots));
    ConFrameRelease(a); ConFrameRelease(b);
#ifdef _WIN32
    HANDLE producer = CreateThread(NULL, 0, produce, NULL, 0, NULL); assert(producer);
#else
    pthread_t producer; assert(!pthread_create(&producer, NULL, produce, NULL));
#endif
    unsigned consumed = 0;
    for (;;) {
        int found = 0;
        for (int i = 0; i < 2; i++) {
            ConRasterFrame *f = slots + i;
            if (!ConFrameRead(f)) continue;
            found = 1; uint32_t seq = f->serial; int width = f->w;
            assert(f->h == 1 && f->used == 1 && width >= 16 && width <= 527);
            pause_thread(); /* The producer must not realloc/modify held pixels. */
            for (int j = 0; j < width; j++) assert(f->pixels[j] == (seq ^ (uint32_t)j));
            assert(f->serial == seq && f->w == width);
            ConFrameRelease(f); consumed++;
        }
        if (!found && ConFrameCas(&done, 0, 0)) break;
        if (!found) pause_thread();
    }
#ifdef _WIN32
    WaitForSingleObject(producer, INFINITE); CloseHandle(producer);
#else
    assert(!pthread_join(producer, NULL));
#endif
    for (int i = 0; i < 2; i++) free(slots[i].pixels);
    assert(consumed > 0);
    printf("production mailbox PASS: 100000 writes, %u stable reads, no occupied-slot overwrite\n", consumed);
    return 0;
}
