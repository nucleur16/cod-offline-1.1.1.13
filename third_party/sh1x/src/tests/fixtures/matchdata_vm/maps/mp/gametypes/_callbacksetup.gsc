CodeCallback_StartGameType()
{
    setClientMatchDataDef("mp/clientmatchdata.def");
    setClientMatchData("highlightCount", 2);
    setClientMatchData("players", 18, "score", -12345);
    setClientMatchData("players", 18, "kills", 65535);
    setClientMatchData("players", 18, "xuid", "native-matchdata");
    setClientMatchDataDef("MP/CLIENTMATCHDATA.DEF");
    a = getClientMatchData("highlightCount");
    b = getClientMatchData("players", 18, "score");
    c = getClientMatchData("players", 18, "kills");
    d = getClientMatchData("players", 18, "xuid");
    if(a == 2 && b == -12345 && c == -1 && d == "native-matchdata")
        print("SH1X_MATCHDATA_VM_PASS\n");
    else
        print("SH1X_MATCHDATA_VM_FAIL\n");
}
CodeCallback_PlayerConnect()
{
    // Let the synthetic peer finish stats, pure validation and its first move.
    wait 2;
    sendClientMatchData();
    print("SH1X_MATCHDATA_SEND_REQUESTED\n");
}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
