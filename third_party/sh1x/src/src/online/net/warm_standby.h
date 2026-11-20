/* Prepared native match integration. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef WARM_STANDBY_H
#define WARM_STANDBY_H
#include <stdint.h>
void WarmStandbyRegister(void);
void WarmStandbyBeginMap(void);
void WarmStandbyMapReady(void);
int WarmStandbyFrame(void);
int WarmStandbyAdmit(uint64_t uid);
int WarmStandbyUnassigned(void);
const char *WarmStandbyStatus(void);
#endif
