/* Room-scoped script dvar ownership. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_CVARS_H
#define PERSISTENT_CVARS_H
int PersistentCvarsBegin(const char *request_digest);
int PersistentCvarsRecord(const char *name);
int PersistentCvarsApply(const char *name,const char *value);
int PersistentCvarsRestore(const char *request_digest);
int PersistentCvarsEmpty(void);
#endif
