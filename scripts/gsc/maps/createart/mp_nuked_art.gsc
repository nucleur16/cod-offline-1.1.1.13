// PROVENANCE (2026-09-01): stand-in, not vendor code.  The zone-shipped
// maps/mp/mp_nuked_fx.gsc (extracted verbatim from mp_nuked.ff, in this
// overlay) calls maps\createart\mp_nuked_art::main() OUTSIDE its dev block,
// but no zone in this build ships that module and vendor/iw4 has no mp_nuked
// at all.  Without this file the loader records an unresolved module and the
// call degrades to a stubbed builtin named "main".  An art module only sets
// fog/light dvars client-side, so an empty main() is the truthful minimum --
// nothing is invented here.
main()
{
}
