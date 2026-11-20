/* Bounded immutable byte transport; no engine API is called by its worker.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "async_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
static SRWLOCK mutex=SRWLOCK_INIT;
static CONDITION_VARIABLE condition=CONDITION_VARIABLE_INIT;
static HANDLE thread;
#define LOCK() AcquireSRWLockExclusive(&mutex)
#define UNLOCK() ReleaseSRWLockExclusive(&mutex)
#define SIGNAL() WakeAllConditionVariable(&condition)
#define WAIT() SleepConditionVariableSRW(&condition,&mutex,INFINITE,0)
#define OPEN _open
#define CLOSE _close
#define FDOPEN _fdopen
#define FILENO _fileno
#define SYNC _commit
#define UNLINK _unlink
#define GETPID GetCurrentProcessId
#define BINARY _O_BINARY
static int mkdir_one(const char *p){return _mkdir(p);}
#else
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static pthread_t thread;
#define LOCK() pthread_mutex_lock(&mutex)
#define UNLOCK() pthread_mutex_unlock(&mutex)
#define SIGNAL() pthread_cond_broadcast(&condition)
#define WAIT() pthread_cond_wait(&condition,&mutex)
#define OPEN open
#define CLOSE close
#define FDOPEN fdopen
#define FILENO fileno
#define SYNC fsync
#define UNLINK unlink
#define GETPID getpid
#define BINARY 0
static int mkdir_one(const char *p){return mkdir(p,0777);}
#endif
#ifndef ASYNC_CAPTURE_QUEUE_BYTES
#define ASYNC_CAPTURE_QUEUE_BYTES (16u*1024u*1024u)
#endif
#ifndef ASYNC_CAPTURE_QUEUE_JOBS
#define ASYNC_CAPTURE_QUEUE_JOBS 1024u
#endif
#define STREAMS 128u
#define PUBLICATIONS 4u
#define PUBLICATION_BIT ((uint64_t)1<<63)
#ifdef ASYNC_CAPTURE_TEST
/* Fixture-only boundary fault injection; omitted from normal builds. */
extern int AsyncCaptureTestIO(const char *operation);
#define IO_GATE(op) AsyncCaptureTestIO(op)
#else
#define IO_GATE(op) 1
#endif

typedef struct {
    unsigned generation,tag,pending;
    int used,closing,closed,attempted,finalizing,error,created;
    char path[AC_PATH_BYTES];
    FILE *file; /* exclusively worker-owned */
    size_t tail_size,failed_tail_size,metadata_size,failed_metadata_size;
    unsigned char tail[AC_TAIL_BYTES],failed_tail[AC_TAIL_BYTES];
    char metadata[AC_METADATA_BYTES],failed_metadata[AC_METADATA_BYTES];
} stream_t;
typedef struct job_s {
    struct job_s *next;
    unsigned index,generation;
    size_t size,charge;
    unsigned char data[];
} job_t;
typedef struct {
    unsigned generation;
    int used,working,done,error,replace;
    char path[AC_PATH_BYTES];
    size_t size;
    unsigned char data[AC_REPORT_BYTES];
} publication_t;
#include "sys_ondemand.h"
SH1X_ONDEMAND(, stream_t, capture_streams, [STREAMS]); /* sys_ondemand.h: allocated when the server starts */
#define streams (*sh1x_od_capture_streams)
SH1X_ONDEMAND(, publication_t, capture_publications, [PUBLICATIONS]); /* sys_ondemand.h: allocated when the server starts */
#define publications (*sh1x_od_capture_publications)
static job_t *head,*tail;
static size_t resident_bytes;
static unsigned resident_jobs,temporary_serial;
static int running,joinable,joining,accepting,stopping,sealed;

static uint64_t handle(unsigned i,unsigned generation,int publication)
{return ((uint64_t)generation<<8)|(i+1u)|(publication?PUBLICATION_BIT:0);}
static stream_t *find_stream(uint64_t h)
{
    unsigned i=(unsigned)(h&255u);
    if(!i || i>STREAMS || (h&PUBLICATION_BIT))return NULL;
    stream_t *s=&streams[i-1];
    return s->used && s->generation==(unsigned)(h>>8)?s:NULL;
}
static publication_t *find_publication(uint64_t h)
{
    unsigned i=(unsigned)(h&255u);
    if(!i || i>PUBLICATIONS || !(h&PUBLICATION_BIT))return NULL;
    publication_t *p=&publications[i-1];
    return p->used && p->generation==(unsigned)((h&~PUBLICATION_BIT)>>8)?p:NULL;
}
static int absolute_path(const char *p)
{
    size_t n;
    if(!p || !(n=strlen(p)) || n>=AC_PATH_BYTES)return 0;
    return p[0]=='/' || (n>2 && p[1]==':' && (p[2]=='/' || p[2]=='\\'));
}
static int parent_directories(const char *path)
{
    char copy[AC_PATH_BYTES];size_t i;
    strcpy(copy,path);
    for(i=1;copy[i];++i)if(copy[i]=='/' || copy[i]=='\\') {
        char c=copy[i];copy[i]=0;
        if(i!=2 || copy[1]!=':')
            if(mkdir_one(copy) && errno!=EEXIST)return 0;
        copy[i]=c;
    }
    return 1;
}
static int sync_parent(const char *path)
{
#ifdef _WIN32
    (void)path;return 1; /* committed files + MoveFileEx WRITE_THROUGH below */
#else
    char parent[AC_PATH_BYTES];char *slash;int fd,ok;
    strcpy(parent,path);slash=strrchr(parent,'/');if(!slash)return 0;
    if(slash==parent)slash[1]=0;else *slash=0;
    fd=OPEN(parent,O_RDONLY|O_DIRECTORY);if(fd<0)return 0;
    ok=SYNC(fd)==0;CLOSE(fd);return ok;
#endif
}
static int write_all(FILE *f,const void *data,size_t size)
{
    const unsigned char *p=data;
    while(size) {
        size_t n;
        if(!IO_GATE("write"))return 0;
        n=fwrite(p,1,size,f);
        if(!n)return 0;
        p+=n;size-=n;
    }
    return !ferror(f);
}
static int flush_file(FILE *f)
{return IO_GATE("flush") && fflush(f)==0 && IO_GATE("sync") && SYNC(FILENO(f))==0 && !ferror(f);}
static int same_file(const char *path,const unsigned char *data,size_t size)
{
    unsigned char block[4096];size_t at=0,n;FILE *f;int ok;
#ifdef _WIN32
    /* _commit uses FlushFileBuffers, which requires a writable Win32 handle.
     * r+b preserves existing bytes while making identical publication retries
     * durably acknowledgeable; no create/truncate mode is used. */
    f=fopen(path,"r+b");
#else
    f=fopen(path,"rb");
#endif
    if(!f)return 0;
    while(at<size) {
        n=size-at;if(n>sizeof(block))n=sizeof(block);
        if(fread(block,1,n,f)!=n || memcmp(block,data+at,n)){fclose(f);return 0;}
        at+=n;
    }
    ok=fgetc(f)==EOF && !ferror(f);
    if(ok)ok=IO_GATE("sync") && SYNC(FILENO(f))==0;
    if(fclose(f))ok=0;
    return ok && sync_parent(path);
}
static int atomic_publish(const char *path,const unsigned char *data,size_t size,int replace)
{
    char temporary[AC_PATH_BYTES+64];int fd=-1,ok=0;unsigned attempt;
    FILE *f=NULL;struct stat st;
    if(!IO_GATE("publish") || !parent_directories(path))return 0;
    if(!stat(path,&st) && !replace)return same_file(path,data,size);
    for(attempt=0;attempt<8;++attempt) {
        snprintf(temporary,sizeof(temporary),"%s.tmp.%lu.%u",path,(unsigned long)GETPID(),++temporary_serial);
        fd=OPEN(temporary,O_CREAT|O_EXCL|O_WRONLY|BINARY,0666);
        if(fd>=0)break;
        if(errno!=EEXIST)return 0;
    }
    if(fd<0)return 0;
    f=FDOPEN(fd,"wb");if(!f){CLOSE(fd);UNLINK(temporary);return 0;}
    ok=write_all(f,data,size) && flush_file(f);
    if(fclose(f))ok=0;
    if(!ok){UNLINK(temporary);return 0;}
    if(!IO_GATE("rename")){UNLINK(temporary);return 0;}
#ifdef _WIN32
    ok=MoveFileExA(temporary,path,MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0))!=0;
#else
    if(replace)ok=rename(temporary,path)==0;
    else {ok=link(temporary,path)==0;UNLINK(temporary);}
#endif
    if(!ok){UNLINK(temporary);return !replace && same_file(path,data,size);}
    /* A false acknowledgement never permits report reset. Stable retry bytes
     * and identity make the post-publication uncertainty idempotent. */
    if(IO_GATE("dirsync") && sync_parent(path))return 1;
    if(!replace){UNLINK(path);(void)sync_parent(path);}
    return 0;
}
static void fail_stream(stream_t *s,int error)
{LOCK();if(!s->error)s->error=error;UNLOCK();}
static int stream_error(stream_t *s)
{int error;LOCK();error=s->error;UNLOCK();return error;}
static void metadata_path(const stream_t *s,char out[AC_PATH_BYTES])
{strcpy(out,s->path);strcpy(strrchr(out,'.'),".json");}
static void open_stream(stream_t *s)
{
    char metadata[AC_PATH_BYTES];struct stat st;int fd;
    metadata_path(s,metadata);
    if(!parent_directories(s->path) || !IO_GATE("open")){fail_stream(s,AC_IO);goto done;}
    if(!stat(metadata,&st)){fail_stream(s,AC_COLLISION);goto done;}
    if(errno!=ENOENT){fail_stream(s,AC_IO);goto done;}
    fd=OPEN(s->path,O_CREAT|O_EXCL|O_WRONLY|BINARY,0666);
    if(fd<0){fail_stream(s,errno==EEXIST?AC_COLLISION:AC_IO);goto done;}
    s->created=1;
    s->file=FDOPEN(fd,"wb");
    if(!s->file){CLOSE(fd);fail_stream(s,AC_IO);}
done:
    LOCK();s->attempted=1;SIGNAL();UNLOCK();
}
static void close_stream(stream_t *s)
{
    int error=stream_error(s),ok=1;char metadata[AC_PATH_BYTES];
    if(!s->attempted)open_stream(s);
    error=stream_error(s);
    if(s->file) {
        const unsigned char *last=error?s->failed_tail:s->tail;
        size_t n=error?s->failed_tail_size:s->tail_size;
        /* After an actual I/O error, avoid retrying an ambiguous partial tail. */
        if(error!=AC_IO && n && !write_all(s->file,last,n))ok=0;
        if(!flush_file(s->file))ok=0;
        if(fclose(s->file))ok=0;
        s->file=NULL;
        if(!ok)fail_stream(s,AC_IO);
        if(!sync_parent(s->path))fail_stream(s,AC_IO);
    }else fail_stream(s,error?error:AC_IO);
    error=stream_error(s);metadata_path(s,metadata);
    /* Never overwrite a pre-existing capture/sidecar after a collision. */
    if(s->created && error!=AC_COLLISION) {
        const char *text=error?s->failed_metadata:s->metadata;
        size_t n=error?s->failed_metadata_size:s->metadata_size;
        if(!n || !atomic_publish(metadata,(const unsigned char *)text,n,0))fail_stream(s,AC_IO);
    }
    LOCK();s->closed=1;SIGNAL();UNLOCK();
}
#ifdef _WIN32
static DWORD WINAPI worker(void *unused)
#else
static void *worker(void *unused)
#endif
{
    (void)unused;
    for(;;) {
        job_t *job=NULL;stream_t *s=NULL;publication_t *p=NULL;
        unsigned i;int action=0;
        LOCK();
        for(i=0;i<PUBLICATIONS;++i)if(publications[i].used && !publications[i].working && !publications[i].done) {
            p=&publications[i];p->working=1;action=3;break;
        }
        /* A ready close has no remaining own records. Prioritize it over
         * unrelated append traffic so its durable sidecar cannot starve. */
        if(!action)for(i=0;i<STREAMS;++i)if(streams[i].used && !streams[i].closed &&
            streams[i].closing && !streams[i].pending) {
            s=&streams[i];s->finalizing=1;action=2;break;
        }
        if(!action && head) {job=head;head=job->next;if(!head)tail=NULL;s=&streams[job->index];action=1;}
        if(!action)for(i=0;i<STREAMS;++i)if(streams[i].used && !streams[i].closed) {
            if(streams[i].closing && !streams[i].pending) {s=&streams[i];s->finalizing=1;action=2;break;}
            if(!streams[i].attempted) {s=&streams[i];action=4;break;}
        }
        if(!action) {
            int unfinished=0;
            for(i=0;i<STREAMS;++i)if(streams[i].used && !streams[i].closed)unfinished=1;
            if(stopping && !unfinished) {running=0;SIGNAL();UNLOCK();break;}
            WAIT();UNLOCK();continue;
        }
        UNLOCK();
        if(action==3) {
            int ok=atomic_publish(p->path,p->data,p->size,p->replace);
            LOCK();p->error=ok?0:AC_IO;p->done=1;SIGNAL();UNLOCK();
        }else if(action==4)open_stream(s);
        else if(action==2){if(!IO_GATE("close"))fail_stream(s,AC_IO);close_stream(s);}
        else {
            if(!s->attempted)open_stream(s);
            if(!stream_error(s) && (!s->file || !write_all(s->file,job->data,job->size)))fail_stream(s,AC_IO);
            LOCK();--s->pending;resident_bytes-=job->charge;--resident_jobs;SIGNAL();UNLOCK();free(job);
        }
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
static int start_locked(void)
{
    if(sealed)return 0;
    if(running)return accepting;
    if(joinable || !IO_GATE("start"))return 0;
    stopping=0;accepting=1;running=1;
#ifdef _WIN32
    thread=CreateThread(NULL,0,worker,NULL,0,NULL);
    if(!thread){running=accepting=0;return 0;}
#else
    if(pthread_create(&thread,NULL,worker,NULL)){running=accepting=0;return 0;}
#endif
    joinable=1;return 1;
}
async_capture_t AsyncCaptureBegin(const char *path,unsigned tag)
{
    unsigned i,g;uint64_t h=0;size_t n;
    if(!absolute_path(path) || (n=strlen(path))<6 || strcmp(path+n-6,".c1cap"))return 0;
    LOCK();
    if(start_locked())for(i=0;i<STREAMS;++i)if(!streams[i].used) {
        stream_t *s=&streams[i];g=s->generation+1;if(!g)g=1;
        memset(s,0,sizeof(*s));s->used=1;s->generation=g;s->tag=tag;strcpy(s->path,path);
        h=handle(i,g,0);SIGNAL();break;
    }
    UNLOCK();return h;
}
int AsyncCaptureAppend(uint64_t h,const void *first,size_t a,const void *second,size_t b)
{
    stream_t *s;job_t *job;size_t charge;int ok=0;
    if((a&&!first)||(b&&!second)||a>ASYNC_CAPTURE_QUEUE_BYTES || b>ASYNC_CAPTURE_QUEUE_BYTES-a)return 0;
    charge=sizeof(*job)+a+b;
    LOCK();s=find_stream(h);
    if(!s || !accepting || s->closing || s->error)goto done;
    if(charge>ASYNC_CAPTURE_QUEUE_BYTES-resident_bytes || resident_jobs>=ASYNC_CAPTURE_QUEUE_JOBS) {
        s->error=AC_SATURATED;goto done;
    }
    job=malloc(charge);if(!job){s->error=AC_SATURATED;goto done;}
    job->next=NULL;job->index=(unsigned)(s-streams);job->generation=s->generation;job->size=a+b;job->charge=charge;
    if(a)memcpy(job->data,first,a);
    if(b)memcpy(job->data+a,second,b);
    if(tail)tail->next=job;else head=job;tail=job;++s->pending;
    resident_bytes+=charge;++resident_jobs;ok=1;SIGNAL();
done:UNLOCK();return ok;
}
int AsyncCaptureError(uint64_t h)
{int error;stream_t *s;LOCK();s=find_stream(h);error=s?s->error:AC_SHUTDOWN;UNLOCK();return error;}
int AsyncCaptureClose(uint64_t h,const void *last,size_t a,const void *bad,size_t b,
    const char *metadata,size_t c,const char *failed,size_t d)
{
    int ok=0;stream_t *s;
    if(a>AC_TAIL_BYTES || b>AC_TAIL_BYTES || c>AC_METADATA_BYTES || d>AC_METADATA_BYTES ||
       (a&&!last)||(b&&!bad)||(c&&!metadata)||(d&&!failed))return 0;
    LOCK();s=find_stream(h);
    if(s && !s->closing) {
        if(a)memcpy(s->tail,last,a);
        if(b)memcpy(s->failed_tail,bad,b);
        if(c)memcpy(s->metadata,metadata,c);
        if(d)memcpy(s->failed_metadata,failed,d);
        s->tail_size=a;s->failed_tail_size=b;s->metadata_size=c;s->failed_metadata_size=d;
        s->closing=1;ok=1;SIGNAL();
    }
    UNLOCK();return ok;
}
int AsyncCapturePoll(async_capture_result_t *result)
{
    unsigned i,g;int found=0;if(!result)return 0;
    LOCK();for(i=0;i<STREAMS;++i)if(streams[i].used && streams[i].closed) {
        stream_t *s=&streams[i];result->tag=s->tag;result->writer=handle(i,s->generation,0);result->error=s->error;strcpy(result->path,s->path);
        g=s->generation;memset(s,0,sizeof(*s));s->generation=g;found=1;break;
    }UNLOCK();return found;
}
async_capture_t AsyncCapturePublish(const char *path,const void *data,size_t size,int replace)
{
    unsigned i,g;uint64_t h=0;
    if(!absolute_path(path) || size>AC_REPORT_BYTES || (size&&!data))return 0;
    LOCK();if(start_locked())for(i=0;i<PUBLICATIONS;++i)if(!publications[i].used) {
        publication_t *p=&publications[i];g=p->generation+1;if(!g)g=1;
        memset(p,0,sizeof(*p));p->generation=g;p->used=1;p->replace=replace;p->size=size;
        strcpy(p->path,path);if(size)memcpy(p->data,data,size);h=handle(i,g,1);SIGNAL();break;
    }UNLOCK();return h;
}
int AsyncCapturePublishStatus(uint64_t h,int *error)
{
    int status;publication_t *p;LOCK();p=find_publication(h);
    status=!p?-2:!p->done?0:p->error?-1:1;if(error)*error=p?p->error:AC_SHUTDOWN;
    UNLOCK();return status;
}
void AsyncCapturePublishRelease(uint64_t h)
{
    publication_t *p;unsigned g;LOCK();p=find_publication(h);
    if(p && p->done){g=p->generation;memset(p,0,sizeof(*p));p->generation=g;}
    UNLOCK();
}
size_t AsyncCaptureQueuedBytes(void)
{size_t n;LOCK();n=resident_bytes;UNLOCK();return n;}
int AsyncCaptureShutdown(unsigned timeout_ms)
{
    unsigned i;int done,join=0;
#ifdef _WIN32
    ULONGLONG deadline=GetTickCount64()+timeout_ms;
#else
    struct timespec deadline;clock_gettime(CLOCK_REALTIME,&deadline);
    deadline.tv_sec+=timeout_ms/1000;deadline.tv_nsec+=(long)(timeout_ms%1000)*1000000;
    if(deadline.tv_nsec>=1000000000){++deadline.tv_sec;deadline.tv_nsec-=1000000000;}
#endif
    LOCK();sealed=1;accepting=0;stopping=1;
    for(i=0;i<STREAMS;++i)if(streams[i].used && !streams[i].closing) {
        streams[i].closing=1;if(!streams[i].error)streams[i].error=AC_SHUTDOWN;
    }
    SIGNAL();
    while(running) {
#ifdef _WIN32
        ULONGLONG now=GetTickCount64();if(now>=deadline)break;
        if(!SleepConditionVariableSRW(&condition,&mutex,(DWORD)(deadline-now),0) && GetLastError()==ERROR_TIMEOUT)break;
#else
        if(pthread_cond_timedwait(&condition,&mutex,&deadline)==ETIMEDOUT)break;
#endif
    }
    done=!running;if(done && joinable && !joining){joining=1;join=1;}
    UNLOCK();
    if(join) {
#ifdef _WIN32
        WaitForSingleObject(thread,INFINITE);CloseHandle(thread);
#else
        pthread_join(thread,NULL);
#endif
        LOCK();joinable=0;joining=0;SIGNAL();UNLOCK();
    }
    return done;
}
