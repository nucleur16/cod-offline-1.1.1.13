/* Native perk-by-name query. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "perk_query.h"
#include "playerstate_ext.h"
#include "perks.h"
#include "assets.h"

int PlayerHasNamedPerk(const playerState_t *ps,const char *name)
{
    const engine_table_t *table;
    const ps_ext_t *extension;
    perk_definition_t definition={0,0};
    int present=0;
    if(!ps || !name || !*name) return -1;
    if(LoadEngineStringTable("mp/coPerkStatsTable.csv",&table)!=1 ||
       ReadPerkDefinition(table,name,&definition)!=1 ||
       definition.slot==PERK_NO_SLOT) return -1;
    extension=PlayerStateExtConst(ps);
    if(!extension || !HasPerk(&extension->perks,&definition,&present)) return -1;
    return present ? 1 : 0;
}
#endif
