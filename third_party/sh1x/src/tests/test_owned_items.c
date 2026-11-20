/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "owned_items.h"

static int failures;
#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while(0)

static int lookup(const char *text, unsigned item, unsigned now)
{
    return OwnedItemsLookup(text, strlen(text), item, now);
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if(!f) { perror(path); exit(2); }
    fputs(text, f);
    fclose(f);
}

int main(void)
{
    const char *manifest =
        "# owned_items -- purchases exported by dw_marketplace_buy.py\n"
        "# columns: itemId  expiryEpoch(0=permanent)  qty   # name\n"
        "3003   0           1     # per/specialty_scavenger\n"
        "3023   2000        1     # per/specialty_extraammo (rental)\r\n"
        "3021   0           0     # spent\n"
        "3030   -1          1\n"
        "garbage line\n"
        "3041 0";
    char dir[] = "obj_owned_items_test";
    char path[256];

    CHECK(lookup(manifest, 3003, 1000) == 1);      /* permanent */
    CHECK(lookup(manifest, 3023, 1000) == 1);      /* rental still running */
    CHECK(lookup(manifest, 3023, 2000) == 0);      /* rental ended */
    CHECK(lookup(manifest, 3021, 1000) == 0);      /* qty 0 is not owned */
    CHECK(lookup(manifest, 3030, 99999) == 1);     /* -1 is permanent */
    CHECK(lookup(manifest, 3041, 1000) == 0);      /* two-column row is malformed */
    CHECK(lookup(manifest, 3001, 1000) == 0);      /* absent */
    CHECK(lookup("3003 0 1\n3003 0 0\n", 3003, 1) == 0); /* last row wins */
    CHECK(OwnedItemsLookup(NULL, 0, 3003, 1) == 0);

    CHECK(ReadOwnedItem(NULL, 1, 3003, 1) == -1);
    CHECK(ReadOwnedItem("", 1, 3003, 1) == -1);

    snprintf(path, sizeof(path), "mkdir -p %s && rm -f %s/owned_items*.txt", dir, dir);
    if(system(path) != 0) { fprintf(stderr, "cannot prepare %s\n", dir); return 2; }
    CHECK(ReadOwnedItem(dir, 77, 3003, 1) == 0);   /* no manifest: nothing bought */
    snprintf(path, sizeof(path), "%s/owned_items.txt", dir);
    write_file(path, "3003 0 1\n");
    CHECK(ReadOwnedItem(dir, 77, 3003, 1) == 1);   /* shared file */
    snprintf(path, sizeof(path), "%s/owned_items.77.txt", dir);
    write_file(path, "3019 0 1\n");
    CHECK(ReadOwnedItem(dir, 77, 3003, 1) == 0);   /* per-uid file wins over shared */
    CHECK(ReadOwnedItem(dir, 77, 3019, 1) == 1);
    CHECK(ReadOwnedItem(dir, 0, 3003, 1) == 1);    /* no uid reads the shared file */

    if(failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    puts("test_owned_items: ok");
    return 0;
}
