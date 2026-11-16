// CoDOL overlay adapted from IW4 maps/mp/_load.gsc.
// Mission filtering follows maps/_specialops_code.gsc non-specialops rules.
#include common_scripts\utility;
#include common_scripts\_fx;
#include maps\mp\_utility;

main()
{
	// common_scripts/utility::isSP() answers "is this singleplayer?" from the
	// map name alone -- level.isSP = !string_starts_with( getdvar("mapname"),
	// "mp_" ), utility.gsc:2080-2086 -- because IW4 had no multiplayer map that
	// was not mp_*.  CoDOL does: zm_zombie_training and the co-op zones are not
	// mp_*, so every isSP() caller below takes the campaign branch, and the
	// first one reached -- common_scripts\_dynamic_world::player_init(), out of
	// the init() threaded below -- walks level.players before
	// _spawnlogic::init() has created it ("type undefined is not an object",
	// _dynamic_world.gsc:119).  isSP() caches into level.isSP and only computes
	// it when undefined, so the supported way to answer it is to answer it: a
	// dedicated match server is never singleplayer, whatever the map is called.
	//
	// _gamelogic::Callback_StartGameType also sets this before calling us, but
	// that alone is not enough.  An IW4 level script's main() calls
	// maps\mp\_load::main() itself (mp_killhouse_sh.gsc:13,
	// mp_fav_tropical.gsc:23) and the level script runs BEFORE
	// Callback_StartGameType.  zm reached _load through the later path only
	// because no maps/mp/zm_zombie_training level script exists yet; supplying
	// one -- the next step for zm -- would bring the crash straight back
	// through the earlier path.  Answer it here too, at the choke point every
	// entry order passes through, ahead of the _loadStarted guard.
	level.isSP = false;

	if ( isDefined( level._loadStarted ) )
		return;

	level._loadStarted = true;
	level thread codolFogDvar();
	codolRemoveMissionEntities();

	level.createFX_enabled = ( getdvar( "createfx" ) != "" );

	struct_class_init();

	initGameFlags();
	initLevelFlags();

	level.generic_index = 0;
	// flag_struct is used as a placeholder when a flag is set without an entity

	level.flag_struct = spawnstruct();
	level.flag_struct assign_unique_id();
	if ( !isdefined( level.flag ) )
	{
		level.flag = [];
		level.flags_lock = [];
	}

	level.requiredMapAspectRatio = getDvarFloat( "scr_RequiredMapAspectratio", 1 );	
	level.createClientFontString_func = maps\mp\gametypes\_hud_util::createFontString;
	level.HUDsetPoint_func = maps\mp\gametypes\_hud_util::setPoint;
	level.leaderDialogOnPlayer_func	= maps\mp\_utility::leaderDialogOnPlayer;
	
	thread maps\mp\gametypes\_tweakables::init();
	
	
	if ( !isdefined( level.func ) )
		level.func = [];
	level.func[ "precacheMpAnim" ] = ::precacheMpAnim;
	level.func[ "scriptModelPlayAnim" ] = ::scriptModelPlayAnim;
	level.func[ "scriptModelClearAnim" ] = ::scriptModelClearAnim;
	
	// dodge this stuff for createfx tool.
	if( ! level.createFX_enabled )
	{
		thread maps\mp\_minefields::minefields();
		thread maps\mp\_radiation::radiation();
		thread maps\mp\_shutter::main();
		thread maps\mp\_destructables::init();
		thread common_scripts\_elevator::init();
		thread common_scripts\_dynamic_world::init();
		thread common_scripts\_destructible::init();
		thread common_scripts\_pipes::main();
	}

	if ( getMapCustom( "thermal" ) == "invert" )
	{
		game["thermal_vision"] = "thermal_snowlevel_mp";
		SetThermalBodyMaterial( "thermalbody_snowlevel" );
	}
	else
	{
		game["thermal_vision"] = "thermal_mp";
	}
	
	VisionSetNaked( getDvar( "mapname" ), 0 );
	VisionSetNight( "default_night_mp" );
	VisionSetMissilecam( "missilecam" );
	VisionSetThermal( game[ "thermal_vision" ] );
	VisionSetPain( getDvar( "mapname" ) );

	lanterns = getentarray("lantern_glowFX_origin","targetname");
	for( i = 0 ; i < lanterns.size ; i++ )
		lanterns[i] thread lanterns();

	maps\mp\_art::main();
	/#
	thread common_scripts\_painter::main("painter_mp");
	#/

	setupExploders();

	thread common_scripts\_fx::initFX();
	if ( level.createFX_enabled )
		maps\mp\_createfx::createfx();

	if ( getdvar( "r_reflectionProbeGenerate" ) == "1" )
	{
		maps\mp\gametypes\_spawnlogic::setMapCenterForReflections();
		maps\mp\_global_fx::main();
		level waittill( "eternity" );
	}

	thread maps\mp\_global_fx::main();
	
	// Do various things on triggers
	for ( p = 0;p < 6;p ++ )
	{
		switch( p )
		{
			case 0:
				triggertype = "trigger_multiple";
				break;

			case 1:
				triggertype = "trigger_once";
				break;

			case 2:
				triggertype = "trigger_use";
				break;
				
			case 3:	
				triggertype = "trigger_radius";
				break;
			
			case 4:	
				triggertype = "trigger_lookat";
				break;

			default:
				assert( p == 5 );
				triggertype = "trigger_damage";
				break;
		}

		triggers = getentarray( triggertype, "classname" );

		for ( i = 0;i < triggers.size;i ++ )
		{
			if( isdefined( triggers[ i ].script_prefab_exploder) )
				triggers[i].script_exploder = triggers[ i ].script_prefab_exploder;

			if( isdefined( triggers[ i ].script_exploder) )
				level thread maps\mp\_load::exploder_load( triggers[ i ] );
		}
	}

	hurtTriggers = getentarray( "trigger_hurt", "classname" );

	foreach ( hurtTrigger in hurtTriggers )
	{
		hurtTrigger thread hurtPlayersThink();
	}

	thread maps\mp\_animatedmodels::main();
	
	// auto-sentry
	level.func[ "damagefeedback" ] = maps\mp\gametypes\_damagefeedback::updateDamageFeedback;
	level.func[ "setTeamHeadIcon" ] = maps\mp\_entityheadicons::setTeamHeadIcon;
	level.laserOn_func = ::laserOn;
	level.laserOff_func = ::laserOff;

	 // defaults
	setDvar( "sm_sunShadowScale", 1 );
	setDvar( "r_specularcolorscale", 2.5 );
	setDvar( "r_diffusecolorscale", 1 );
	setDvar( "r_lightGridEnableTweaks", 0 );
	setDvar( "r_lightGridIntensity", 1 );
	setDvar( "r_lightGridContrast", 0 );
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
	exploder( trigger.script_exploder );
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
				//if ( isdefined( ents[ i ].script_disconnectpaths ) )
					//ents[ i ] connectpaths();
			}
			else if ( ( isdefined( ents[ i ].targetname ) ) && ( ents[ i ].targetname == "exploderchunk" ) )
			{
				ents[ i ] hide();
				ents[ i ] notsolid();
				//if ( isdefined( ents[ i ].spawnflags ) && ( ents[ i ].spawnflags & 1 ) )
					//ents[ i ] connectpaths();
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
		ent = createExploder( exploder.script_fxid );
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
// 		ent.v[ "worldfx" ] = true;
		if ( !isdefined( exploder.script_fxid ) )
			ent.v[ "fxid" ] = "No FX";
		else
			ent.v[ "fxid" ] = exploder.script_fxid;
		ent.v[ "exploder" ] = exploder.script_exploder;
		assertEx( isdefined( exploder.script_exploder ), "Exploder at origin " + exploder.origin + " has no script_exploder" );

		if ( !isdefined( ent.v[ "delay" ] ) )
			ent.v[ "delay" ] = 0;
			
		if ( isdefined( exploder.target ) )
		{
			org = getent( ent.v[ "target" ], "targetname" ).origin;
			ent.v[ "angles" ] = vectortoangles( org - ent.v[ "origin" ] );
// 			forward = anglestoforward( angles );
// 			up = anglestoup( angles );
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
	if (!isdefined(level._effect["lantern_light"]))
		level._effect["lantern_light"]	= loadfx("props/glow_latern");
		
	loopfx("lantern_light", self.origin, 0.3, self.origin + (0,0,1));
}


hurtPlayersThink()
{
	level endon ( "game_ended" );
	
	wait ( randomFloat( 1.0 ) );
	
	for ( ;; )
	{
		foreach ( player in level.players )
		{
			if ( player isTouching( self ) && isReallyAlive( player ) )
				player _suicide();
		}
		
		wait ( 0.5 );
	}	
}


// Shared IW4 maps/_load calls _specialops::specialops_remove_unused before
// map initialization. CoDOL MP shares geometry with named co-op missions;
// the MP and Survival loader must apply the same non-specialops selection.
codolRemoveMissionEntities()
{
    category = getDvar( "codol_zoneType" );
    if ( category != "mp" && category != "survival" )
        return;
    // Survival always uses the MP world, even if a previous campaign left
    // its legacy specialops dvar set. Co-op owns its separate mission filter.
    if ( category == "mp" && getDvar( "specialops" ) == "1" )
        return;

    entities = getentarray();
    removed = 0;
    for ( i = 0; i < entities.size; i++ )
    {
        ent = entities[i];
        remove = false;
        if ( isDefined( ent.script_specialops ) )
        {
            assertEx( ent.script_specialops == 0 || ent.script_specialops == 1,
                "script_specialops must be zero or one" );
            remove = ent.script_specialops == 1;
        }
        // A named mission entity has no matching mission in normal MP.
        if ( isDefined( ent.script_specialopsname ) )
            remove = true;
        if ( remove )
        {
            ent delete();
            removed++;
        }
    }
    printLn( "CoDOL " + category + " mission cleanup: removed " + removed + " mission-only entities" );
}

// Sends each player the map's fog switch, C1 dvar "1464" (r_fog, default 1; the 1.1.1.13 client knows its dvars by
// number only). A level script sets level.codolFog = 0 before calling main() to turn fog off; every other map sends
// 1, so the setting does not carry over to the next map.
codolFogDvar()
{
	fog = 1;
	if ( isDefined( level.codolFog ) )
		fog = level.codolFog;
	for ( ;; )
	{
		level waittill( "connected", player );
		player setClientDvar( "1464", fog );
	}
}
