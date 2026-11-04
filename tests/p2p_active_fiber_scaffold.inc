/* Clock/lock doubles; active installers still call actual native owner API. */
typedef int SRWLOCK;
#define SRWLOCK_INIT 0
typedef struct { int64_t QuadPart; } LARGE_INTEGER;
static int require_clock_lock;
static void AcquireSRWLockExclusive(SRWLOCK *p){(void)p;++lock_depth;}
static void ReleaseSRWLockExclusive(SRWLOCK *p){(void)p;assert(lock_depth>0);--lock_depth;}
static BOOL QueryPerformanceFrequency(LARGE_INTEGER *p){p->QuadPart=1000;return TRUE;}
static BOOL QueryPerformanceCounter(LARGE_INTEGER *p){assert(!require_clock_lock||lock_depth);p->QuadPart=(int64_t)monotime.tv_sec*1000+monotime.tv_nsec/1000000;return TRUE;}
