CodeCallback_StartGameType()
{
    setDvar("sh1x_vector_fixture", "(-1.5, 20, 3)");
    a = getDvarVector("sh1x_vector_fixture");
    b = getDvarVector("sh1x_missing_vector_fixture", "4 5 6");
    if(a == (-1.5, 20, 3) && b == (4, 5, 6))
        print("SH1X_DVAR_VECTOR_VM_PASS\n");
    else
        print("SH1X_DVAR_VECTOR_VM_FAIL\n");
}
CodeCallback_PlayerConnect() {}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
