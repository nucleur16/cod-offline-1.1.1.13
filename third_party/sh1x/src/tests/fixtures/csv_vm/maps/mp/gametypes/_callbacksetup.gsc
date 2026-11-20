CodeCallback_StartGameType()
{
    a = tableLookup("mp/unlocktable.csv", 0, "ghillie_arctic", 3);
    b = tableLookup("mp/unlocktable.csv", 0, "ghillie_desert", 2);
    c = tableLookup("mp/unlocktable.csv", 0, "ghillie_urban", 3);
    if(a == "ch_ghillie" && b == "4" && c == "ch_ghillie_2")
        print("SH1X_REFERENCE_TABLE_VM_PASS\n");
    else
        print("SH1X_REFERENCE_TABLE_VM_FAIL\n");
}
CodeCallback_PlayerConnect() {}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
