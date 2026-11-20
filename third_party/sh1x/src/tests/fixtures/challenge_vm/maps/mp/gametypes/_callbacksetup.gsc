CodeCallback_StartGameType()
{
    name = "mp/allChallengesTable.csv";
    print("SH1X_GHILLIE_ID=" + tableLookup(name, 0, "ch_ghillie", 4) + "\n");
    print("SH1X_GHILLIE_THRESHOLD1=" + tableLookup(name, 0, "ch_ghillie", 6) + "\n");
    print("SH1X_GHILLIE_THRESHOLD2=" + tableLookup(name, 0, "ch_ghillie", 8) + "\n");
    print("SH1X_GHILLIE_THRESHOLD3=" + tableLookup(name, 0, "ch_ghillie", 10) + "\n");
}
CodeCallback_PlayerConnect() {}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
