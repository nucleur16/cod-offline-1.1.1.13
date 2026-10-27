// survival rules entry: Spec Ops Survival, its own match category.
//
// CODOL_ScriptEntryPoints (src/codol/script/codol_script_source.c) runs this
// module's main() for every survival match, whatever g_gametype says: the
// category is the one input that selects it (fs_game survival, codol_zoneType
// survival), as maps/_zombiemode_gametype is zm's.  Survival is separate from
// the co-op campaign: it mounts common_gametype_survival on the unchanged
// multiplayer level and compiles none of maps/_coop* (docs/CODOL_SURVIVAL_PLAN.md).

main()
{
	maps\_survival_callbacksetup::SetupCallbacks();
	maps\_survival_mode::init();
}
