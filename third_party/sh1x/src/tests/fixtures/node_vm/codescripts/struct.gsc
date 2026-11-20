// Isolated native VM fixture; not a gameplay implementation.
InitStructs()
{
 precacheitem("m4_mp");
 logprint("SH1X_STOCK_WEAPON_PRECACHE_PASS\n");
 precacheitem("frag_grenade_mp");
 logprint("SH1X_OFFHAND_PRECACHE_PASS\n");
 level.struct = [];
 pathnodes = getallnodes();
 assert(pathnodes.size > 0);
 pathnode = pathnodes[0];
 assert(isdefined(pathnode.origin));
 assert(isdefined(pathnode.classname));
 pathnode.sh1x_probe = 73;
 assert(getallnodes()[0].sh1x_probe == 73);
 assert(!isdefined(getnode("missing_sh1x_probe", "targetname")));
 matches = getnodearray(pathnode.classname, "classname");
 assert(matches.size > 0);
 assert(matches[0] == pathnode);
 logprint("SH1X_PATH_NODE_VM_PASS\n");
 nodes = getallvehiclenodes();
 assert(nodes.size == 1);
 node = getvehiclenode("auto59", "targetname");
 assert(isdefined(node));
 assert(node == nodes[0]);
 assert(node.classname == "info_vehicle_node_rotate");
 assert(distance(node.origin, (5197.2, 1604, 16)) < 0.1);
 assert(!isdefined(getvehiclenode("missing_sh1x_probe", "targetname")));
 matches = getvehiclenodearray("auto59", "targetname"); assert(matches.size == 1);
 node.sh1x_probe = 42;
 assert(node.sh1x_probe == 42);
 logprint("SH1X_VEHICLE_NODE_VM_PASS\n");
 prop = spawn("script_model", (1, 2, 3));
 prop PhysicsLaunchClient((4, 5, 6), (7, 8, 9));
 assert(distance(prop.origin, (1, 2, 3)) < 0.01);
 prop delete();
 prop = spawn("script_model", (3, 2, 1));
 prop PhysicsLaunchClient();
 prop delete();
 logprint("SH1X_PHYSICS_LAUNCH_CLIENT_VM_PASS\n");
 hud = newhudelem();
 hud.fontscale = 1;
 hud changefontscaleovertime(2);
 hud.fontscale = 3;
 assert(hud.fontscale == 3);
 hud reset();
 hud changefontscaleovertime(1);
 hud destroy();
 logprint("SH1X_HUD_FONTSCALE_VM_PASS\n");
}
CreateStruct()
{
 s = spawnstruct();
 level.struct[level.struct.size] = s;
 return s;
}



