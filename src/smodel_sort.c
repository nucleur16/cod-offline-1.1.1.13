/*
 * smodel_sort.c - repairs the static model references C1's load-time static model sort leaves stale. Included by
 * vlobby_env.c; installed for every map (InstallSmodelSort), it changes nothing where C1's own renumbering suffices.
 *
 * When a world loads, C1 (1.1.1.13) sorts its static models by the material of each model's first drawn surface
 * (0x00AA1F50, called from 0x00AA3433): it rotates blocks of the draw instances (world + 0x224, 0x4C bytes each)
 * and of the culling instances (world + 0x218, 0x24 bytes each) into material order, then renumbers the smodel
 * references the moved blocks had: those of the primary lights' shadow geometry (world + 0x1C0) and those of each
 * cell's FIRST aabb tree node (world + 0x48, one pointer per cell, world + 0x34 cells). The other nodes of the cell
 * trees (children at node + [node + 0x28], node + 0x18 of them, 0x2C bytes each; smodelIndexCount at + 0x22,
 * smodelIndexes at + 0x24) keep the old numbers, so after a sort that moved anything the culling marks the wrong
 * static models visible: in the tree's leaves an index now names whatever instance the rotation put there.
 *
 * In the stock worlds the sort moves models too, but only ones the first node lists (stock lobby 1015 models,
 * mp_highrise 3491, mp_afghan 11430: 0 references stale), so the bug never showed. The merged lobby zone
 * (tools/vlobby_merge) appends the donor scenes' static models after the stock world's 1015, out of material order,
 * and puts a new root over the stock tree: the sort moves blocks of the stock world's models and 7930 of the lobby's
 * 8825 tree references went stale (the vehicles and props that vanished, and stray models drawn in their place). The
 * more donor models, the more blocks move: hence the apparent limit near 1015-1024. It is not a count limit.
 *
 * The repair: before C1's sort, every draw instance's runtime word at + 0x44 (its cached-model slots, which C1 clears
 * after a sort that moved anything) is set to the instance's index, and every tree node's index list is copied.
 * After the sort the words say where each instance went; every node's list (the first nodes' too, from the copies,
 * so nothing is renumbered twice) is rewritten through that permutation and the words are put back.
 *
 * C1 sites (1.1.1.13), byte-checked; on a mismatch one line is logged and C1's own sort runs unrepaired:
 *   0x00AA3433  call 0x00AA1F50 (the sort; returns nonzero when it moved models), replaced by a call to SmsSort
 *   0x00AA1F50  the sort: world pointer 0x00CF39A0, count world + 0x1C8, draw instances world + 0x224 (0x00AA1F70),
 *               0x4C each (0x00AA1FA4), the first-node renumbering over world + 0x34 / world + 0x48 (0x00AA227F)
 */

#define SMS_CALL_VA      0x00AA3433u
#define SMS_SORT_VA      0x00AA1F50u
#define SMS_WORLD_PTR_VA 0x00CF39A0u
#define SMS_W_CELLS      0x34u     /* int: cells */
#define SMS_W_ROOTS      0x48u     /* GfxAabbTree *[cells]: each cell's first tree node */
#define SMS_W_COUNT      0x1C8u    /* unsigned: static models */
#define SMS_W_DRAW       0x224u    /* draw instances, SMS_DRAW_SIZE each */
#define SMS_DRAW_SIZE    0x4Cu
#define SMS_DRAW_TAG     0x44u     /* DWORD: runtime cached-model slots, borrowed for the tag */
#define SMS_DRAW_CHECK   0x3Au     /* placement, model, cull distance: what the sort moves unchanged (it sets + 0x3E/0x3F) */
#define SMS_NODE_SIZE    0x2Cu
#define SMS_NODE_KIDS    0x18u     /* WORD */
#define SMS_NODE_COUNT   0x22u     /* WORD smodelIndexCount */
#define SMS_NODE_LIST    0x24u     /* WORD *smodelIndexes */
#define SMS_NODE_CHILD   0x28u     /* int: offset of the children from the node */
#define SMS_MAX_NODES    (1 << 20)

typedef char (__cdecl *SmsSortFn)(void);
typedef struct { WORD *list; DWORD count, at; } SmsList;   /* a node's index list and where its copy starts */

static void *SmsGrow(void *p, SIZE_T bytes)
{
    return p ? HeapReAlloc(GetProcessHeap(), 0, p, bytes) : HeapAlloc(GetProcessHeap(), 0, bytes);
}

static void SmsFree(void *p)
{
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

/* every node of every cell's tree that lists static models, with a copy of its list (copy[at .. at + count)) */
static BOOL SmsCollect(const BYTE *w, SmsList **outLists, int *outCount, WORD **outCopy)
{
    int cells = *(const int *)(w + SMS_W_CELLS);
    BYTE *const *roots = *(BYTE *const *const *)(w + SMS_W_ROOTS);
    *outLists = NULL; *outCount = 0; *outCopy = NULL;
    if (cells <= 0) return TRUE;
    if (cells > 65536 || !Readable(roots, (SIZE_T)cells * 4)) return FALSE;
    BYTE **stack = NULL;
    SmsList *lists = NULL;
    WORD *copy = NULL;
    int sp = 0, stackMax = 0, n = 0, listMax = 0, visited = 0;
    DWORD copied = 0, copyMax = 0;
    BOOL ok = TRUE;
    for (int c = 0; c < cells && ok; c++) {
        if (!roots[c]) continue;
        if (!Readable(roots[c], SMS_NODE_SIZE)) { ok = FALSE; break; }
        sp = 0;
        if (stackMax < 1) { stackMax = 256; stack = SmsGrow(stack, stackMax * sizeof(*stack)); if (!stack) { ok = FALSE; break; } }
        stack[sp++] = roots[c];
        while (sp && ok) {
            BYTE *node = stack[--sp];
            if (++visited > SMS_MAX_NODES) { ok = FALSE; break; }
            WORD count = *(const WORD *)(node + SMS_NODE_COUNT);
            if (count) {
                WORD *list = *(WORD *const *)(node + SMS_NODE_LIST);
                if (!Readable(list, (SIZE_T)count * 2)) { ok = FALSE; break; }
                if (n == listMax) {
                    listMax = listMax ? listMax * 2 : 1024;
                    SmsList *g = SmsGrow(lists, (SIZE_T)listMax * sizeof(*lists));
                    if (!g) { ok = FALSE; break; }
                    lists = g;
                }
                if (copied + count > copyMax) {
                    while (copied + count > copyMax) copyMax = copyMax ? copyMax * 2 : 16384;
                    WORD *g = SmsGrow(copy, (SIZE_T)copyMax * 2);
                    if (!g) { ok = FALSE; break; }
                    copy = g;
                }
                lists[n].list = list; lists[n].count = count; lists[n].at = copied;
                memcpy(copy + copied, list, (SIZE_T)count * 2);
                copied += count;
                n++;
            }
            WORD kids = *(const WORD *)(node + SMS_NODE_KIDS);
            if (!kids) continue;
            BYTE *child = node + *(const int *)(node + SMS_NODE_CHILD);
            if (!Readable(child, (SIZE_T)kids * SMS_NODE_SIZE)) { ok = FALSE; break; }
            if (sp + kids > stackMax) {
                while (sp + kids > stackMax) stackMax *= 2;
                BYTE **g = SmsGrow(stack, (SIZE_T)stackMax * sizeof(*stack));
                if (!g) { ok = FALSE; break; }
                stack = g;
            }
            for (int k = 0; k < kids; k++) stack[sp++] = child + (SIZE_T)SMS_NODE_SIZE * k;
        }
    }
    SmsFree(stack);
    if (!ok) { SmsFree(lists); SmsFree(copy); return FALSE; }
    *outLists = lists; *outCount = n; *outCopy = copy;
    return TRUE;
}

static char __cdecl SmsSort(void)
{
    SmsSortFn sort = (SmsSortFn)(DWORD_PTR)SMS_SORT_VA;
    BYTE *w = *(BYTE *const *)(DWORD_PTR)SMS_WORLD_PTR_VA;
    if (!w || !Readable(w, SMS_W_DRAW + 4)) return sort();
    unsigned n = *(const unsigned *)(w + SMS_W_COUNT);
    BYTE *draw = *(BYTE *const *)(w + SMS_W_DRAW);
    if (n < 2) return sort();
    if (n > 65535 || !Readable(draw, (SIZE_T)n * SMS_DRAW_SIZE)) {
        Log("smodel sort: %u static models at %p not readable; C1's sort runs unrepaired", n, draw);
        return sort();
    }
    SmsList *lists;
    int listCount;
    WORD *copy;
    if (!SmsCollect(w, &lists, &listCount, &copy)) {
        Log("smodel sort: the cell trees could not be walked; C1's sort runs unrepaired");
        return sort();
    }
    DWORD *saved = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * 4);
    WORD *where = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * 2);
    if (!saved || !where) {
        SmsFree(saved); SmsFree(where); SmsFree(lists); SmsFree(copy);
        Log("smodel sort: out of memory; C1's sort runs unrepaired");
        return sort();
    }
#if K33_DIAGNOSTICS
    BYTE *before = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * SMS_DRAW_SIZE);
    if (before) memcpy(before, draw, (SIZE_T)n * SMS_DRAW_SIZE);
#endif
    for (unsigned i = 0; i < n; i++) {
        DWORD *tag = (DWORD *)(draw + (SIZE_T)i * SMS_DRAW_SIZE + SMS_DRAW_TAG);
        saved[i] = *tag;
        *tag = i;
    }
    char moved = sort();
    /* where[old] = new, from the tags the instances carried through the sort */
    BOOL permutation = TRUE;
    memset(where, 0xFF, (SIZE_T)n * 2);
    for (unsigned p = 0; p < n && permutation; p++) {
        DWORD old = *(const DWORD *)(draw + (SIZE_T)p * SMS_DRAW_SIZE + SMS_DRAW_TAG);
        if (old >= n || where[old] != 0xFFFF) permutation = FALSE;
        else where[old] = (WORD)p;
    }
    DWORD refs = 0, repaired = 0, outOfRange = 0;
    if (permutation) {
        for (int l = 0; l < listCount; l++) {
            WORD *list = lists[l].list;
            const WORD *was = copy + lists[l].at;
            for (DWORD k = 0; k < lists[l].count; k++, refs++) {
                if (was[k] >= n) { outOfRange++; continue; }
                WORD want = where[was[k]];
                if (list[k] != want) { list[k] = want; repaired++; }
            }
        }
        for (unsigned p = 0; p < n; p++) {
            DWORD old = *(const DWORD *)(draw + (SIZE_T)p * SMS_DRAW_SIZE + SMS_DRAW_TAG);
            *(DWORD *)(draw + (SIZE_T)p * SMS_DRAW_SIZE + SMS_DRAW_TAG) = saved[old];
        }
    } else {
        /* cannot happen with C1's rotations; the tags are cleared as C1 would (0x00A95EF0 after a moving sort) */
        for (unsigned p = 0; p < n; p++) *(DWORD *)(draw + (SIZE_T)p * SMS_DRAW_SIZE + SMS_DRAW_TAG) = 0;
    }
    if (moved || !permutation || repaired || outOfRange)
        Log("smodel sort: %u static models, sort moved=%d; %lu tree references in %d nodes, %lu renumbered after it%s%s",
            n, (int)(unsigned char)moved, (unsigned long)refs, listCount, (unsigned long)repaired,
            permutation ? "" : " (NOT a permutation: left as C1 sorted)", outOfRange ? " (some references out of range)" : "");
#if K33_DIAGNOSTICS
    if (before && permutation && moved) {
        DWORD stale = 0;
        for (int l = 0; l < listCount; l++)
            for (DWORD k = 0; k < lists[l].count; k++) {
                WORD was = copy[lists[l].at + k];
                if (was < n && memcmp(draw + (SIZE_T)lists[l].list[k] * SMS_DRAW_SIZE, before + (SIZE_T)was * SMS_DRAW_SIZE,
                                      SMS_DRAW_CHECK))
                    stale++;
            }
        Log("smodel sort: check: %lu tree references name another instance than before the sort", (unsigned long)stale);
    }
    SmsFree(before);
#endif
    SmsFree(saved); SmsFree(where); SmsFree(lists); SmsFree(copy);
    return moved;
}

static void InstallSmodelSort(void)
{
    static const BYTE kSortHead[16] = { 0x83, 0xEC, 0x24, 0xA1, 0xA0, 0x39, 0xCF, 0x00, 0x55, 0x8B, 0xA8, 0xC8, 0x01, 0x00, 0x00, 0x89 };
    static const BYTE kSortDraw[6] = { 0x8B, 0xB0, 0x24, 0x02, 0x00, 0x00 };             /* 0x00AA1F70 mov esi, [eax+0x224] */
    static const BYTE kSortStride[3] = { 0x6B, 0xED, 0x4C };                             /* 0x00AA1FA4 imul ebp, ebp, 0x4C */
    static const BYTE kSortRoots[20] = { 0x8B, 0x0D, 0xA0, 0x39, 0xCF, 0x00, 0x33, 0xC0, 0x89, 0x44, 0x24, 0x2C,
                                         0x39, 0x41, 0x34, 0x7E, 0x6D, 0x8B, 0x51, 0x48 }; /* 0x00AA227F cells, roots */
    static const BYTE kNodeList[13] = { 0x66, 0x3B, 0x4E, 0x22, 0x73, 0x4D, 0x8B, 0x7C, 0x24, 0x20, 0x8B, 0x46, 0x24 };
    BYTE *call = (BYTE *)(DWORD_PTR)SMS_CALL_VA;
    LONG rel = (LONG)SMS_SORT_VA - (LONG)(SMS_CALL_VA + 5);
    if (!Readable(call, 5) || call[0] != 0xE8 || memcmp(call + 1, &rel, 4) ||
        !Readable((void *)(DWORD_PTR)SMS_SORT_VA, 0x360) ||
        memcmp((void *)(DWORD_PTR)SMS_SORT_VA, kSortHead, sizeof(kSortHead)) ||
        memcmp((void *)(DWORD_PTR)0x00AA1F70u, kSortDraw, sizeof(kSortDraw)) ||
        memcmp((void *)(DWORD_PTR)0x00AA1FA4u, kSortStride, sizeof(kSortStride)) ||
        memcmp((void *)(DWORD_PTR)0x00AA227Fu, kSortRoots, sizeof(kSortRoots)) ||
        memcmp((void *)(DWORD_PTR)0x00AA229Au, kNodeList, sizeof(kNodeList))) {
        Log("smodel sort: the static model sort 0x%08X or its call 0x%08X differs; not repaired", SMS_SORT_VA, SMS_CALL_VA);
        return;
    }
    BYTE patch[5] = { 0xE8 };
    LONG to = (LONG)(DWORD_PTR)SmsSort - (LONG)(SMS_CALL_VA + 5);
    memcpy(patch + 1, &to, 4);
    WriteCode(call, patch, sizeof(patch));
    Log("smodel sort: the static model sort's call 0x%08X repairs the tree references", SMS_CALL_VA);
}
