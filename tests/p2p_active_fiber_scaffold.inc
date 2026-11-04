/* Clock/lock doubles; active installers still call actual native owner API. */
typedef int SRWLOCK;
#define SRWLOCK_INIT 0
typedef struct { int64_t QuadPart; } LARGE_INTEGER;
static int require_clock_lock;
static int commit_clock_hook,commit_clock_calls;
static int (*g_piActivePrincipals)(const void *,unsigned,uint64_t);
static int (*g_piObservePrincipals)(const void *,unsigned,void *,unsigned);
static int sha256(const void *,size_t,unsigned char[32]);
#define P2PSSha256 sha256
static void AcquireSRWLockExclusive(SRWLOCK *p){(void)p;++lock_depth;}
static void ReleaseSRWLockExclusive(SRWLOCK *p){(void)p;assert(lock_depth>0);--lock_depth;}
static BOOL QueryPerformanceFrequency(LARGE_INTEGER *p){p->QuadPart=1000;return TRUE;}
static BOOL QueryPerformanceCounter(LARGE_INTEGER *p){assert(!require_clock_lock||lock_depth);if(commit_clock_hook&&++commit_clock_calls==2){clock_ms+=25000;monotime.tv_sec+=25;}p->QuadPart=(int64_t)monotime.tv_sec*1000+monotime.tv_nsec/1000000;return TRUE;}
