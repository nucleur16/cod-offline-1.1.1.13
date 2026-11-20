CodeCallback_StartGameType()
{
    near = spawnstruct(); near.origin = (1, 0, 0);
    far = spawnstruct(); far.origin = (0, 0, 9);
    middle = spawnstruct(); middle.origin = (0, 4, 0);
    input = [];
    input["far"] = far; input[7] = near; input["middle"] = middle;
    sorted = SortByDistance(input, (0, 0, 0));
    if (sorted.size != 3 || sorted[0] != near || sorted[1] != middle || sorted[2] != far)
    { print("SH1X_SORT_VM_FAIL_ORDER\n"); return; }
    if (input["far"] != far || input[7] != near || input.size != 3)
    { print("SH1X_SORT_VM_FAIL_INPUT\n"); return; }
    reverse = SortByDistance(input, (0, 0, 10));
    if (reverse[0] != far) { print("SH1X_SORT_VM_FAIL_ORIGIN\n"); return; }
    empty = SortByDistance([], (0, 0, 0));
    if (empty.size != 0) { print("SH1X_SORT_VM_FAIL_EMPTY\n"); return; }
    entity = spawn("script_origin", (0, 0, 2));
    mixed = []; mixed[0] = entity; mixed[1] = near;
    result = SortByDistance(mixed, (0, 0, 0));
    if (result[0] != near || result[1] != entity)
    { print("SH1X_SORT_VM_FAIL_ENTITY\n"); return; }
    entity delete();
    print("SH1X_SORT_VM_PASS\n");
}
CodeCallback_PlayerConnect() {}
CodeCallback_PlayerDisconnect() {}
CodeCallback_PlayerDamage() {}
CodeCallback_PlayerKilled() {}
CodeCallback_PlayerLastStand() {}
CodeCallback_ScriptCommand() {}
