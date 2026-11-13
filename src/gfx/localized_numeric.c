/* Bounded exact-C1 numeric localization. The replaced5F4BF0 is never called.
 * Input ABI is ECX=int32 values*, stack=format,count, caller cleans8bytes.
 * A fixed%s call to C1's guarded va preserves its two-slot TLS lifetime. */
#ifndef K33NUM_AT
#define K33NUM_AT(va) K33LOC_AT(va)
#endif
#ifndef K33NUM_KNOWN
#define K33NUM_KNOWN(address) K33NumKnownCaller(address)
#endif
#ifndef K33NUM_VA
#define K33NUM_VA(format,text) (((const char *(__cdecl *)(const char *,...))(uintptr_t)0x006286d0u)(format,text))
#endif
#include "localized_numeric_guards.h"

static int K33NumKnownCaller(DWORD address)
{
    static const DWORD callers[]={0x4aa1df,0x4aa446,0x4aa4a3,0x4b4552,0x5ede38,0x701014};
    for(unsigned i=0;i<sizeof(callers)/sizeof(callers[0]);i++)if(address==callers[i])return 1;
    return 0;
}

static void K33NumDecimal(int32_t value,char output[12],K33LocView *view)
{
    char reverse[10];unsigned n=0,used=0;uint32_t magnitude=(uint32_t)value;
    if(value<0) { output[used++]='-';magnitude=0u-magnitude; }
    do { reverse[n++]=(char)('0'+magnitude%10u);magnitude/=10u; } while(magnitude);
    while(n)output[used++]=reverse[--n];
    output[used]=0;view->data=output;view->length=used;
}

static void **K33NumTlsRoot(void)
{
#ifdef K33NUM_TLS_ROOT
    return K33NUM_TLS_ROOT();
#else
    void **root;__asm__ __volatile__("movl %%fs:0x2c,%0":"=r"(root));return root;
#endif
}

static BYTE *K33NumTlsState(void)
{
    /* These offsets and two1024-byte slots are pinned by the entire native
     * va body. Validate every dereference plus full writable state and index;
     * missing/uninitialized thread state must not enter the native function. */
    void **root=K33NumTlsRoot();BYTE *block,*context,*state;int index;
    if(!K33LocReadable(root,sizeof(*root)))return NULL;
    memcpy(&block,root,sizeof(block));
    if(!K33LocReadable(block,16))return NULL;
    memcpy(&context,block+12,sizeof(context));
    if(!K33LocReadable(context,8))return NULL;
    memcpy(&state,context+4,sizeof(state));
    if(!K33LocWritable(state,0x804))return NULL;
    memcpy(&index,state+0x800,4);
    return index==0 || index==1 ? state:NULL;
}

static const char *__attribute__((cdecl,noinline,used,force_align_arg_pointer))
K33NumWorker(DWORD caller,const int32_t *numbers,const char *format,int count)
{
    static const char empty[]="";char output[K33LOC_CAPACITY],decimal[9][12];
    K33LocView text,values[9];memset(output,0,sizeof(output));memset(values,0,sizeof(values));
    if(!K33NUM_KNOWN(caller))return empty;
    if(count>=0 && count<=9 && (!count || K33LocReadable(numbers,(size_t)count*4)) &&
       K33LocViewOf(format,&text)) {
        for(int i=0;i<count;i++) { int32_t number;memcpy(&number,numbers+i,4);K33NumDecimal(number,decimal[i],&values[i]); }
        K33LocFormat(text,values,count,output,sizeof(output));
    }
    BYTE *state=K33NumTlsState();int index;
    if(!state)return empty;
    memcpy(&index,state+0x800,4);
    const char *result=K33NUM_VA("%s",output);
    int next;memcpy(&next,state+0x800,4);
    if(result!=(const char *)state+index*1024 || next!=1-index ||
       !memchr(result,0,1024))return empty;
    return result;
}

/* Explicit thunk for C1's ECX+stack convention. No native trampoline. The
 * cdecl worker preserves EBX/ESI/EDI/EBP, realigns its stack, returns EAX. */
static const char *__attribute__((naked,noinline,used)) K33NumThunk(void)
{
    __asm__ __volatile__(
        "pushl %ebp\n\t"
        "movl %esp,%ebp\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "pushl %ecx\n\t"
        "pushl 4(%ebp)\n\t"
        "call _K33NumWorker\n\t"
        "addl $16,%esp\n\t"
        "leave\n\t"
        "ret\n\t");
}

static int K33NumBytes(DWORD va,const BYTE *bytes,unsigned size)
{
    const void *at=K33NUM_AT(va);
    return K33LocReadable(at,size) && !memcmp(at,bytes,size);
}

static int K33NumGuards(void)
{
    static const BYTE decimal[]={'%','d',0},string[]={'%','s',0};
    if(!K33NumBytes(0x5f4bf0,kK33NumOriginal,sizeof(kK33NumOriginal)) ||
       !K33NumBytes(0x6286d0,kK33NumVa,sizeof(kK33NumVa)) ||
       !K33NumBytes(0xada5ad,kK33NumVsn,sizeof(kK33NumVsn)) ||
       !K33NumBytes(0xc40a74,decimal,sizeof(decimal)) || !K33NumBytes(0xc41f10,string,sizeof(string)))return 0;
    for(unsigned i=0;i<sizeof(kK33NumFrames)/sizeof(kK33NumFrames[0]);i++) {
        const K33LocSite *site=&kK33NumFrames[i];const BYTE *at=K33NUM_AT(site->address);
        if(!K33LocReadable(at,site->size))return 0;
        if(site->address==0x4aa320) {
            /* C1 reconstructed import9A1D78C and packed B424C0 both name
             * WINMM!timeGetTime in the independently checked import tables. */
            DWORD slot;memcpy(&slot,at+0x49,4);
            if(memcmp(at,site->bytes,0x49) || memcmp(at+0x4d,site->bytes+0x4d,site->size-0x4d) ||
               (slot!=0x09a1d78cu && slot!=0x00b424c0u))return 0;
            FARPROC genuine=GetProcAddress(GetModuleHandleA("winmm.dll"),"timeGetTime"),resolved;
            const void *pointer=K33NUM_AT(slot);
            if(!genuine || !K33LocReadable(pointer,sizeof(resolved)))return 0;
            memcpy(&resolved,pointer,sizeof(resolved));if(resolved!=genuine)return 0;
        } else if(memcmp(at,site->bytes,site->size))return 0;
    }
    return 1;
}

static int InstallBoundedLocalizedNumeric(void)
{
    if(!K33NumGuards())return 0;
    BYTE patch[9]={0xe9,0,0,0,0,0x90,0x90,0x90,0x90};DWORD previous,ignored;
    void *target=K33NUM_AT(0x5f4bf0);LONG relative=(LONG)((uintptr_t)K33NumThunk-((uintptr_t)target+5));
    memcpy(patch+1,&relative,4);
    if(!VirtualProtect(target,sizeof(patch),PAGE_EXECUTE_READWRITE,&previous))return 0;
    memcpy(target,patch,sizeof(patch));int matched=!memcmp(target,patch,sizeof(patch));
    int flushed=FlushInstructionCache(GetCurrentProcess(),target,sizeof(patch));
    int restored=VirtualProtect(target,sizeof(patch),previous,&ignored);
    return matched && flushed && restored;
}
