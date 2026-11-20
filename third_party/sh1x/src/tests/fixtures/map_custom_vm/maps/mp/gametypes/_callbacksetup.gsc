CodeCallback_StartGameType()
{
    map = getMapCustom("map");
    environment = getMapCustom("environment");
    allies = getMapCustom("allieschar");
    print("SH1X_MAP_CUSTOM_ENV=" + environment + " ALLIES=" + allies + "\n");
    if(map == "mp_bog_sh" && environment != "" && allies != "" && getMapCustom("sh1x_absent_key") == "")
        print("SH1X_MAP_CUSTOM_VM_PASS\n");
    else
        print("SH1X_MAP_CUSTOM_VM_FAIL\n");
}
CodeCallback_PlayerConnect() {}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
