CodeCallback_StartGameType()
{
    effect = loadFx("misc/thermal_beacon_inverted");
    PrecacheFxTeamThermal(effect, "J_Spine4");
    PrecacheFxTeamThermal(effect, "j_spine4");
    // Distinct diagnostic tag exercises an override, not model attachment rendering.
    PrecacheFxTeamThermal(effect, "sh1x_thermal_probe");
    PrecacheFxTeamThermal(effect, "SH1X_THERMAL_PROBE");
    effectConstant = tableLookup("mp/constConfigStrings_mp.csv", 1, "misc/thermal_beacon_inverted", 0);
    print("SH1X_THERMAL_EFFECT_CONSTANT=" + effectConstant + "\n");
    print("SH1X_THERMAL_VM_PASS\n");
}
CodeCallback_PlayerConnect() {}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
