#ifndef ASYNC_CAPTURE_H
#define ASYNC_CAPTURE_H
#include <stddef.h>
#include <stdint.h>
/* OS-I/O-only worker. All arguments are copied before admission returns.
 * Never pass an engine pointer, FS handle, cvar or mutable world object. */
typedef uint64_t async_capture_t;
enum { AC_OK=0, AC_SATURATED=1, AC_IO=2,
       AC_SHUTDOWN=3, AC_COLLISION=4 };
#define AC_METADATA_BYTES 12288u
#define AC_TAIL_BYTES 128u
#define AC_PATH_BYTES 1024u
#define AC_REPORT_BYTES 65536u
typedef struct {
    unsigned tag;
    async_capture_t writer;
    int error;
    char path[AC_PATH_BYTES];
} async_capture_result_t;
static inline int AsyncCaptureResultMatches(const async_capture_result_t *r,
                                                  unsigned tag, async_capture_t writer)
{ return r && writer && r->tag == tag && r->writer == writer; }
async_capture_t AsyncCaptureBegin(const char *absolute_path,unsigned tag);
int AsyncCaptureAppend(async_capture_t,const void *,size_t,const void *,size_t);
int AsyncCaptureError(async_capture_t);
/* Close admission has reserved storage, independent of byte-queue saturation.
 * failure variants must say completed=false and carry an abnormal terminal. */
int AsyncCaptureClose(async_capture_t,
    const void *tail,size_t tail_size,const void *failed_tail,size_t failed_tail_size,
    const char *metadata,size_t metadata_size,const char *failed_metadata,size_t failed_metadata_size);
int AsyncCapturePoll(async_capture_result_t *);
/* Reserved, priority atomic-publication slots for immutable reward reports.
 * Status: 0 pending, 1 durably committed, -1 failed, -2 stale handle.
 * A failed job is retained until Release; retry identical bytes/name. */
async_capture_t AsyncCapturePublish(const char *absolute_path,
    const void *data,size_t size,int replace_existing);
int AsyncCapturePublishStatus(async_capture_t,int *error);
void AsyncCapturePublishRelease(async_capture_t);
/* Reject late admission atomically; drain accepted jobs. Timeout is explicit,
 * never an excuse to publish a partial capture as complete. Main/exit only. */
int AsyncCaptureShutdown(unsigned timeout_ms);
size_t AsyncCaptureQueuedBytes(void);
#endif
