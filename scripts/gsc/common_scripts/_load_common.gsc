// common_scripts/_load_common.gsc -- the level-load systems that belong to the
// MAP, not to the mode: destructibles, exploders, pipes, the dynamic world,
// elevators, minefields, the old-style destructables and lantern glows.
//
// WHY A SHARED MODULE.  Both IW4 level loaders, maps\_load.gsc (campaign and
// Spec Ops) and maps\mp\_load.gsc (multiplayer), start the same
// common_scripts systems over the map's entities -- _destructible::init,
// _elevator::init, _pipes::main, _dynamic_world::init, _fx::initFX and the
// exploder setup (IW4-GSC maps/_load.gsc:289-371, maps/mp/_load.gsc) -- and
// T5 moves that shared half into maps\_load_common.gsc, which its campaign
// _load and its maps\_zombiemode_load both call.  CoDOL plays coop on the
// multiplayer level zones (the coop zones are addons, docs/
// CODOL_CATEGORY_SCRIPT_SETS_20260917.md section 2.3), so a coop match stands
// on exactly the entities a multiplayer match does -- destructible cars and
// props on crash, estate and favela, exploder geometry, steam and water pipes
// -- and none of them ran, because the only thing that started them was
// maps\mp\_load::main(), whose #include of maps\mp\_utility compiles the
// ~100-file multiplayer chain into the match.
//
// PROVENANCE.  maps/mp/_load.gsc of this tree (IW4 maps\mp\_load.gsc, CoDOL
// adapted): main()'s map-system half, setupExploders(), exploder_load() and
// lanterns(), unchanged except where marked.  Left in the multiplayer loader,
// because they are the mode's, not the map's: tweakables, the HUD and leader
// dialog function pointers, game["thermal_vision"] and the vision set, the
// global fx and reflection-probe path, the trigger_hurt suicide sweep (IW4
// multiplayer only; trigger_hurt itself is native), the multiplayer mission
// entity cleanup, and the createfx editor.  The multiplayer loader is not
// changed and does not call this file, so a multiplayer match compiles
// nothing new.
//
// COMPILE CLOSURE: common_scripts only (utility, _fx, _createfx,
// _createfxmenu, _destructible, _destructible_types, _elevator, _pipes,
// _dynamic_world) plus maps/mp/_minefields and maps/mp/_destructables, which
// include nothing.  MEASURED with tools/codol_script_resolve.py --only-seeds.
//
// USE.  From the level script's main() -- the precache window, before
// CodeCallback_StartGameType -- once:
//   common_scripts\_load_common::main();
// A mode may skip a system by setting level.load_common_skip[ <name> ] = true
// first; the names are "minefields", "destructables", "elevator",
// "dynamic_world", "destructible", "pipes", "lanterns", "exploders", "fx".

main()
{
	// common_scripts/utility::isSP() decides singleplayer from the map name
	// (!string_starts_with( mapname, "mp_" )), which is wrong for every CoDOL
	// zm_* and coop_* map; it caches into level.isSP, so the supported answer
	// is to set it.  A dedicated server is never singleplayer.
	level.isSP = false;

	if ( isDefined( level._loadCommonStarted ) )
		return;
	level._loadCommonStarted = true;

	if ( !isDefined( level.load_common_skip ) )
		level.load_common_skip = [];

	level.createFX_enabled = ( getdvar( "createfx" ) != "" );

	common_scripts\utility::struct_class_init();

	if ( !isDefined( level.flag ) )
	{
		level.flag = [];
		level.flags_lock = [];
	}
	if ( !isDefined( level.createFXent ) )
		level.createFXent = [];

	// The code commands common_scripts reaches through level.func
	// (utility::noself_func / self_func): _destructible precaches and plays
	// its multiplayer animations through these three.
	if ( !isDefined( level.func ) )
		level.func = [];
	level.func[ "precacheMpAnim" ] = ::load_precacheMpAnim;
	level.func[ "scriptModelPlayAnim" ] = ::load_scriptModelPlayAnim;
	level.func[ "scriptModelClearAnim" ] = ::load_scriptModelClearAnim;

	started = "";
	// dodge this stuff for createfx tool.
	if ( !level.createFX_enabled )
	{
		if ( run( "minefields" ) )
		{
			thread maps\mp\_minefields::minefields();
			started += " minefields";
		}
		if ( run( "destructables" ) )
		{
			thread maps\mp\_destructables::init();
			started += " destructables";
		}
		if ( run( "elevator" ) )
		{
			thread common_scripts\_elevator::init();
			started += " elevator";
		}
		if ( run( "dynamic_world" ) )
		{
			thread common_scripts\_dynamic_world::init();
			level thread dynamic_world_players();
			started += " dynamic_world";
		}
		if ( run( "destructible" ) )
		{
			guard_destructible_anims();
			thread common_scripts\_destructible::init();
			started += " destructible";
		}
		if ( run( "pipes" ) )
		{
			thread common_scripts\_pipes::main();
			started += " pipes";
		}
	}

	if ( run( "lanterns" ) )
	{
		lanterns = getentarray( "lantern_glowFX_origin", "targetname" );
		for ( i = 0; i < lanterns.size; i++ )
			lanterns[ i ] thread lanterns();
	}

	if ( run( "exploders" ) )
	{
		setupExploders();
		started += " exploders";
	}

	if ( run( "fx" ) )
		thread common_scripts\_fx::initFX();

	if ( run( "exploders" ) )
	{
		// Do various things on triggers
		triggertypes = [];
		triggertypes[ 0 ] = "trigger_multiple";
		triggertypes[ 1 ] = "trigger_once";
		triggertypes[ 2 ] = "trigger_use";
		triggertypes[ 3 ] = "trigger_radius";
		triggertypes[ 4 ] = "trigger_lookat";
		triggertypes[ 5 ] = "trigger_damage";
		for ( p = 0; p < triggertypes.size; p++ )
		{
			triggers = getentarray( triggertypes[ p ], "classname" );
			for ( i = 0; i < triggers.size; i++ )
			{
				if ( isdefined( triggers[ i ].script_prefab_exploder ) )
					triggers[ i ].script_exploder = triggers[ i ].script_prefab_exploder;

				if ( isdefined( triggers[ i ].script_exploder ) )
					level thread exploder_load( triggers[ i ] );
			}
		}
	}

	destructibles = getEntArray( "destructible_vehicle", "targetname" ).size + getEntArray( "destructible_toy", "targetname" ).size;
	codolprintln( "CoDOL load_common: " + getDvar( "mapname" ) + " (" + getDvar( "sv_zoneType" ) + ") started" + started +
		"; " + destructibles + " destructibles, " + level.createFXent.size + " createfx/exploder entries" );
}

run( name )
{
	return !( isDefined( level.load_common_skip[ name ] ) && level.load_common_skip[ name ] );
}

// IW4 maps\mp\_load.gsc points level.func at the code functions themselves;
// a wrapper keeps the call shape noself_func/self_func expect.
load_precacheMpAnim( animName )
{
	precacheMpAnim( animName );
}

load_scriptModelPlayAnim( animName )
{
	self scriptModelPlayAnim( animName );
}

load_scriptModelClearAnim()
{
	self scriptModelClearAnim();
}

// common_scripts\_dynamic_world tracks movement through its touch triggers
// for every player it hears "connecting" for, which only the multiplayer
// _playerlogic raises.  A mode that raises "connected" alone gets the same
// tracker here.  (CoDOL: not in either IW4 loader.)
dynamic_world_players()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		if ( isDefined( player.touchTriggers ) )
			continue;
		player.touchTriggers = [];
		player thread common_scripts\_dynamic_world::movementTracker();
	}
}

exploder_load( trigger )
{
	level endon( "killexplodertridgers" + trigger.script_exploder );
	trigger waittill( "trigger" );
	if ( isdefined( trigger.script_chance ) && randomfloat( 1 ) > trigger.script_chance )
	{
		if ( isdefined( trigger.script_delay ) )
			wait trigger.script_delay;
		else
			wait 4;
		level thread exploder_load( trigger );
		return;
	}
	common_scripts\utility::exploder( trigger.script_exploder );
	level notify( "killexplodertridgers" + trigger.script_exploder );
}

setupExploders()
{
	// Hide exploder models.
	ents = getentarray( "script_brushmodel", "classname" );
	smodels = getentarray( "script_model", "classname" );
	for ( i = 0;i < smodels.size;i ++ )
		ents[ ents.size ] = smodels[ i ];

	for ( i = 0;i < ents.size;i ++ )
	{
		if ( isdefined( ents[ i ].script_prefab_exploder ) )
			ents[ i ].script_exploder = ents[ i ].script_prefab_exploder;

		if ( isdefined( ents[ i ].script_exploder ) )
		{
			if ( ( ents[ i ].model == "fx" ) && ( ( !isdefined( ents[ i ].targetname ) ) || ( ents[ i ].targetname != "exploderchunk" ) ) )
				ents[ i ] hide();
			else if ( ( isdefined( ents[ i ].targetname ) ) && ( ents[ i ].targetname == "exploder" ) )
			{
				ents[ i ] hide();
				ents[ i ] notsolid();
			}
			else if ( ( isdefined( ents[ i ].targetname ) ) && ( ents[ i ].targetname == "exploderchunk" ) )
			{
				ents[ i ] hide();
				ents[ i ] notsolid();
			}
		}
	}

	script_exploders = [];

	potentialExploders = getentarray( "script_brushmodel", "classname" );
	for ( i = 0;i < potentialExploders.size;i ++ )
	{
		if ( isdefined( potentialExploders[ i ].script_prefab_exploder ) )
			potentialExploders[ i ].script_exploder = potentialExploders[ i ].script_prefab_exploder;

		if ( isdefined( potentialExploders[ i ].script_exploder ) )
			script_exploders[ script_exploders.size ] = potentialExploders[ i ];
	}

	potentialExploders = getentarray( "script_model", "classname" );
	for ( i = 0;i < potentialExploders.size;i ++ )
	{
		if ( isdefined( potentialExploders[ i ].script_prefab_exploder ) )
			potentialExploders[ i ].script_exploder = potentialExploders[ i ].script_prefab_exploder;

		if ( isdefined( potentialExploders[ i ].script_exploder ) )
			script_exploders[ script_exploders.size ] = potentialExploders[ i ];
	}

	potentialExploders = getentarray( "item_health", "classname" );
	for ( i = 0;i < potentialExploders.size;i ++ )
	{
		if ( isdefined( potentialExploders[ i ].script_prefab_exploder ) )
			potentialExploders[ i ].script_exploder = potentialExploders[ i ].script_prefab_exploder;

		if ( isdefined( potentialExploders[ i ].script_exploder ) )
			script_exploders[ script_exploders.size ] = potentialExploders[ i ];
	}

	if ( !isdefined( level.createFXent ) )
		level.createFXent = [];

	acceptableTargetnames = [];
	acceptableTargetnames[ "exploderchunk visible" ] = true;
	acceptableTargetnames[ "exploderchunk" ] = true;
	acceptableTargetnames[ "exploder" ] = true;

	for ( i = 0; i < script_exploders.size; i ++ )
	{
		exploder = script_exploders[ i ];
		ent = common_scripts\utility::createExploder( exploder.script_fxid );
		ent.v = [];
		ent.v[ "origin" ] = exploder.origin;
		ent.v[ "angles" ] = exploder.angles;
		ent.v[ "delay" ] = exploder.script_delay;
		ent.v[ "firefx" ] = exploder.script_firefx;
		ent.v[ "firefxdelay" ] = exploder.script_firefxdelay;
		ent.v[ "firefxsound" ] = exploder.script_firefxsound;
		ent.v[ "firefxtimeout" ] = exploder.script_firefxtimeout;
		ent.v[ "earthquake" ] = exploder.script_earthquake;
		ent.v[ "damage" ] = exploder.script_damage;
		ent.v[ "damage_radius" ] = exploder.script_radius;
		ent.v[ "soundalias" ] = exploder.script_soundalias;
		ent.v[ "repeat" ] = exploder.script_repeat;
		ent.v[ "delay_min" ] = exploder.script_delay_min;
		ent.v[ "delay_max" ] = exploder.script_delay_max;
		ent.v[ "target" ] = exploder.target;
		ent.v[ "ender" ] = exploder.script_ender;
		ent.v[ "type" ] = "exploder";
		if ( !isdefined( exploder.script_fxid ) )
			ent.v[ "fxid" ] = "No FX";
		else
			ent.v[ "fxid" ] = exploder.script_fxid;
		ent.v[ "exploder" ] = exploder.script_exploder;

		if ( !isdefined( ent.v[ "delay" ] ) )
			ent.v[ "delay" ] = 0;

		// CoDOL: a target that is not an entity (a struct, or one the addon
		// entities did not bring) leaves the exploder's own angles, instead of
		// faulting on .origin of undefined and ending the load.
		if ( isdefined( exploder.target ) )
		{
			targetEnt = getent( ent.v[ "target" ], "targetname" );
			if ( isDefined( targetEnt ) )
				ent.v[ "angles" ] = vectortoangles( targetEnt.origin - ent.v[ "origin" ] );
		}

		// this basically determines if its a brush / model exploder or not
		if ( exploder.classname == "script_brushmodel" || isdefined( exploder.model ) )
		{
			ent.model = exploder;
			ent.model.disconnect_paths = exploder.script_disconnectpaths;
		}

		if ( isdefined( exploder.targetname ) && isdefined( acceptableTargetnames[ exploder.targetname ] ) )
			ent.v[ "exploder_type" ] = exploder.targetname;
		else
			ent.v[ "exploder_type" ] = "normal";

		ent common_scripts\_createfx::post_entity_creation_function();
	}
}

lanterns()
{
	if ( !isdefined( level._effect ) )
		level._effect = [];
	if ( !isdefined( level._effect[ "lantern_light" ] ) )
		level._effect[ "lantern_light" ] = loadfx( "props/glow_latern" );

	common_scripts\_fx::loopfx( "lantern_light", self.origin, 0.3, self.origin + ( 0, 0, 1 ) );
}

// _destructible_types reads an animated destructible's anims from
// level._destructible_preanims, which the MAP fills: every IW4 / CoDOL level
// script calls the matching common_scripts\_destructible_types_anim_*::main()
// before its loader (maps/mp/mp_crash_tropical.gsc: light_fluo_single and
// me_fanceil1_spin; mp_killhouse_sh.gsc: airconditioner).  The registration
// cannot live here: every %anim a compiled script names joins the server's
// animtree, and a map must name exactly its own (maps/mp/mp_crash_precache.gsc
// records the index shift an extra entry caused).  An unregistered anim is a
// fatal script error at load (get_precached_anim), so a level script that
// does not register one -- a mode's own copy of the map's level script --
// must not end the match: such a destructible is left a static prop and
// reported.  (CoDOL: not in either IW4 loader.)
guard_destructible_anims()
{
	ents = getEntArray( "destructible_toy", "targetname" );
	vehicles = getEntArray( "destructible_vehicle", "targetname" );
	for ( i = 0; i < vehicles.size; i++ )
		ents[ ents.size ] = vehicles[ i ];

	skipped = 0;
	missing = "";
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.destructible_type ) )
			continue;
		needed = destructible_needed_anims( e.destructible_type );
		for ( j = 0; j < needed.size; j++ )
		{
			if ( isDefined( level._destructible_preanims ) && isDefined( level._destructible_preanims[ needed[ j ] ] ) )
				continue;
			e.targetname = "destructible_unregistered";
			skipped++;
			if ( !isSubStr( missing, needed[ j ] ) )
				missing += " " + needed[ j ];
			break;
		}
	}
	if ( skipped > 0 )
		codolprintln( "CoDOL load_common: " + skipped + " destructibles left static; the level script registers no anim" + missing );
}

// The anims each animated destructible type of _destructible_types.gsc asks
// get_precached_anim for (its toy_* / vehicle_* builders).
destructible_needed_anims( type )
{
	a = [];
	switch ( type )
	{
	case "toy_generator":
		a[ 0 ] = "generator_explode";
		break;
	case "toy_generator_on":
		a[ 0 ] = "generator_vibration";
		a[ 1 ] = "generator_explode";
		a[ 2 ] = "generator_explode_02";
		a[ 3 ] = "generator_explode_03";
		break;
	case "toy_airconditioner":
		a[ 0 ] = "ex_airconditioner_fan";
		break;
	case "toy_ceiling_fan":
		a[ 0 ] = "me_fanceil1_spin";
		a[ 1 ] = "me_fanceil1_spin_stop";
		break;
	case "toy_wall_fan":
		a[ 0 ] = "wall_fan_rotate";
		a[ 1 ] = "wall_fan_stop";
		break;
	case "toy_light_ceiling_fluorescent":
		a[ 0 ] = "light_fluorescent_swing";
		a[ 1 ] = "light_fluorescent_swing_02";
		a[ 2 ] = "light_fluorescent_null";
		break;
	case "toy_light_ceiling_fluorescent_spotlight":
		a[ 0 ] = "light_fluorescent_swing";
		break;
	case "toy_light_ceiling_fluorescent_single":
		a[ 0 ] = "light_fluorescent_single_swing";
		a[ 1 ] = "light_fluorescent_single_swing_02";
		a[ 2 ] = "light_fluorescent_single_swing_03";
		a[ 3 ] = "light_fluorescent_single_null";
		break;
	case "toy_light_ceiling_fluorescent_single_spotlight":
		a[ 0 ] = "light_fluorescent_single_swing_02";
		break;
	case "toy_locker_double":
		a[ 0 ] = "locker_broken_both_doors_1";
		break;
	case "toy_chicken":
	case "toy_chicken_white":
	case "toy_chicken_black_white":
		a[ 0 ] = "chicken_cage_loop_01";
		a[ 1 ] = "chicken_cage_death";
		break;
	case "vehicle_motorcycle_01":
		a[ 0 ] = "vehicle_motorcycle_destroy_01";
		break;
	case "vehicle_motorcycle_02":
		a[ 0 ] = "vehicle_motorcycle_destroy_02";
		break;
	default:
		if ( isSubStr( type, "snowmobile" ) )
			a[ 0 ] = "vehicle_snowmobile_destroy_01";
		break;
	}
	return a;
}
