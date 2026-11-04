/* Corrected helper only. C1 bytes are read as nonexecutable fixture data.
 * No C1 process, original routine, network or overflow demonstration. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static BYTE *image;
static DWORD caller=0x45533b;
static void *fixture_at(DWORD va) { assert(va>=0x450000 && va<0x600000);return image+va-0x450000; }
#define K33LOC_AT(va) fixture_at(va)
#define K33LOC_CALLER() caller
#include "../localized_substitution.c"
static unsigned checks;
#define CHECK(x) do { assert(x);checks++; } while(0)
static K33LocView view(const char *s) { K33LocView v={s,s ? strlen(s):0};return v; }

static void expect(const char *format,K33LocView *args,int count,const char *wanted,K33LocResult result)
{
    struct { unsigned char pre[8];char text[1024];unsigned char post[8]; } out;
    memset(&out,0x5a,sizeof(out));
    CHECK(K33LocFormat(view(format),args,count,out.text,sizeof(out.text))==result);
    CHECK(!strcmp(out.text,wanted));
    for(unsigned i=0;i<8;i++) CHECK(out.pre[i]==0x5a && out.post[i]==0x5a);
    for(size_t i=strlen(wanted);i<sizeof(out.text);i++) CHECK(out.text[i]==0);
}
static void policy_tests(void)
{
    K33LocView args[9];for(unsigned i=0;i<9;i++)args[i]=view("player");
    args[0]=view("Alice");args[1]=view("12");args[8]=view("nine");
    expect("Players &&1 / &&2",args,2,"Players Alice / 12",K33LOC_OK);
    expect("&&2 &&1 &&1",args,2,"12 Alice Alice",K33LOC_OK);
    expect("&&9",args,9,"nine",K33LOC_OK);
    expect("&&10",args,1,"Alice0",K33LOC_OK);
    expect("&&&1",args,1,"&Alice",K33LOC_OK);
    expect("literal &&x &&",NULL,0,"literal &&x &&",K33LOC_OK);
    expect("",NULL,0,"",K33LOC_OK);
    args[0]=view("");expect("a&&1b",args,1,"ab",K33LOC_OK);args[0]=view("Alice");
    expect("&&0",args,1,"",K33LOC_INVALID);
    expect("&&2",args,1,"",K33LOC_INVALID);
    expect("&&9",args,8,"",K33LOC_INVALID);
    expect("&&1",NULL,0,"",K33LOC_INVALID);
    expect("text",args,-1,"",K33LOC_INVALID);
    expect("text",args,10,"",K33LOC_INVALID);
    expect("text",NULL,1,"",K33LOC_INVALID);
    args[0]=view(NULL);expect("&&1",args,1,"",K33LOC_INVALID);
    expect("literal",args,1,"literal",K33LOC_OK);args[0]=view("Alice");
    expect(NULL,args,1,"",K33LOC_INVALID);
    K33LocView excessive={"x",K33LOC_INPUT_LIMIT+1};char single='z';
    CHECK(K33LocFormat(excessive,args,1,&single,1)==K33LOC_INVALID && !single);
    CHECK(K33LocFormat(view("x"),args,1,NULL,1)==K33LOC_INVALID);
    single='z';CHECK(K33LocFormat(view("x"),args,1,&single,0)==K33LOC_INVALID && single=='z');
    /* Small harmless strings across every capacity, with adjacent canaries. */
    const char *wanted="Start Alice with 12 points, Alice.";
    for(size_t capacity=1;capacity<=64;capacity++) {
        unsigned char arena[80];memset(arena,0x5a,sizeof(arena));char *out=(char *)arena+8;
        K33LocResult result=K33LocFormat(view("Start &&1 with &&2 points, &&1."),args,2,out,capacity);
        size_t n=strlen(wanted)<capacity-1 ? strlen(wanted):capacity-1;
        CHECK(result==(n<strlen(wanted) ? K33LOC_TRUNCATED:K33LOC_OK));
        CHECK(!memcmp(out,wanted,n) && out[n]==0);
        for(unsigned i=0;i<8;i++)CHECK(arena[i]==0x5a);
        for(size_t i=8+capacity;i<sizeof(arena);i++)CHECK(arena[i]==0x5a);
    }
    char tiny[4]={'x','x','x','x'};
    CHECK(K33LocFormat(view("ordinary text &&2"),args,1,tiny,sizeof(tiny))==K33LOC_INVALID);
    CHECK(!memcmp(tiny,"\0\0\0\0",4)); /* Invalid reference beyond visible truncation. */
    /* Corrected-helper boundary only: benign repeated literal, no native call. */
    char ordinary[1025];memset(ordinary,'a',sizeof(ordinary)-1);ordinary[1024]=0;
    struct { char text[1024];unsigned char canary[8]; } full;
    memset(&full,0x5a,sizeof(full));
    CHECK(K33LocFormat(view(ordinary),NULL,0,full.text,sizeof(full.text))==K33LOC_TRUNCATED);
    CHECK(strlen(full.text)==1023 && !memcmp(full.text,ordinary,1023));
    for(unsigned i=0;i<8;i++)CHECK(full.canary[i]==0x5a);
    args[0]=view(ordinary);memset(&full,0x5a,sizeof(full));
    CHECK(K33LocFormat(view("&&1"),args,1,full.text,sizeof(full.text))==K33LOC_TRUNCATED);
    CHECK(strlen(full.text)==1023 && !memcmp(full.text,ordinary,1023));
    for(unsigned i=0;i<8;i++)CHECK(full.canary[i]==0x5a);
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
static void guard_tests(const char *path)
{
    image=VirtualAlloc(NULL,0x1b0000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(image);
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(file!=INVALID_HANDLE_VALUE);
    read_code(file,0x5f4de0,fixture_at(0x5f4de0),sizeof(kK33LocOriginal));
    for(unsigned i=0;i<6;i++)read_code(file,kK33LocFrames[i].address,fixture_at(kK33LocFrames[i].address),kK33LocFrames[i].size);
    CloseHandle(file);CHECK(K33LocGuards());
    /* Every guarded byte rejects installation before mutation. Native bytes
     * are data only and never called. Successful production installation
     * transiently changes page protection, then restores PAGE_READWRITE. */
    unsigned corruptions=0;
    for(unsigned group=0;group<7;group++) {
        BYTE *at=group ? fixture_at(kK33LocFrames[group-1].address):fixture_at(0x5f4de0);
        size_t size=group ? kK33LocFrames[group-1].size:sizeof(kK33LocOriginal);
        for(size_t i=0;i<size;i++) {
            at[i]^=1;CHECK(!K33LocGuards());CHECK(!InstallBoundedLocalizedSubstitution());
            if(group) CHECK(!memcmp(fixture_at(0x5f4de0),kK33LocOriginal,sizeof(kK33LocOriginal)));
            at[i]^=1;corruptions++;
        }
    }
    CHECK(corruptions==3177);CHECK(K33LocGuards());
    CHECK(InstallBoundedLocalizedSubstitution());
    BYTE *target=fixture_at(0x5f4de0);LONG relative;
    memcpy(&relative,target+1,4);CHECK(target[0]==0xe9 && target[5]==0x90);
    CHECK((uintptr_t)target+5+(intptr_t)relative==(uintptr_t)K33LocReplacement);
    CHECK(!memcmp(target+6,kK33LocOriginal+6,sizeof(kK33LocOriginal)-6));
    for(unsigned i=0;i<6;i++) CHECK(!memcmp(fixture_at(kK33LocFrames[i].address),kK33LocFrames[i].bytes,kK33LocFrames[i].size));
    CHECK(!InstallBoundedLocalizedSubstitution()); /* No blind repeat patch. */
    VirtualFree(image,0,MEM_RELEASE);
}
static void native_tests(void)
{
    struct { BYTE pre[8];char text[1024];BYTE post[8]; } out;
    K33LocArgs args={2,{"Alice","12"}};
    CHECK(sizeof(args)==40);
    for(unsigned i=0;i<6;i++) {
        caller=kK33LocSites[i].return_address;memset(&out,0x5a,sizeof(out));
        K33LocReplacement("&&1 has &&2 points",&args,out.text);
        CHECK(!strcmp(out.text,"Alice has 12 points"));
        for(unsigned n=0;n<8;n++)CHECK(out.pre[n]==0x5a && out.post[n]==0x5a);
    }
    args.count=0;K33LocReplacement("ordinary text",&args,out.text);CHECK(!strcmp(out.text,"ordinary text"));
    K33LocReplacement("literal",NULL,out.text);CHECK(!strcmp(out.text,"literal"));
    K33LocReplacement("&&1",NULL,out.text);CHECK(out.text[0]==0);
    args.count=10;K33LocReplacement("text",&args,out.text);CHECK(out.text[0]==0);
    args.count=-1;K33LocReplacement("text",&args,out.text);CHECK(out.text[0]==0);
    args.count=1;args.values[0]=NULL;K33LocReplacement("&&1",&args,out.text);CHECK(out.text[0]==0);
    K33LocReplacement(NULL,&args,out.text);CHECK(out.text[0]==0);
    caller=0;memset(&out,0x5a,sizeof(out));K33LocReplacement("text",NULL,out.text);
    CHECK(!out.text[0] && out.text[1]==0x5a && out.pre[7]==0x5a && out.post[0]==0x5a);
    caller=kK33LocSites[0].return_address;
    SYSTEM_INFO si;GetSystemInfo(&si);size_t page=si.dwPageSize;DWORD previous;
    char *memory=VirtualAlloc(NULL,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(memory);
    CHECK(VirtualProtect(memory+page,page,PAGE_NOACCESS,&previous));
    memory[page-2]='x';memory[page-1]=0;
    K33LocReplacement(memory+page-2,NULL,out.text);CHECK(!strcmp(out.text,"x"));
    memory[page-1]='x';K33LocReplacement(memory+page-2,NULL,out.text);CHECK(out.text[0]==0);
    K33LocReplacement("text",(const K33LocArgs *)(memory+page),out.text);CHECK(out.text[0]==0);
    K33LocReplacement("text",NULL,memory+page-1);CHECK(memory[page-1]=='x');
    K33LocReplacement("text",NULL,memory+page); /* Inaccessible output never touched. */
    VirtualFree(memory,0,MEM_RELEASE);
    memory=VirtualAlloc(NULL,K33LOC_INPUT_LIMIT+1,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(memory);
    memset(memory,'a',K33LOC_INPUT_LIMIT);memory[K33LOC_INPUT_LIMIT]=0;
    K33LocReplacement(memory,NULL,out.text);CHECK(out.text[0]==0);VirtualFree(memory,0,MEM_RELEASE);
}
int main(int argc,char **argv)
{
    CHECK(argc==2);policy_tests();native_tests();guard_tests(argv[1]);
    printf("PASS %u corrected-helper/byte-guard checks; original C1 routine never executed\n",checks);
    return 0;
}
