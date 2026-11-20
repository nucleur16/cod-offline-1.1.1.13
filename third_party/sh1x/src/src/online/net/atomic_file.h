/* Same-directory replacement for native server state. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ATOMIC_FILE_H
#define ATOMIC_FILE_H
#include <stddef.h>
int AtomicWriteFile(const char *path, const void *data, size_t length);
int AtomicWriteHomeFile(const char *qpath, const void *data, size_t length);
#endif
