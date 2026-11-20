#include "sv_frame.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* The engine-state translator owns this in the server; the harness stubs the
  * translator out, so it supplies the diagnostic slot the capture writes. */
const char *player_failure;
static hud_state_t produced;
static int accepted=1;
int CollectServerHud(hud_state_t *out,const hudElemState_t *source)
{
    assert(source); *out=produced; return accepted;
}
int main(void)
{
    hudElemState_t source={0}; hud_state_t out,before;
    produced.count[0]=1; produced.elements[0][0].field[0]=123;
    assert(CaptureClientSnapshotHud(0,42,&source));
    produced.elements[0][0].field[0]=456;
    assert(CaptureClientSnapshotHud(1,42,&source));
    assert(CopyClientSnapshotHud(0,42,&out) && out.elements[0][0].field[0]==123);
    before=out;
    assert(!CopyClientSnapshotHud(0,41,&out) && !memcmp(&out,&before,sizeof(out)));
    assert(CaptureClientSnapshotHud(0,43,&source));
    assert(!CopyClientSnapshotHud(0,42,&out));
    assert(CopyClientSnapshotHud(0,43,&out) && out.elements[0][0].field[0]==456);
    accepted=0; assert(!CaptureClientSnapshotHud(0,44,&source));
    assert(!CopyClientSnapshotHud(0,43,&out) && !CopyClientSnapshotHud(0,44,&out));
    assert(CopyClientSnapshotHud(1,42,&out));
    InvalidateClientSnapshotHud(1); assert(!CopyClientSnapshotHud(1,42,&out));
    accepted=1; assert(CaptureClientSnapshotHud(1,45,&source));
    assert(!CaptureClientSnapshotHud(1,46,NULL) && !CopyClientSnapshotHud(1,45,&out));
    assert(!CaptureClientSnapshotHud(MAX_CLIENTS,1,&source));
    assert(!CopyClientSnapshotHud(MAX_CLIENTS,1,&out));
    assert(!CopyClientSnapshotHud(0,44,NULL));
    ClearClientSnapshotHud(0); ClearClientSnapshotHud(1); ClearClientSnapshotHud(MAX_CLIENTS);
    puts("PASS: per-client HUD capture ownership, sequence identity, invalidation, failed replacement and teardown");
}
