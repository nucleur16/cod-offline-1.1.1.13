The c8 Zombies crash is an invalid animation reference, not a missing actor
receiver tree. The native receiver already requires a live entity, nonempty
owned tree, DObj and equality between its DObj tree and its owned tree.

The exact c8 map from retained nm and source is:

- +0x3b4d8d: XAnimGetAnimTreeDebugName dereferences its null XAnim_s argument.
- +0x3a20cc: retained Scr_GetAnim mismatch diagnostic calls that getter on
  Scr_GetAnims(reference.tree), without a null check.
- +0x20b6c3: anim_param passed the owned receiver tree into Scr_GetAnim.
- CODOL_Scr_GetAnimTime+0x6a: the actor movement script requests this reference.

The native parity guard in codol_model_animation.c removes server-only names
before the tree is built. The retained 4dba script still queries
getAnimTime(%walk_and_run_loops), although C1's loaded animlists omit that node.
Existing measured documentation records the server/C1 index divergence caused
by including it (263 versus 247 entries). Re-enabling it is not a valid fix.
The parent and rawfiles reviewer own the separate stale-caller correction.

The corrected shared anim_param uses retained Scr_GetAnim(index,NULL) for
normal VM type/value extraction. It validates the 1-based realm-local tree id
against the actual registry count, requires a built source tree, requires the
source tree to equal the receiver tree and bounds the animation index. Invalid
references raise the ordinary VM parameter error before any unchecked lookup
or developer-only diagnostic metadata access. Valid GetAnimTime still calls
the retained XAnimGetTime and reads the native info pool's current phase.

The fixture compiles the actual complete CODOL_Scr_GetAnimTime, receiver and
helper source plus actual CODOL_ServerAnimTreeCount. It assembles seven exact
retained function bodies, including Scr_GetAnim, Scr_GetAnims and XAnimGetTime;
the code is extracted verbatim rather than translated. The x86 structure sizes
and phase offset are asserted. Entity/DObj ownership, script-model eligibility,
VM exception transfer and result emission are explicit fixture boundaries.
This is native function/ABI proof, not a real executing GSC match or C1 proof.

The donor's valid query returns the real native phase. Two isolated donor child
processes reproduce SIGSEGV for zero-reference and registered-but-unbuilt tree
cases. Each corrected profile passes 24 cases covering valid leaf/sync phases,
an unallocated info record's native zero result, invalid receivers, absent or
mismatched DObjs, absent/empty trees, wrong argument count/type, absent/out-of-
range source trees, mismatched built trees and out-of-range animation indices.

Normal serves MP/co-op; Zombies serves ZM. Existing 4d4 and c8 binaries remain
preserved. This native safety correction does not substitute a timing constant,
publish a missing animation node, or qualify the stale script caller. Real C1
gameplay acceptance belongs to the parent after the caller package is corrected.
