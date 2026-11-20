/* Local developer capability; production builds cannot enable it.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef LOCALTEST_H
#define LOCALTEST_H
#include <string.h>

static inline int LocalTestPolicy(const char *environment, const char *enabled,
                                      const char *zone, const char *gametype,
                                      const char *dedicated)
{
#ifdef LOCAL_TEST_BUILD
    return environment && !strcmp(environment, "1") && enabled && !strcmp(enabled, "1") &&
           zone && !strcmp(zone, "zm") && gametype && !strcmp(gametype, "zm") &&
           dedicated && !strcmp(dedicated, "2");
#else
    (void)environment; (void)enabled; (void)zone; (void)gametype; (void)dedicated;
    return 0;
#endif
}
int LocalTestAllowed(void);
int LocalTestBuild(void);
void Scr_LocalTestAllowed(void);
#endif
