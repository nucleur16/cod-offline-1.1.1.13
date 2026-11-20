EXPECTED_PERK = "specialty_fastreload";
CodeCallback_StartGameType() {}
CodeCallback_PlayerConnect()
{
    // Synthetic peer uploads immediately after connect. Keep the thread alive
    // through that upload; this delay is a test fixture, not production logic.
    wait 2;
    index = getClassIndex(" CUSTOM2 ");
    invalid = getClassIndex("autoassign");
    a = getClassPerk(0, 1);
    b = getClassPerk(index, 1);
    c = getClassPerk(1, 2);
    d = getClassPerk(7, 5);
    if(index == 1 && invalid == -1 && a == "" && b == EXPECTED_PERK && c == "" && d == "")
        print("SH1X_CLASS_PERK_VM_PASS\n");
    else
        print("SH1X_CLASS_PERK_VM_FAIL\n");
}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
