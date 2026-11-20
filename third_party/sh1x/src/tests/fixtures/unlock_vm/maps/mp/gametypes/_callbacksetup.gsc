CodeCallback_StartGameType() {}
CodeCallback_PlayerConnect()
{
    // Test admin binds the synthetic secure profile after upload.
    wait 3;
    a = self isItemUnlocked("ghillie_arctic");
    b = self isItemUnlocked("ghillie_urban");
    c = self isItemUnlocked("ghillie_desert");
    d = self isItemUnlocked("cardtitle_ghilliemist");
    if(a && b && !c && d)
        print("SH1X_UNLOCK_VM_PASS\n");
    else
        print("SH1X_UNLOCK_VM_FAIL\n");
}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
