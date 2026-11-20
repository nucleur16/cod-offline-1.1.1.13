#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#include "radar_input.h"
#ifdef OL_SERVER
#include "radar_types.h"
#endif
#include "sys_cod4defs.h"
#include <string.h>

/* C1 client field c3c1e0 is a generic integer at client+35a4. Keep the
 * integer for script reads; only the frame publisher converts it to a bool.
 * This is live gclient input, never part of a spectator/archive copy. */
static int radar_block_input[MAX_CLIENTS];

int ClientRadarBlockInput(unsigned client)
{
    return client < MAX_CLIENTS ? radar_block_input[client] : 0;
}
void SetClientRadarBlockInput(unsigned client, int value)
{
    if (client < MAX_CLIENTS) radar_block_input[client] = value;
}
void ResetClientRadarInput(unsigned client)
{
    SetClientRadarBlockInput(client, 0);
#ifdef OL_SERVER
    ResetClientRadarTypes(client);
#endif
}
void ResetAllRadarInput(void)
{
    memset(radar_block_input, 0, sizeof(radar_block_input));
#ifdef OL_SERVER
    ResetAllRadarTypes();
#endif
}

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentRadarInputEmpty(void) {return resident_static_zero(radar_block_input,sizeof(radar_block_input));}
#endif
