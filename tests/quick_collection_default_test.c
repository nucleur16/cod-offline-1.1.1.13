#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned writes,checks;
static BYTE fixtureRegistration[128],fixtureName[5];static DWORD fixtureOldDefault;
#define QCD_REGISTRATION ((DWORD_PTR)fixtureRegistration)
#define QCD_NAME ((DWORD_PTR)fixtureName)
#define QCD_OLD_DEFAULT ((DWORD_PTR)&fixtureOldDefault)
static BOOL Readable(const void *at,SIZE_T n) {
    DWORD_PTR p=(DWORD_PTR)at;
    return (p==QCD_REGISTRATION && n<=sizeof(fixtureRegistration)) || (p==QCD_NAME && n<=sizeof(fixtureName)) ||
           (p==QCD_OLD_DEFAULT && n<=4);
}
static void WriteCode(void *to,const void *from,SIZE_T n) { ++writes;memcpy(to,from,n); }
#define Log(...) ((void)0)
#include "../src/rooms/quick_collection_default.c"
static void require(int ok,const char *why) { ++checks;if (!ok) { fprintf(stderr,"FAIL %s\n",why);ExitProcess(1); } }
static void reset(void) {
    memcpy(fixtureRegistration,kQuickCollectionRegistration,sizeof(kQuickCollectionRegistration));
    memcpy(fixtureName,"2580",5);fixtureOldDefault=0x42b40000;writes=0;
}
int main(void) {
    reset();PatchQuickCollectionDefault();
    require(writes==1,"exact one operand write");
    DWORD pointer;memcpy(&pointer,fixtureRegistration+0x23,4);
    require(*(float*)pointer==30.0f,"default operand reads thirty");
    require(fixtureOldDefault==0x42b40000,"shared ninety constant unchanged");
    BYTE result[sizeof(kQuickCollectionRegistration)];memcpy(result,fixtureRegistration,sizeof(result));
    memcpy(result+0x23,kQuickCollectionRegistration+0x23,4);
    require(!memcmp(result,kQuickCollectionRegistration,sizeof(result)),"all other fixtureRegistration bytes unchanged");
    for (unsigned i=0;i<sizeof(kQuickCollectionRegistration);i++) {
        reset();fixtureRegistration[i]^=1;PatchQuickCollectionDefault();require(writes==0,"any changed fixtureRegistration byte rejects");
    }
    reset();fixtureName[0]='3';PatchQuickCollectionDefault();require(writes==0,"wrong dvar fixtureName rejects");
    reset();fixtureOldDefault=0x42b00000;PatchQuickCollectionDefault();require(writes==0,"wrong default rejects");
    printf("PASS: %u exact Quick fixtureRegistration checks\n",checks);
}
