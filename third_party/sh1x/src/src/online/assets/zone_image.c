/* Zone images in read-only temporary-file views (feat/iw4-asset-layout, step 4: free the zone images).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * With freeZoneImages a zone image is a view of a temporary file (delete-on-close) instead of private memory.
 * Once its zone is replayed the view is made read-only and trimmed: its pages are file-backed, so they are not
 * private commit, the system writes them to the file and reads the same bytes back on the next touch, and the
 * address never changes, so every pointer into the image stays valid. Readers parse the serialized image exactly
 * as before; only the pages they touch cost memory, and ZoneImageTrim trims those again after a map load.
 *
 * This replaces the first design, which decommitted the pages and rebuilt them on touch from the replayed blocks
 * through the image map (a vectored exception handler). That made the bytes readers see depend on heap memory that
 * lives for the whole match: the replayed blocks (live asset memory the engine and the C1-layout readers use) and
 * the map's chunk list and block-0 shadow. Any later change to those, by a stray write, an in-place runtime field
 * or a reallocation, changed the image itself, and the next trim made it permanent: the live server on 2026-10-27
 * lost every rawfile and string-table name lookup in the middle of its fourth match on a reused mp_rust (every
 * later map load failed on mp/constConfigStrings_mp.csv). A file view has no such dependency: its bytes are the
 * file's, the view is read-only (a write faults at the writer instead of corrupting a later reader), and each
 * page's CRC-32 from the release is checked on demand (sh1x_imagecheck, and the soak tests).
 *
 * Elsewhere, and when the switch is off, an image is a plain allocation. */
#include "zone_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define PSAPI_VERSION 2
#include <psapi.h>
#include <malloc.h>
#endif

typedef struct {
    uint8_t *base;
    size_t size;
    const zone_t *zone;
    uint32_t *crc;             /* per 4 KB page: CRC-32 of the bytes as released */
    size_t pages;
} region_t;
static region_t regions[256];
static unsigned region_count;
static unsigned long long checked_pages,modified_pages;
static size_t released_bytes;
static zone_image_event_t events[16];
static unsigned event_count;
static int file_backed_images;
#ifdef _WIN32
static SRWLOCK lock=SRWLOCK_INIT;
#endif

static uint32_t crc_table[256];
static uint32_t crc32_of(const uint8_t *p,size_t n)
{
    uint32_t c=0xffffffffu; size_t i;
    if(!crc_table[1]) {
        unsigned k,b;
        for(k=0;k<256;++k) { uint32_t v=k; for(b=0;b<8;++b) v=v&1 ? 0xedb88320u^(v>>1) : v>>1; crc_table[k]=v; }
    }
    for(i=0;i<n;++i) c=crc_table[(c^p[i])&0xff]^(c>>8);
    return c^0xffffffffu;
}
/* The asset whose serialized bytes hold image offset at (directory entry: offset + 0x28 is where it starts). */
static void owner_of(const zone_t *z,size_t at,unsigned *type,unsigned *index)
{
    uint32_t k;
    *type=UINT32_MAX; *index=UINT32_MAX;
    if(!z || !z->data || z->table_offset>z->size || z->asset_count>(z->size-z->table_offset)/16) return;
    for(k=0;k<z->asset_count;++k) {
        const uint8_t *e=z->data+z->table_offset+16*k;
        uint32_t t=e[0]|(uint32_t)e[1]<<8|(uint32_t)e[2]<<16|(uint32_t)e[3]<<24;
        uint32_t pos=(e[4]|(uint32_t)e[5]<<8|(uint32_t)e[6]<<16|(uint32_t)e[7]<<24)+0x28;
        uint32_t ext=e[12]|(uint32_t)e[13]<<8|(uint32_t)e[14]<<16|(uint32_t)e[15]<<24;
        if(at>=pos && at<(size_t)pos+ext) { *type=t; *index=k; return; }
    }
}
static size_t page_length(const region_t *r,size_t index)
{
    size_t at=index*4096;
    return at+4096>r->size ? r->size-at : 4096;
}

void SetFileBackedImages(int on) { file_backed_images=on!=0; }

#ifdef _WIN32
/* A view of a new delete-on-close temporary file, or NULL; *file is the file's handle, which the caller keeps open
 * for as long as the view exists and closes only after unmapping it.
 *
 * MEASURED 2026-10-27, the cause of the live 17733 failure: the first version closed the handle right after
 * mapping, on the reasoning that the view keeps the file alive. It does not keep its data. Closing the last handle
 * of a delete-on-close file deletes it there and then; the mapping still works while its pages are resident, but
 * once the system has written a page out and later evicts it from the standby list, the page reads back as zeros.
 * Under memory pressure that happened during a match: a failed probe's read-only image views held zeros from their
 * first page (cdb dump; sh1x_imagecheck: 47747 of 73107 pages differed from their CRC at release), and with the
 * first design the file-backed replayed blocks zeroed the same way, so the images rebuilt from them lost their
 * rawfile and string-table names. Holding the handle keeps the file, and its data, until the view is gone; the
 * file is still deleted when the handle closes, or when the process dies. */
static void *temp_view(size_t size,const char *prefix,HANDLE *file_out)
{
    char dir[MAX_PATH],path[MAX_PATH]; HANDLE file,map; void *view=NULL;
    *file_out=INVALID_HANDLE_VALUE;
    if(!GetTempPathA(sizeof(dir),dir) || !GetTempFileNameA(dir,prefix,0,path)) return NULL;
    file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(file==INVALID_HANDLE_VALUE) { DeleteFileA(path); return NULL; }
    map=CreateFileMappingA(file,NULL,PAGE_READWRITE,(DWORD)((unsigned long long)size>>32),(DWORD)size,NULL);
    if(map) { view=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,size); CloseHandle(map); }
    if(!view) { CloseHandle(file); return NULL; }
    *file_out=file;
    return view;
}
/* Whether p is the base of one of this module's image views (they are freed by unmapping). */
typedef struct image_view_s { void *base; HANDLE file; struct image_view_s *next; } image_view_t;
static image_view_t *image_views;
#endif

void *ZoneImageAllocate(size_t size)
{
#ifdef _WIN32
    if(file_backed_images && size>=65536) {
        HANDLE file; void *view=temp_view(size,"szi",&file); image_view_t *v;
        if(view && (v=malloc(sizeof(*v)))) {
            AcquireSRWLockExclusive(&lock); v->base=view; v->file=file; v->next=image_views; image_views=v;
            ReleaseSRWLockExclusive(&lock);
            return view; /* a new file maps as zeros */
        }
        if(view) { UnmapViewOfFile(view); CloseHandle(file); }
    }
#endif
    return malloc(size ? size : 1);
}

void ZoneImageFree(void *data)
{
    unsigned i; int view=0;
    if(!data) return;
#ifdef _WIN32
    {
        image_view_t **link,*v=NULL;
        AcquireSRWLockExclusive(&lock);
        for(i=0;i<region_count;++i) if(regions[i].base==data) {
            released_bytes-=regions[i].size; free(regions[i].crc); regions[i]=regions[--region_count]; break;
        }
        for(link=&image_views;*link;link=&(*link)->next) if((*link)->base==data) { v=*link; *link=v->next; break; }
        ReleaseSRWLockExclusive(&lock);
        if(v) { UnmapViewOfFile(v->base); CloseHandle(v->file); free(v); view=1; }
    }
#else
    (void)i;
#endif
    if(!view) free(data);
}

int ZoneImageRelease(const zone_t *zone,const load_blocks_t *blocks)
{
#ifdef _WIN32
    region_t r={0}; image_view_t *v; size_t i; DWORD old;
    (void)blocks;
    if(!zone || !zone->data || !zone->size) return 0;
    AcquireSRWLockShared(&lock);
    for(v=image_views;v && v->base!=zone->data;v=v->next) {}
    ReleaseSRWLockShared(&lock);
    if(!v || region_count>=256) return 0; /* a plain allocation: nothing to release */
    r.base=zone->data; r.size=zone->size; r.zone=zone; r.pages=(zone->size+4095)/4096;
    if((r.crc=malloc(r.pages*sizeof(*r.crc)))) for(i=0;i<r.pages;++i) r.crc[i]=crc32_of(r.base+i*4096,page_length(&r,i));
    /* read-only from here: a write into an image is a bug, and faults where it happens */
    if(!VirtualProtect(r.base,r.size,PAGE_READONLY,&old)) { free(r.crc); return 0; }
    /* the bytes are in the file before the pages may leave memory (a failed write, e.g. a full temp volume, keeps
     * the image resident instead of losing it) */
    if(!FlushViewOfFile(r.base,0) || !FlushFileBuffers(v->file)) { free(r.crc); return 0; }
    VirtualUnlock(r.base,r.size); /* never locked: this takes the pages out of the working set */
    AcquireSRWLockExclusive(&lock);
    regions[region_count++]=r; released_bytes+=r.size;
    ReleaseSRWLockExclusive(&lock);
    return 1;
#else
    (void)zone; (void)blocks; return 0;
#endif
}

/* Every released image page against its CRC from the release; a page that differs is an integrity event. */
void ZoneImageCheck(unsigned *written,unsigned *drifted)
{
    unsigned bad=0;
#ifdef _WIN32
    unsigned i;
    AcquireSRWLockShared(&lock);
    for(i=0;i<region_count;++i) {
        const region_t *r=regions+i; size_t k;
        if(!r->crc) continue;
        for(k=0;k<r->pages;++k) {
            ++checked_pages;
            if(crc32_of(r->base+k*4096,page_length(r,k))==r->crc[k]) continue;
            ++bad; ++modified_pages;
            if(event_count<sizeof(events)/sizeof(events[0])) {
                zone_image_event_t *e=events+event_count++;
                e->kind=ZONE_IMAGE_WRITTEN; e->zone_size=r->size; e->offset=k*4096;
                owner_of(r->zone,k*4096,&e->asset_type,&e->asset_index);
            }
        }
        VirtualUnlock(r->base,r->size); /* the check read every page: trim them again */
    }
    ReleaseSRWLockShared(&lock);
#endif
    if(written) *written=bad;
    if(drifted) *drifted=0;
}
unsigned ZoneImageEvents(const zone_image_event_t **out)
{
    if(out) *out=events;
    return event_count;
}

void ZoneImageTrim(void)
{
#ifdef _WIN32
    unsigned i;
    BlockTrim();
    AcquireSRWLockShared(&lock);
    for(i=0;i<region_count;++i) VirtualUnlock(regions[i].base,regions[i].size);
    ReleaseSRWLockShared(&lock);
#endif
}

void ZoneImageStats(zone_image_stats_t *out)
{
    zone_image_stats_t s={0};
    if(!out) return;
    s.regions=region_count; s.released=released_bytes; s.checked_pages=checked_pages; s.modified_pages=modified_pages;
#ifdef _WIN32
    {
        /* resident image pages: the working-set share of the views */
        unsigned i;
        AcquireSRWLockShared(&lock);
        for(i=0;i<region_count;++i) {
            const region_t *r=regions+i; size_t k; PSAPI_WORKING_SET_EX_INFORMATION q;
            for(k=0;k<r->pages;++k) {
                q.VirtualAddress=r->base+k*4096;
                if(QueryWorkingSetEx(GetCurrentProcess(),&q,sizeof(q)) && q.VirtualAttributes.Valid) s.committed+=4096;
            }
        }
        ReleaseSRWLockShared(&lock);
    }
#endif
    *out=s;
}

/* ---- Replayed blocks backed by temporary files (fileBackedBlocks) ------------------------------------------
 * A large replay block (1 MB or more) is a view of a temporary file (delete-on-close) instead of private heap:
 * its pages are file-backed, so they are not private commit, and once trimmed (BlockTrim, after a zone's
 * replay and after each level load) the system writes them to the file and reads them back on the next touch.
 * The block's address never changes. Elsewhere, and below the threshold, blocks stay calloc'd. */
typedef struct block_region_s { void *base; size_t size; void *file; int unsafe; struct block_region_s *next; } block_region_t; /* file: the HANDLE */
static block_region_t *block_regions;
static int file_backed_blocks;
static size_t file_backed_bytes;
void SetFileBackedBlocks(int on) { file_backed_blocks=on!=0; }
size_t FileBackedBlockBytes(void) { return file_backed_bytes; }
void *BlockAllocate(size_t size)
{
#ifdef _WIN32
    if(file_backed_blocks && size>=(1u<<20)) {
        HANDLE file; void *view=temp_view(size,"sbk",&file); block_region_t *r;
        if(view && (r=malloc(sizeof(*r)))) {
            r->base=view; r->size=size; r->file=file; r->unsafe=0;
            AcquireSRWLockExclusive(&lock); r->next=block_regions; block_regions=r; file_backed_bytes+=size;
            ReleaseSRWLockExclusive(&lock);
            return view; /* a new file maps as zeros */
        }
        if(view) { UnmapViewOfFile(view); CloseHandle(file); }
    }
#endif
    return calloc(size ? size : 1,1);
}
void BlockFree(void *data)
{
#ifdef _WIN32
    block_region_t **link,*r=NULL;
    if(!data) return;
    AcquireSRWLockExclusive(&lock);
    for(link=&block_regions;*link;link=&(*link)->next) if((*link)->base==data) { r=*link; *link=r->next; break; }
    if(r) file_backed_bytes-=r->size;
    ReleaseSRWLockExclusive(&lock);
    if(r) { UnmapViewOfFile(r->base); CloseHandle(r->file); free(r); return; }
#endif
    free(data);
}
void BlockTrim(void)
{
#ifdef _WIN32
    block_region_t *r;
    AcquireSRWLockExclusive(&lock);
    /* VirtualUnlock of pages that were never locked removes them from the working set. Blocks stay writable (live
     * asset memory), so their pages are written to the file first: one whose write fails stays resident. */
    for(r=block_regions;r;r=r->next) {
        if(r->unsafe || !FlushViewOfFile(r->base,0)) { r->unsafe=1; continue; }
        VirtualUnlock(r->base,r->size);
    }
    ReleaseSRWLockExclusive(&lock);
#endif
}

void ProcessMemory(size_t *private_bytes,size_t *working_set,size_t *heap_used,size_t *heap_free)
{
    size_t used=0,unused=0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc; _HEAPINFO hi;
    memset(&pmc,0,sizeof(pmc)); pmc.cb=sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&pmc,sizeof(pmc));
    if(private_bytes) *private_bytes=pmc.PrivateUsage;
    if(working_set) *working_set=pmc.WorkingSetSize;
    hi._pentry=NULL;
    while(_heapwalk(&hi)==_HEAPOK) { if(hi._useflag==_USEDENTRY) used+=hi._size; else unused+=hi._size; }
#else
    if(private_bytes) *private_bytes=0;
    if(working_set) *working_set=0;
#endif
    if(heap_used) *heap_used=used;
    if(heap_free) *heap_free=unused;
}

/* " +0x<offset>" for each caller of the caller (module-relative), for diagnostics; empty elsewhere. */
void CallerOffsets(char *out,size_t size,unsigned skip)
{
    if(!out || !size) return;
    out[0]=0;
#ifdef _WIN32
    {
        void *frames[10]; unsigned short n=CaptureStackBackTrace(skip,10,frames,NULL),i; size_t used=0;
        HMODULE self=NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)(void *)CallerOffsets,&self);
        for(i=0;i<n && used+14<size;++i)
            used+=(size_t)snprintf(out+used,size-used," +0x%x",(unsigned)((uintptr_t)frames[i]-(uintptr_t)self));
    }
#else
    (void)skip;
#endif
}
