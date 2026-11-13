/* Corrected worker/custom-ABI thunk only. All native code is data; native va
 * is mocked. No execution of C1,5F4BF0 or C1 CRT; no network sockets. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/gfx/localized_substitution.c"
static unsigned checks;
#define CHECK(x) do { assert(x);checks++; } while(0)
typedef struct { DWORD base;unsigned size;BYTE *data; } Region;
static Region regions[10];static unsigned region_count;
static FARPROC timer_import;static int allow_thunk,va_calls,va_bad_return,va_bad_index;
static DWORD observed_caller;
static BYTE block[16],context[8];static void *tls_root[1];
static struct { BYTE pre[8];BYTE state[0x804];BYTE post[8]; } tls;
static void *fixture_at(DWORD va)
{
    if(va==0x9a1d78c || va==0xb424c0)return &timer_import;
    for(unsigned i=0;i<region_count;i++)if(va>=regions[i].base && va-regions[i].base<regions[i].size)
        return regions[i].data+va-regions[i].base;
    return NULL;
}
static int fixture_known(DWORD caller);
static const char *fixture_va(const char *format,const char *text)
{
    CHECK(!strcmp(format,"%s"));const char *nul=memchr(text,0,1024);CHECK(nul);
    int index;memcpy(&index,tls.state+0x800,4);CHECK(index==0 || index==1);
    char *result=(char *)tls.state+index*1024;memcpy(result,text,(size_t)(nul-text)+1);
    int next=va_bad_index ? index:1-index;memcpy(tls.state+0x800,&next,4);va_calls++;
    return va_bad_return ? "wrong":result;
}
#define K33NUM_AT(va) fixture_at(va)
#define K33NUM_VA(format,text) fixture_va(format,text)
#define K33NUM_TLS_ROOT() tls_root
#define K33NUM_KNOWN(address) fixture_known(address)
#include "../src/gfx/localized_numeric.c"
static int fixture_known(DWORD caller)
{
    observed_caller=caller;return allow_thunk || K33NumKnownCaller(caller);
}
static void reset_tls(void)
{
    memset(&tls,0,sizeof(tls));memset(tls.pre,0x5a,8);memset(tls.post,0x5a,8);
    tls_root[0]=block;BYTE *p=context;memcpy(block+12,&p,4);p=tls.state;memcpy(context+4,&p,4);
    va_bad_return=va_bad_index=allow_thunk=0;
}
static void canaries(void)
{
    for(unsigned i=0;i<8;i++)CHECK(tls.pre[i]==0x5a && tls.post[i]==0x5a);
}
extern const char fixture_after_numeric_thunk;
/* This ordinary C-callable fixture enters the corrected thunk with C1's
 * ECX+stack convention and four possible incoming stack alignments. It
 * independently checks caller cleanup and preserved nonvolatile registers. */
static const char *__attribute__((naked,noinline)) fixture_invoke(const int32_t *values,const char *format,int count,int alignment)
{
    __asm__ __volatile__(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $16,%esp\n\t"
        "movl $0x13572468,%ebx\n\tmovl $0x23456789,%esi\n\tmovl $0x34567890,%edi\n\t"
        "andl $-16,%esp\n\tsubl 20(%ebp),%esp\n\tmovl %esp,-16(%ebp)\n\t"
        "pushl 16(%ebp)\n\tpushl 12(%ebp)\n\tmovl 8(%ebp),%ecx\n\tcall _K33NumThunk\n\t"
        ".globl _fixture_after_numeric_thunk\n_fixture_after_numeric_thunk:\n\t"
        "addl $8,%esp\n\tmovl %eax,-20(%ebp)\n\tcmpl %esp,-16(%ebp)\n\tjne 1f\n\t"
        "cmpl $0x13572468,%ebx\n\tjne 1f\n\tcmpl $0x23456789,%esi\n\tjne 1f\n\t"
        "cmpl $0x34567890,%edi\n\tjne 1f\n\tmovl -20(%ebp),%eax\n\tjmp 2f\n"
        "1:\n\txorl %eax,%eax\n2:\n\tlea -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n\t");
}

static void worker_tests(void)
{
    reset_tls();int32_t values[9]={12,-7,0,1234,INT32_MIN,INT32_MAX,1,-1,9};
    static const DWORD callers[]={0x4aa1df,0x4aa446,0x4aa4a3,0x4b4552,0x5ede38,0x701014};
    for(unsigned i=0;i<6;i++)CHECK(!strcmp(K33NumWorker(callers[i],values,"&&1 / &&2",2),"12 / -7"));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&1 &&2 &&3 &&4",4),"12 -7 0 1234"));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&5 &&6 &&9",9),"-2147483648 2147483647 9"));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&1/&&2/&&3/&&4/&&5/&&6/&&7/&&8/&&9",9),
                  "12/-7/0/1234/-2147483648/2147483647/1/-1/9"));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&10 &&x 50%",1),"120 &&x 50%"));
    CHECK(!strcmp(K33NumWorker(callers[0],NULL,"literal &&x",0),"literal &&x"));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&0",1),""));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&2",1),""));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"&&1",0),""));
    CHECK(!strcmp(K33NumWorker(callers[0],NULL,"&&1",1),""));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",-1),""));
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",10),""));
    CHECK(!strcmp(K33NumWorker(callers[0],values,NULL,1),""));
    int before=va_calls;CHECK(!strcmp(K33NumWorker(0,values,"&&1",1),""));CHECK(va_calls==before);
    /* Local corrected decimal conversion, including signed boundaries. */
    for(unsigned i=0;i<9;i++) {
        char expected[12],actual[12];K33LocView view;snprintf(expected,sizeof(expected),"%d",values[i]);
        K33NumDecimal(values[i],actual,&view);CHECK(!strcmp(actual,expected) && view.length==strlen(expected));
    }
    /* Exactly the native two-slot lifetime: the second result preserves the
     * first; the third consumes its slot, without introducing a private ring. */
    reset_tls();const char *first=K33NumWorker(callers[0],values,"first &&1",1);
    const char *second=K33NumWorker(callers[0],values,"second &&2",2);
    CHECK(first==(const char *)tls.state && second==(const char *)tls.state+1024);
    CHECK(!strcmp(first,"first 12") && !strcmp(second,"second -7"));
    const char *third=K33NumWorker(callers[0],values,"third",0);
    CHECK(third==first && !strcmp(third,"third") && !strcmp(second,"second -7"));
    char literal[1025];memset(literal,'a',1024);literal[1024]=0;
    const char *bounded=K33NumWorker(callers[0],NULL,literal,0);CHECK(strlen(bounded)==1023);
    CHECK(!memcmp(bounded,literal,1023));canaries();
    reset_tls();allow_thunk=1;
    for(int alignment=0;alignment<=12;alignment+=4) {
        const char *result=fixture_invoke(values,"&&1 &&2",2,alignment);
        CHECK(result && !strcmp(result,"12 -7"));
        CHECK(observed_caller==(DWORD)(uintptr_t)&fixture_after_numeric_thunk);canaries();
    }
    reset_tls();BYTE *valid=context;tls_root[0]=NULL;before=va_calls;
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));CHECK(va_calls==before);
    reset_tls();valid=NULL;memcpy(block+12,&valid,4);before=va_calls;
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));CHECK(va_calls==before);
    reset_tls();valid=NULL;memcpy(context+4,&valid,4);before=va_calls;
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));CHECK(va_calls==before);
    for(int index=-1;index<=2;index+=3) {
        reset_tls();memcpy(tls.state+0x800,&index,4);before=va_calls;
        CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));CHECK(va_calls==before);canaries();
    }
    reset_tls();va_bad_return=1;CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));
    reset_tls();va_bad_index=1;CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));
    reset_tls();SYSTEM_INFO si;GetSystemInfo(&si);DWORD previous;size_t page=si.dwPageSize;
    BYTE *memory=VirtualAlloc(NULL,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(memory);
    CHECK(VirtualProtect(memory+page,page,PAGE_NOACCESS,&previous));
    CHECK(!strcmp(K33NumWorker(callers[0],(const int32_t *)(memory+page),"&&1",1),""));
    memcpy(memory+page-4,values,4);
    CHECK(!strcmp(K33NumWorker(callers[0],(const int32_t *)(memory+page-4),"&&1 &&2",2),""));
    memory[page-1]='x';CHECK(!strcmp(K33NumWorker(callers[0],values,(const char *)memory+page-1,1),""));
    valid=memory+page;memcpy(context+4,&valid,4);before=va_calls;
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));CHECK(va_calls==before);
    VirtualFree(memory,0,MEM_RELEASE);reset_tls();canaries();
    BYTE *readonly=VirtualAlloc(NULL,0x804,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(readonly);
    CHECK(VirtualProtect(readonly,0x804,PAGE_READONLY,&previous));
    memcpy(context+4,&readonly,4);before=va_calls;
    CHECK(!strcmp(K33NumWorker(callers[0],values,"text",1),""));CHECK(va_calls==before);
    VirtualFree(readonly,0,MEM_RELEASE);reset_tls();canaries();
}

static void read_code(HANDLE file,DWORD va,void *out,DWORD bytes)
{
    BYTE header[4096];DWORD got=0;SetFilePointer(file,0,NULL,FILE_BEGIN);
    CHECK(ReadFile(file,header,sizeof(header),&got,NULL) && got==sizeof(header));
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)header;
    IMAGE_NT_HEADERS32 *nt=(IMAGE_NT_HEADERS32 *)(header+dos->e_lfanew);
    CHECK(dos->e_magic==IMAGE_DOS_SIGNATURE && nt->Signature==IMAGE_NT_SIGNATURE && nt->OptionalHeader.ImageBase==0x400000);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);DWORD rva=va-0x400000;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        if(rva>=s->VirtualAddress && rva+bytes<=s->VirtualAddress+s->SizeOfRawData) {
            SetFilePointer(file,s->PointerToRawData+rva-s->VirtualAddress,NULL,FILE_BEGIN);
            CHECK(ReadFile(file,out,bytes,&got,NULL) && got==bytes);return;
        }
    }
    CHECK(0);
}
static void add_region(HANDLE file,DWORD va,unsigned size)
{
    Region *r=&regions[region_count++];r->base=va;r->size=size;
    r->data=VirtualAlloc(NULL,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(r->data);read_code(file,va,r->data,size);
}
static void guard_tests(const char *path)
{
    CHECK(LoadLibraryA("winmm.dll"));timer_import=GetProcAddress(GetModuleHandleA("winmm.dll"),"timeGetTime");CHECK(timer_import);
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(file!=INVALID_HANDLE_VALUE);
    add_region(file,0x5f4bf0,sizeof(kK33NumOriginal));add_region(file,0x6286d0,sizeof(kK33NumVa));add_region(file,0xada5ad,sizeof(kK33NumVsn));
    for(unsigned i=0;i<5;i++)add_region(file,kK33NumFrames[i].address,kK33NumFrames[i].size);
    add_region(file,0xc40a74,3);add_region(file,0xc41f10,3);CloseHandle(file);CHECK(K33NumGuards());
    unsigned corruptions=0;
    for(unsigned group=0;group<region_count;group++)for(unsigned i=0;i<regions[group].size;i++) {
        regions[group].data[i]^=1;CHECK(!K33NumGuards());CHECK(!InstallBoundedLocalizedNumeric());
        if(group)CHECK(!memcmp(fixture_at(0x5f4bf0),kK33NumOriginal,sizeof(kK33NumOriginal)));
        regions[group].data[i]^=1;corruptions++;
    }
    CHECK(corruptions==2159);CHECK(K33NumGuards());
    BYTE *timer=(BYTE *)fixture_at(0x4aa320)+0x49;DWORD saved,slot=0xb422e0;memcpy(&saved,timer,4);memcpy(timer,&slot,4);
    CHECK(!InstallBoundedLocalizedNumeric());memcpy(timer,&saved,4);
    FARPROC real=timer_import;timer_import=NULL;CHECK(!InstallBoundedLocalizedNumeric());
    timer_import=GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetTickCount");CHECK(!InstallBoundedLocalizedNumeric());timer_import=real;
    CHECK(K33NumGuards());CHECK(InstallBoundedLocalizedNumeric());BYTE *target=fixture_at(0x5f4bf0);LONG relative;memcpy(&relative,target+1,4);
    CHECK(target[0]==0xe9 && (uintptr_t)target+5+(intptr_t)relative==(uintptr_t)K33NumThunk);
    for(unsigned i=5;i<9;i++)CHECK(target[i]==0x90);
    CHECK(!memcmp(target+9,kK33NumOriginal+9,sizeof(kK33NumOriginal)-9));
    for(unsigned i=0;i<5;i++) {
        const K33LocSite *s=&kK33NumFrames[i];const BYTE *at=fixture_at(s->address);
        if(s->address==0x4aa320)CHECK(!memcmp(at,s->bytes,0x49) && !memcmp(at+0x4d,s->bytes+0x4d,s->size-0x4d));
        else CHECK(!memcmp(at,s->bytes,s->size));
    }
    CHECK(!InstallBoundedLocalizedNumeric());for(unsigned i=0;i<region_count;i++)VirtualFree(regions[i].data,0,MEM_RELEASE);
}
int main(int argc,char **argv)
{
    CHECK(argc==2);worker_tests();guard_tests(argv[1]);
    printf("PASS %u numeric worker/thunk/guard checks; native va mocked, original routine never executed\n",checks);return 0;
}
