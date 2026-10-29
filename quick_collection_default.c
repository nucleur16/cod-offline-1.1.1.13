/* Exact C1 Quick-only numeric float dvar 2580 registration. Its flags are 0,
 * so ordinary startup/config settings are refused. Change only this caller's
 * default operand, never the shared 90.0 constant or registration flags/domain.
 * Native Quick frame reads this slot in phases1/2 at71B060 and71B24A.
 * Ready, vote, admission and enter-room countdown remain unchanged. */
static const float g_quickCollectionDefault=30.0f;
#ifndef QCD_REGISTRATION
#define QCD_REGISTRATION 0x006fe722u
#define QCD_NAME 0x00c7abd0u
#define QCD_OLD_DEFAULT 0x00c844c0u
#endif
static const BYTE kQuickCollectionRegistration[]={
    0xd9,0x05,0x50,0x44,0xc8,0x00,0x83,0xc4,0x18,0x68,0xad,0xfe,0xc2,0x00,
    0x53,0x83,0xec,0x0c,0xd9,0x5c,0x24,0x08,0xa3,0xa0,0x4d,0xc4,0x06,
    0xd9,0xee,0xd9,0x5c,0x24,0x04,0xd9,0x05,0xc0,0x44,0xc8,0x00,0xd9,
    0x1c,0x24,0x68,0xd0,0xab,0xc7,0x00,0xe8,0x7a,0x5d,0xf2,0xff,0x68,
    0xad,0xfe,0xc2,0x00,0x53,0x68,0xad,0xfe,0xc2,0x00,0x68,0xd8,0xab,
    0xc7,0x00,0xa3,0xa4,0x4d,0xc4,0x06
};
static void PatchQuickCollectionDefault(void)
{
    BYTE *registration=(BYTE *)(DWORD_PTR)QCD_REGISTRATION;
    const void *name=(const void *)(DWORD_PTR)QCD_NAME;
    const void *oldDefault=(const void *)(DWORD_PTR)QCD_OLD_DEFAULT;
    const DWORD ninety=0x42b40000u;
    if (!Readable(registration,sizeof(kQuickCollectionRegistration)) ||
        memcmp(registration,kQuickCollectionRegistration,sizeof(kQuickCollectionRegistration)) ||
        !Readable(name,5) || memcmp(name,"2580",5) ||
        !Readable(oldDefault,4) || memcmp(oldDefault,&ninety,4)) {
        Log("Quick collection: exact registration differs; unchanged");return;
    }
    DWORD pointer=(DWORD)(DWORD_PTR)&g_quickCollectionDefault;
    WriteCode(registration+0x23,&pointer,4);
    Log("Quick collection: native default 30 seconds; flags/domain/Ready unchanged");
}
