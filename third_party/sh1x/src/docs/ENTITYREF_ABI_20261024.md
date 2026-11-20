# Retained entity reference return ABI

The retained `Scr_GetEntityRef` assembly reads one unsigned index from its first
stack argument and returns the packed entity/class reference in EAX. It delegates
the packing to `Scr_GetEntityIdRefExtern`. Its former C declaration returned
`scr_entref_t`, which adds an implicit output pointer under the i386 Linux GCC
struct-return ABI. The assembly interpreted that stack pointer as the argument
index instead of the explicit zero.

The production attack-dog goal-node caller confirms this mismatch: it pushed
zero followed by a stack destination pointer before calling the retained entry.
Live patrol and exit calls raised `parameter -6820691 does not exist` at
`maps/mp/_dogs.gsc:1089`, aborting those script paths. The changed declaration
returns a scalar; `Scr_GetEntityRefValue` unpacks that scalar in compiled code.
Actor goal nodes, vehicle node arguments, and the existing common entity-field
callers use the wrapper. Assembly callers keep their existing ABI.

Verification includes the full 32-bit Linux server build, an executable i386
fixture using the production declaration/wrapper and a one-argument assembly
entry, native path-node ownership/lookup checks, and inspection of the rebuilt
goal-node caller. It now passes only zero and consumes EAX. No GSC workaround,
client launch, production native replacement, or live dog acceptance is included.

This is a native correction. It must retain the normal native package and
maintenance gates; it is not eligible for a script-only release.

Both native profile input manifests include the corrected ABI and callers.
The Zombies source delta is unchanged; it materializes the corrected shared
declarations/callers and its species-specific actor implementation. Previous
profile binary hashes are retained as prior results and the current tested
binary fields are cleared. The default-tree full build and focused tests above
do not qualify a reproducible profile package for production.

The reported Zombies hellhound loss of effects, sound and player damage remains
a separate investigation. The demonstrated goal-node ABI failure is from MP
attack dogs; it does not establish the cause of those hellhound symptoms.
