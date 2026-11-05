/* Execute the exact x86 bulk-count hook on a private page. No C1 process. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
typedef unsigned char BYTE;
typedef uintptr_t DWORD_PTR;
typedef unsigned long DWORD;
typedef size_t SIZE_T;
typedef int BOOL;
#define TRUE 1
#define FALSE 0

static BYTE *fake_entry, *fake_empty, *fake_exit;
static BYTE *fake_indirect, *fake_indirect_skip;
static BYTE *fake_post, *fake_post_skip;
static int fail_entry_write;
static int fail_indirect_write;
static int fail_post_write;
static BYTE region[0x4fc];
static BYTE *region_ptr = region;
static BYTE *indirect_state;
static BYTE *indirect_frame_ptr;
static uint32_t indirect_frame[8];
static BYTE *post_state;
static unsigned overflow;
static unsigned *overflow_ptr = &overflow;
#define BULK_COUNT_RUN_VA ((uintptr_t)fake_entry)
#define BULK_COUNT_EMPTY_VA ((uintptr_t)fake_empty)
#define BULK_COUNT_EXIT_VA ((uintptr_t)fake_exit)
#define BULK_INDIRECT_COUNT_RUN_VA ((uintptr_t)fake_indirect)
#define BULK_INDIRECT_COUNT_SKIP_VA ((uintptr_t)fake_indirect_skip)
#define BULK_POSTREAD_GUARD_VA ((uintptr_t)fake_post)
#define BULK_POSTREAD_SKIP_VA ((uintptr_t)fake_post_skip)
static BOOL Readable(const void *p, SIZE_T n) { return p && (n == 6 || n == 7); }
static void WriteCode(void *p, const void *src, SIZE_T n)
{
    if (((fail_entry_write && p == fake_entry) ||
         (fail_indirect_write && p == fake_indirect) ||
         (fail_post_write && p == fake_post)) && ((const BYTE *)src)[0] == 0xe9) {
        memcpy(p, src, 3);
        return;
    }
    memcpy(p, src, n);
}
static void Log(const char *unused, ...) { (void)unused; }
#include "../bulk_count_guard.c"

static int __attribute__((noinline)) Run(void)
{
    int result;
    __asm__ volatile(
        "pushl %%ebx\n\t"
        "pushl %%esi\n\t"
        "pushl %%edi\n\t"
        "movl %1, %%edi\n\t"
        "movl %2, %%esi\n\t"
        "xorl %%ebx, %%ebx\n\t"
        "call *%3\n\t"
        "popl %%edi\n\t"
        "popl %%esi\n\t"
        "popl %%ebx\n\t"
        : "=a"(result)
        : "m"(region_ptr), "m"(overflow_ptr), "m"(fake_entry)
        : "ecx", "edx", "memory", "cc");
    return result;
}

static int __attribute__((noinline)) RunIndirect(void)
{
    int result;
    __asm__ volatile(
        "pushl %%ebp\n\t"
        "pushl %%esi\n\t"
        "movl %1, %%esi\n\t"
        "movl %2, %%ebp\n\t"
        "call *%3\n\t"
        "popl %%esi\n\t"
        "popl %%ebp\n\t"
        : "=a"(result)
        : "m"(indirect_state), "m"(indirect_frame_ptr), "m"(fake_indirect)
        : "ecx", "edx", "memory", "cc");
    return result;
}

static int __attribute__((noinline)) RunPost(void)
{
    int result;
    __asm__ volatile(
        "pushl %%ebp\n\t"
        "pushl %%ebx\n\t"
        "pushl %%edx\n\t"
        "movl %1, %%edx\n\t"
        "movl %2, %%ebx\n\t"
        "movl %3, %%ebp\n\t"
        "call *%4\n\t"
        "popl %%edx\n\t"
        "popl %%ebx\n\t"
        "popl %%ebp\n\t"
        : "=a"(result)
        : "m"(region_ptr), "m"(post_state), "m"(indirect_frame_ptr), "m"(fake_post)
        : "ecx", "esi", "edi", "memory", "cc");
    return result;
}

int main(void)
{
    static const BYTE expected[7] = {0x39,0x1f,0x7e,0x1d,0x8d,0x4f,0x3c};
    static const BYTE indirect_expected[7] = {0xc7,0x45,0xf0,0,0,0,0};
    static const BYTE post_expected[6] = {0x83,0x3a,0,0x89,0x55,0xf0};
    BYTE *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(page != MAP_FAILED);
    indirect_state = mmap(NULL, 0x7234d4 + 0x4fc, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(indirect_state != MAP_FAILED);
    post_state = mmap(NULL, 0xbeb18 + 4, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(post_state != MAP_FAILED);
    indirect_frame_ptr = (BYTE *)&indirect_frame[4];
    fake_entry = page + 64;
    fake_empty = page + 128;
    fake_exit = page + 192;
    fake_indirect = page + 256;
    fake_indirect_skip = page + 320;
    fake_post = page + 384;
    fake_post_skip = page + 480;
    memcpy(fake_entry, expected, sizeof(expected));
    memcpy(fake_indirect, indirect_expected, sizeof(indirect_expected));
    memcpy(fake_post, post_expected, sizeof(post_expected));
    fake_entry[0] ^= 1;
    assert(!InstallBulkCountGuard());
    fake_entry[0] ^= 1;
    fake_indirect[0] ^= 1;
    fake_post[0] ^= 1;
    assert(!InstallBulkCountGuard());
    fake_post[0] ^= 1;
    assert(!InstallBulkCountGuard());
    fake_indirect[0] ^= 1;
    fake_entry[7] = 0x89; fake_entry[8] = 0xc8; fake_entry[9] = 0xc3;
    fake_empty[0] = 0x31; fake_empty[1] = 0xc0; fake_empty[2] = 0xc3;
    fake_exit[0] = 0xb8; fake_exit[1] = 2; fake_exit[2] = 0;
    fake_exit[3] = 0; fake_exit[4] = 0; fake_exit[5] = 0xc3;
    /* Recreate the displaced count-vs-zero JLE at the resume address. */
    fake_indirect[7] = 0x0f; fake_indirect[8] = 0x8e;
    *(int32_t *)(fake_indirect + 9) = (int32_t)(fake_indirect_skip - (fake_indirect + 13));
    fake_indirect[13] = 0xb8; fake_indirect[14] = 3;
    fake_indirect[15] = 0; fake_indirect[16] = 0; fake_indirect[17] = 0;
    fake_indirect[18] = 0xc3;
    fake_indirect_skip[0] = 0x31; fake_indirect_skip[1] = 0xc0;
    fake_indirect_skip[2] = 0xc3;
    /* Resume through the original MOV [EBP-8],0 and its count-vs-zero JLE. */
    fake_post[6] = 0xc7; fake_post[7] = 0x45; fake_post[8] = 0xf8;
    memset(fake_post + 9, 0, 4);
    fake_post[13] = 0x0f; fake_post[14] = 0x8e;
    *(int32_t *)(fake_post + 15) = (int32_t)(fake_post_skip - (fake_post + 19));
    fake_post[19] = 0xb8; fake_post[20] = 4;
    memset(fake_post + 21, 0, 3); fake_post[24] = 0xc3;
    fake_post_skip[0] = 0x31; fake_post_skip[1] = 0xc0;
    fake_post_skip[2] = 0xc3;
    fail_entry_write = 1;
    assert(!InstallBulkCountGuard());
    assert(!memcmp(fake_entry, expected, sizeof(expected)));
    fail_entry_write = 0;
    fail_indirect_write = 1;
    assert(!InstallBulkCountGuard());
    assert(!memcmp(fake_entry, expected, sizeof(expected)));
    fail_indirect_write = 0;
    fail_post_write = 1;
    assert(!InstallBulkCountGuard());
    assert(!memcmp(fake_entry, expected, sizeof(expected)));
    assert(!memcmp(fake_indirect, indirect_expected, sizeof(indirect_expected)));
    assert(!memcmp(fake_post, post_expected, sizeof(post_expected)));
    fail_post_write = 0;
    assert(InstallBulkCountGuard());
    *(uint32_t *)region = 0;
    overflow = 0; assert(Run() == 0 && overflow == 0);
    *(uint32_t *)region = 19;
    overflow = 0; assert(Run() == (int)(uintptr_t)(region + 0x3c) && overflow == 0);
    *(uint32_t *)region = 20;
    overflow = 0; assert(Run() == 2 && overflow == 1);
    *(uint32_t *)region = UINT32_MAX;
    overflow = 0; assert(Run() == 0 && overflow == 0);
    *(uint32_t *)(indirect_state + 0x7234d4) = 0;
    assert(RunIndirect() == 0);
    *(uint32_t *)(indirect_state + 0x7234d4) = 19;
    assert(RunIndirect() == 3);
    *(uint32_t *)(indirect_state + 0x7234d4) = 20;
    assert(RunIndirect() == 0);
    assert(*(uint32_t *)(indirect_state + 0x7234d4) == 0);
    *(uint32_t *)(indirect_state + 0x7234d4) = UINT32_MAX;
    assert(RunIndirect() == 0);
    assert(*(uint32_t *)(indirect_state + 0x7234d4) == UINT32_MAX);
    assert(indirect_frame[0] == 0);
    *(uint32_t *)region = 0;
    assert(RunPost() == 0);
    for (unsigned i = 0; i < 19; ++i)
        *(uint32_t *)(region + 0x3c + i * 0x40) = i;
    *(uint32_t *)region = 19;
    assert(RunPost() == 4);
    *(uint32_t *)region = 20;
    *(uint32_t *)(post_state + 0xbeb18) = 20;
    assert(RunPost() == 0 && *(uint32_t *)(post_state + 0xbeb18) == 0);
    *(uint32_t *)region = 19;
    *(uint32_t *)(region + 0x3c + 18 * 0x40) = 19;
    *(uint32_t *)(post_state + 0xbeb18) = 19;
    assert(RunPost() == 0 && *(uint32_t *)(post_state + 0xbeb18) == 0);
    *(uint32_t *)region = UINT32_MAX;
    *(uint32_t *)(post_state + 0xbeb18) = UINT32_MAX;
    assert(RunPost() == 0 && *(uint32_t *)(post_state + 0xbeb18) == 0);
    assert(!munmap(post_state, 0xbeb18 + 4));
    assert(!munmap(indirect_state, 0x7234d4 + 0x4fc));
    assert(!munmap(page, 4096));
    return 0;
}
