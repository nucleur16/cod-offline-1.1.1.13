/* Infostring bounds (sh1x security audit 2026-10-26): Info_SetValueForKey
 * may never write a result of MAX_INFO_STRING characters (its terminator
 * landed one byte past the buffer), and Info_RemoveKey copies keys bounded.
 * The real qshared.c is linked; unreferenced code is discarded.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void QDECL Com_Printf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_DPrintf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_PrintWarning(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_PrintError(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
int Q_vsnprintf(char *str, size_t size, const char *format, va_list args)
{ return vsnprintf(str, size, format, args); }
void QDECL Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; assert(!"Com_Error"); }

void Info_RemoveKey(char *s, const char *key);

/* Guard bytes around each buffer catch a write past the end. */
static struct { char info[MAX_INFO_STRING]; char guard[16]; } box;

static void fill(char *s, size_t length)
{
    /* "\k\" + value of the requested total length. */
    size_t i;
    assert(length >= 4);
    s[0] = '\\'; s[1] = 'k'; s[2] = '\\';
    for (i = 3; i < length; ++i) s[i] = 'v';
    s[length] = 0;
}

int main(void)
{
    char key[BIG_INFO_STRING + 64];
    size_t i;

    /* Room for "\ip\1.2.3.4" (11) exactly: 1012 + 11 = 1023 fits. */
    memset(box.guard, 0x5a, sizeof(box.guard));
    fill(box.info, MAX_INFO_STRING - 1 - 11);
    Info_SetValueForKey(box.info, "ip", "1.2.3.4");
    assert(strlen(box.info) == MAX_INFO_STRING - 1);
    assert(!strncmp(box.info, "\\ip\\1.2.3.4\\k\\", 14));
    for (i = 0; i < sizeof(box.guard); ++i) assert(box.guard[i] == 0x5a);

    /* One more byte would need MAX_INFO_STRING + 1 bytes: refused, untouched. */
    fill(box.info, MAX_INFO_STRING - 11);   /* 1013 + 11 = 1024 characters */
    Info_SetValueForKey(box.info, "ip", "1.2.3.4");
    assert(strlen(box.info) == MAX_INFO_STRING - 11);
    assert(box.info[0] == '\\' && box.info[1] == 'k');
    for (i = 0; i < sizeof(box.guard); ++i) assert(box.guard[i] == 0x5a);

    /* Replacing a key still works and removes the old pair. */
    strcpy(box.info, "\\name\\old\\rate\\25000");
    Info_SetValueForKey(box.info, "name", "new");
    assert(!strcmp(box.info, "\\name\\new\\rate\\25000"));
    assert(!strcmp(Info_ValueForKey(box.info, "rate"), "25000"));
    Info_RemoveKey(box.info, "rate");
    assert(!strcmp(box.info, "\\name\\new"));

    /* A key longer than the key buffer is skipped, not copied past it. */
    key[0] = '\\';
    for (i = 1; i < BIG_INFO_STRING + 32; ++i) key[i] = 'x';
    strcpy(key + i, "\\v\\a\\b");
    Info_RemoveKey(key, "a");
    assert(!strcmp(key + BIG_INFO_STRING + 32, "\\v"));
    puts("CoDOL infostring edits stay inside MAX_INFO_STRING and bound oversize keys");
    return 0;
}
