/*
	scripts/codol/maps/mp/_dogs.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\_dogs.gsc

	T5 function names, structure, comments and behaviour are kept.  What
	changed for CoDOL, and why:

	  KILLSTREAK REGISTRATION
	  - T5's initKillstreak() is removed.  It called
	    maps\mp\gametypes\_hardpoints::registerKillstreak* and
	    maps\mp\gametypes\_tweakables::getTweakableValue; this tree has neither
	    _hardpoints nor _tweakables.  Registration here is
	    "level.killStreakFuncs[ "dogs_mp" ] = ::useKillstreakDogs;" and lives in
	    maps\mp\killstreaks\_killstreaks.gsc (see _autosentry::init for the
	    pattern), so init() below only sets up state and precaches.
	  - useKillstreakDogs takes the local dispatch signature ( lifeId ) instead
	    of T5's ( hardpointType ); _killstreaks.gsc calls
	    "self [[ level.killstreakFuncs[streakName] ]]( lifeId )".  The T5 uses
	    of hardpointType are the literal "dogs_mp".
	  - maps\mp\_killstreakrules::isKillstreakAllowed / killstreakStart /
	    killstreakStop are dropped: _killstreakrules does not exist here and
	    the one local port of it (maps\mp\killstreaks\_rcbomb_rules) asserts
	    name == "rcbomb_mp", so it is not a dog counterpart.  T5's "dog" rule
	    (createRule( "dog", 1, 1 ): one dog set in the whole match at a time)
	    is kept in useKillstreakDogs as the level.dogs check: level.dogs is
	    defined exactly from dog_manager_spawn_dogs until the alive tracker's
	    dog_manager_delete_dogs, which is where T5 calls killstreakStop.
	    Without it a second call reset level.dogs under the first set and
	    started a second alive tracker; whichever deleted first left
	    level.dogs undefined and the other died at "size cannot be applied to
	    undefined" in dog_manager_delete_dogs.
	  - maps\mp\gametypes\_battlechatter_mp::onKillstreakUsed and
	    maps\mp\gametypes\_hardpoints::playKillstreakStartDialog are dropped:
	    no counterpart, and _killstreaks::usedKillstreak already plays the
	    team/enemy inbound leader dialog for every reward after the use
	    function returns true.
	  - maps\mp\gametypes\_persistence::statAdd( "DOGS_USED", 1, false ) and
	    maps\mp\gametypes\_globallogic_score::incItemStatByReference are
	    dropped (no DOGS_USED playerdata field in C1, no _globallogic_score
	    module).  maps\mp\_matchdata::logKillstreakEvent is used instead -
	    that is what every other CoDOL killstreak does (_ac130, _autosentry,
	    _emp).  _killstreaks::usedKillstreak also does the per-player award
	    stat via maps\mp\_awards::getKillstreakAwardRef.
	  - level.globalKillstreaksCalled++ is dropped: nothing in this tree
	    defines or reads it.
	  - The T5 "allowHardpointStreakAfterDeath" tweakable choice between
	    self.deathCount and self.pers["hardPointItemDeathCount"+type] is
	    dropped; neither field exists here.  The dog's requiredDeathCount is
	    the reward's lifeId, which is this tree's per-life identity.

	  ASSETS (verified against obj/dog-asset-string-inventory.json)
	  - Models: T5 "german_shepherd" / "german_shepherd_black" are NOT in the
	    C1 zones.  The only dog model string the client ships is
	    "german_sheperd_dog" (T5 SP spelling).  There is no black variant, so
	    the friendly and enemy models are the same model.  Both names are
	    produced by dog_model_friendly()/dog_model_enemy() below and stored in
	    level.friendlyDogModel / level.enemyDogModel, which is the single place
	    to re-point them.
	  - Weapon: T5 "dog_bite_mp" is NOT in the C1 zones.  C1 ships a WeaponDef
	    named "dog_bite" (obj/claude-workers/weapon-gates-fields.json), so the
	    bite weapon is level.dogBiteWeapon = dog_bite_weapon() = "dog_bite".
	  - Shellshock "dog_bite" is unchanged: C1 ships shock/dog_bite.shock.

	  OTHER LOCAL FIXUPS
	  - getclosest() has no counterpart here (T5 defines it in maps\mp\_utility;
	    the CoDOL maps\mp\_utility does not), so dog_get_exit_node uses the
	    local dog_get_closest_node() - the same body, carried in.
	  - bbPrint( "mpdogspawnused: ..." ) is dropped: Blackbox telemetry has no
	    counterpart in this server.
	  - dog_create_spawn_influencer() is T5's _spawning::create_dog_influencers
	    on the native attached-sphere influencer (see the function).
	  - maps\mp\gametypes\_weapons::monitor_dog_special_grenades() has no
	    counterpart in the local _weapons, so its T5 body is carried here
	    (monitor_dog_special_grenades below) next to flash_dogs().
	  - T5 maps\mp\gametypes\_globallogic_actor (Callback_ActorDamage /
	    Callback_ActorKilled) is carried here too: dogs are the only MP actor.
	    It is where friendly fire, the owner and same-owner dogs are refused,
	    hit markers are drawn and the "dogkill" XP is given.
	  - GetDvarFloat( #"name" ) rewritten to getFloatProperty( "name", default )
	    - the T5 #"..." canonical-hash token does not exist in this VM, and
	    these two spawn dvars are not registered here.
	  - The entire /# devgui_dog_think ... #/ developer block is dropped: it
	    needs maps\mp\gametypes\_dev, maps\mp\gametypes\_supplydrop,
	    getHostPlayer() and the CameraActivate/CameraSetPosition/CameraSetLookAt
	    methods, none of which exist in this tree.  No gameplay is in it.
	  - dog_dvar_update() additionally registers dog_turn90_angle and
	    dog_turn180_angle with their T5 code defaults, because
	    maps\mp\animscripts\dog_turn reads them with GetDvarFloat and this
	    engine has no code-side default for them.
*/

#include maps\mp\_utility;
#include common_scripts\utility;

// CoDOL: T5 leaves #using_animtree("dog") commented out in the dog animscripts
// because its actor code loads self.animTree = "dog.atr" at runtime.  This
// server has no runtime tree loader: the native actor (codol_actor_lifecycle.cpp
// CODOL_ActorPrepareAnimation) looks the "dog" tree up in the script animtree
// registry, which only holds trees a compiled script declared.  Without this
// line animtrees/dog.atr never mounts, every spawnactor() yields undefined and
// the 2026-09-16 call-in died at "undefined is not an entity" (line 422).
#using_animtree("dog");

// The one place the dog model name lives.  Written as functions rather than
// bare literals so the aitype/character scripts can reach the same string
// before _dogs::init has run (precache ordering).
dog_model_friendly()
{
	return "german_sheperd_dog";
}

dog_model_enemy()
{
	// C1 ships no black shepherd; friendly and enemy share the one model.
	return "german_sheperd_dog";
}

dog_bite_weapon()
{
	return "dog_bite";
}

init()
{
	level.friendlyDogModel = dog_model_friendly();
	level.enemyDogModel = dog_model_enemy();
	level.dogBiteWeapon = dog_bite_weapon();

	precacheModel(level.friendlyDogModel);
	if ( level.enemyDogModel != level.friendlyDogModel )
		precacheModel(level.enemyDogModel);
	precacheItem(level.dogBiteWeapon);
	precacheShellshock("dog_bite");

	level.maxDogsAttackingPerPlayer = 2;
	level.spawnTimeWaitMin = 2;
	level.spawnTimeWaitMax = 5;

	level.no_dogs = false;

	init_node_arrays();

	// CoDOL: a map without path nodes no longer disables the dogs here; the
	// first call-in builds substitute nodes from the bot waypoints or the
	// spawn points (dog_ensure_nodes), and refuses only if neither exists.

	regenTime = 5;
	level.dogHealth_RegularRegenDelay = regenTime * 1000;
	level.dogHealthRegenDisabled = (level.dogHealth_RegularRegenDelay <= 0);

	dog_dvar_update();
	thread dog_dvar_updater();
	thread dog_usage_init();

	thread init_all_preexisting_dogs();

	// T5 _callbacksetup::SetDefaultCallbacks installs
	// _globallogic_actor::Callback_ActorDamage / Callback_ActorKilled for every
	// MP actor.  Dogs are the only MP actor, so the ported pair lives here and
	// is installed with the dog subsystem (see Callback_ActorDamage below).
	level.callbackActorDamage = ::Callback_ActorDamage;
	level.callbackActorKilled = ::Callback_ActorKilled;

	// CoDOL dev trigger (developer servers only; see dog_test_watcher).
	thread dog_test_watcher();
	thread maps\mp\animscripts\dog_attack_log::test_watcher();

//	create_dogs();
}

// `rcon set codol_dogsTest 1` calls the dogs in for the first live player;
// "2" also runs the mp_nuked decoy check below.  Other verbs are in
// dog_test_verb().  Test hooks can damage and flash entities, so, like
// _codol_bots::watchKillstreakTestDvar, they only run with developer 1.
dog_test_watcher()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	// no game_ended endon: "state" must still answer while the dogs leave
	// after the match ends.

	setDvar( "codol_dogsTest", "0" );

	for ( ;; )
	{
		wait 1;

		request = getDvar( "codol_dogsTest" );
		if ( request == "" || request == "0" )
			continue;
		setDvar( "codol_dogsTest", "0" );

		tokens = strTok( request, " " );
		if ( int( tokens[0] ) <= 0 )
		{
			dog_test_verb( tokens );
			continue;
		}

		test = int( tokens[0] );

		// `rcon set codol_dogsTest 2` reproduces the mp_nuked defect on any map:
		// it plants a script_model whose targetname is "dog_spawner" so that
		// dog_manager_spawn_dogs' getent() finds a NON-spawner, then calls the
		// dogs in. The native isactorspawner()/spawnactor() must both refuse it
		// with a named "CoDOL dogs:" warning and return undefined, the manager
		// must fall back to spawnactorspawner(), and no script error may raise.
		if ( test >= 2 )
		{
			decoy = spawn( "script_model", (0,0,0) );
			decoy.targetname = "dog_spawner";
			decoy setmodel( "tag_origin" );
			print( "CoDOL dogs: codol_dogsTest 2: planted a non-spawner entity with targetname \"dog_spawner\"\n" );
			probe = decoy isactorspawner();
			print( "CoDOL dogs: codol_dogsTest 2: isactorspawner() returned " + probe + "\n" );
			nodog = decoy spawnactor();
			if ( isdefined( nodog ) )
				print( "CoDOL dogs: codol_dogsTest 2: FAIL - spawnactor() on a non-spawner returned an entity\n" );
			else
				print( "CoDOL dogs: codol_dogsTest 2: spawnactor() on a non-spawner returned undefined, as required\n" );
		}

		caller = undefined;
		foreach ( player in level.players )
		{
			if ( isDefined( player.team ) && player.team != "spectator" && isAlive( player ) )
			{
				caller = player;
				break;
			}
		}

		if ( !isDefined( caller ) )
		{
			print( "CoDOL dogs: codol_dogsTest: no live player to call the dogs for\n" );
			continue;
		}

		print( "CoDOL dogs: codol_dogsTest: calling dogs for " + caller.name + " (" + caller.team + ")\n" );
		caller thread useKillstreakDogs( 0 );
	}
}

// Developer verbs for codol_dogsTest (dog_test_watcher gates developer 1):
//   "call <client>"         useKillstreakDogs for that client number
//   "state"                 every dog: health, team, owner, enemy, flash
//   "hurt <who> <damage>"   the first live dog takes <damage> of rifle fire
//                           from <who>: owner, mate (owner's teammate),
//                           enemy or world; reports health before/after
//   "flash <who>"           a 1-point flash_grenade_mp splash from <who>,
//                           then the dog's flash strength
//   "nodes <team>"          the dog spawn nodes for a team
//   "spawnweight"           the dog spawn influencer on each team's spawns
//   "goto x y z gx gy gz"   place the first dog and send it somewhere
//   "embed x y z"           put the first dog inside solid
//   "where"                 the first dog's origin
//   (codol_dogsNoPathNodes 1/2 before a map load: no path nodes, use bot
//   waypoints / spawn points)
//   "leave"                 make_all_dogs_leave()
//   "endgame"               _gamelogic::forceEnd(), to watch the dogs leave
dog_test_verb( tokens )
{
	verb = tokens[0];

	if ( verb == "call" && tokens.size > 1 )
	{
		foreach ( player in level.players )
			if ( player getEntityNumber() == int( tokens[1] ) )
			{
				print( "CoDOL dogs test: call for " + player.name + " (" + player.team + ")\n" );
				used = player useKillstreakDogs( 0 );
				print( "CoDOL dogs test: useKillstreakDogs returned " + used + "\n" );
			}
		return;
	}

	if ( verb == "leave" )
	{
		print( "CoDOL dogs test: leave\n" );
		make_all_dogs_leave();
		return;
	}

	if ( verb == "spawnweight" )
	{
		// The dog influencer's weight on each team's spawn candidates: the
		// point nearest a live dog, and the lowest weight of all spawn points.
		if ( !isDefined( level.dogs ) )
		{
			print( "CoDOL dogs test: spawnweight: no dogs\n" );
			return;
		}
		teams = [];
		teams[0] = "allies";
		teams[1] = "axis";
		foreach ( t in teams )
		{
			worst = 0;
			worstAt = "none";
			foreach ( spawnpoint in level.spawnpoints )
			{
				w = codol_rcSpawnWeight( spawnpoint.origin, t );
				if ( w < worst )
				{
					worst = w;
					worstAt = "" + spawnpoint.origin;
				}
			}
			atDog = "n/a";
			foreach ( dog in level.dogs )
				if ( isDefined( dog ) && isAlive( dog ) )
				{
					atDog = codol_rcSpawnWeight( dog.origin, t );
					break;
				}
			print( "CoDOL dogs test: spawnweight " + t + ": at a dog " + atDog + ", worst spawn point " + worst + " at " + worstAt + "\n" );
		}
		return;
	}

	// "goto x y z gx gy gz": the first live dog, ignoring enemies, is placed
	// at (x y z) and sent to (gx gy gz) -- the wall-hop check.
	if ( verb == "goto" && tokens.size > 6 )
	{
		dog = dog_test_first_dog();
		if ( !isDefined( dog ) )
			return;
		dog.ignoreall = true;
		dog notify( "attacking" );	// ends dog_patrol without restarting it
		dog forceteleport( ( float( tokens[1] ), float( tokens[2] ), float( tokens[3] ) ), ( 0, 90, 0 ) );
		dog setgoalpos( ( float( tokens[4] ), float( tokens[5] ), float( tokens[6] ) ) );
		print( "CoDOL dogs test: goto: dog " + dog getEntityNumber() + " at " + dog.origin + " -> (" + tokens[4] + " " + tokens[5] + " " + tokens[6] + ")\n" );
		return;
	}

	// "embed x y z": the first live dog is put inside solid at (x y z)
	// (teleport, unlike forceteleport, does not lift it out).
	if ( verb == "embed" && tokens.size > 3 )
	{
		dog = dog_test_first_dog();
		if ( !isDefined( dog ) )
			return;
		dog teleport( ( float( tokens[1] ), float( tokens[2] ), float( tokens[3] ) ) );
		print( "CoDOL dogs test: embed: dog " + dog getEntityNumber() + " at " + dog.origin + "\n" );
		return;
	}

	if ( verb == "where" )
	{
		dog = dog_test_first_dog();
		if ( isDefined( dog ) )
			print( "CoDOL dogs test: where: dog " + dog getEntityNumber() + " at " + dog.origin + "\n" );
		return;
	}

	if ( verb == "nodes" && tokens.size > 1 )
	{
		nodes = dog_spawn_nodes_for_team( tokens[1] );
		print( "CoDOL dogs test: " + nodes.size + " spawn nodes for " + tokens[1] + " (authored " + level.dog_spawn_nodes_authored + ")\n" );
		foreach ( node in nodes )
			print( "CoDOL dogs test:   node " + node.origin + "\n" );
		print( "CoDOL dogs test: " + level.dogexitnodes.size + " exit nodes\n" );
		return;
	}

	if ( verb == "endgame" )
	{
		print( "CoDOL dogs test: endgame (_gamelogic::forceEnd)\n" );
		level thread maps\mp\gametypes\_gamelogic::forceEnd();
		return;
	}

	if ( verb == "state" )
	{
		if ( !isDefined( level.dogs ) )
		{
			print( "CoDOL dogs test: state: no dogs (level.dogs undefined)\n" );
			return;
		}
		print( "CoDOL dogs test: state: " + level.dogs.size + " dog slots\n" );
		for ( i = 0; i < level.dogs.size; i++ )
		{
			dog = level.dogs[i];
			if ( !isDefined( dog ) )
			{
				print( "CoDOL dogs test:   #" + i + " gone\n" );
				continue;
			}
			owner = "none";
			if ( isDefined( dog.script_owner ) )
				owner = dog.script_owner.name + "/" + dog.script_owner.team + " (kills " + dog.script_owner.pers["kills"] + ", streak " + dog.script_owner.pers["cur_kill_streak"] + ")";
			enemy = "none";
			if ( isDefined( dog.enemy ) )
			{
				enemy = "ent " + dog.enemy getEntityNumber();
				if ( isPlayer( dog.enemy ) )
					enemy = dog.enemy.name + "/" + dog.enemy.team;
			}
			print( "CoDOL dogs test:   #" + i + " ent " + dog getEntityNumber() + " alive " + isAlive( dog ) + " health " + dog.health +
				" team " + dog.aiteam + " owner " + owner + " enemy " + enemy + " flash " + dog getFlashBangedStrength() + " at " + dog.origin + "\n" );
		}
		return;
	}

	if ( ( verb == "hurt" || verb == "flash" ) && tokens.size > 1 )
	{
		dog = undefined;
		if ( isDefined( level.dogs ) )
			for ( i = 0; i < level.dogs.size && !isDefined( dog ); i++ )
				if ( isDefined( level.dogs[i] ) && isAlive( level.dogs[i] ) )
					dog = level.dogs[i];
		if ( !isDefined( dog ) )
		{
			print( "CoDOL dogs test: " + verb + ": no live dog\n" );
			return;
		}

		who = tokens[1];
		attacker = undefined;
		if ( who == "owner" )
			attacker = dog.script_owner;
		else if ( who != "world" )
		{
			foreach ( player in level.players )
			{
				if ( !isAlive( player ) || isDefined( attacker ) )
					continue;
				if ( isDefined( dog.script_owner ) && player == dog.script_owner )
					continue;
				if ( who == "mate" && level.teamBased && player.team == dog.aiteam )
					attacker = player;
				else if ( who == "enemy" && ( !level.teamBased || player.team != dog.aiteam ) )
					attacker = player;
			}
		}
		if ( who != "world" && !isDefined( attacker ) )
		{
			print( "CoDOL dogs test: " + verb + ": no live " + who + " attacker\n" );
			return;
		}

		before = dog.health;
		source = dog.origin + ( 0, 0, 30 );
		name = "world";
		if ( isDefined( attacker ) )
		{
			source = attacker.origin + ( 0, 0, 50 );
			name = attacker.name + "/" + attacker.team;
		}

		if ( verb == "hurt" )
		{
			damage = 25;
			if ( tokens.size > 2 )
				damage = int( tokens[2] );
			if ( isDefined( attacker ) )
				dog doDamage( damage, source, attacker, attacker, 0, "MOD_RIFLE_BULLET" );
			else
				dog doDamage( damage, source );
		}
		else
		{
			dog doDamage( 1, source, attacker, attacker, 0, "MOD_GRENADE_SPLASH", 0, "flash_grenade_mp" );
		}

		wait 0.1;
		after = "dead";
		if ( isDefined( dog ) && isAlive( dog ) )
			after = dog.health;
		flash = "n/a";
		if ( isDefined( dog ) && isAlive( dog ) )
			flash = dog getFlashBangedStrength();
		print( "CoDOL dogs test: " + verb + " by " + name + " (FF " + level.friendlyfire + ", hardcore " + level.hardcoreMode + "): health " + before + " -> " + after + ", flash " + flash + "\n" );
		return;
	}

	print( "CoDOL dogs test: unknown request '" + verb + "'\n" );
}

dog_test_first_dog()
{
	if ( isDefined( level.dogs ) )
		foreach ( dog in level.dogs )
			if ( isDefined( dog ) && isAlive( dog ) )
				return dog;
	print( "CoDOL dogs test: no live dog\n" );
	return undefined;
}

useKillstreakDogs(lifeId)
{
	// T5 _killstreakrules "dog" rule: one set at a time (see header).
	if ( isdefined( level.dogs ) )
	{
		self iPrintLnBold( &"KILLSTREAKS_DOGS_NOT_AVAILABLE" );
		return false;
	}

	// CoDOL: T5 returns true here and dog_manager_spawn_dogs then quietly
	// spawns nothing on a map without path nodes or a dog spawner, which spends
	// the reward on no dogs.  Resolve the spawner now and refuse instead, so
	// the player keeps the reward, as the one-set rule above does.
	if ( level.no_dogs || !dog_ensure_nodes() || !dog_acquire_spawner() )
	{
		self iPrintLnBold( &"KILLSTREAKS_DOGS_NOT_AVAILABLE" );
		return false;
	}

	self notify( "called_in_the_dogs" );
	level notify( "called_in_the_dogs" );
	team = self.team;
	otherTeam = level.otherTeam[team];

	self maps\mp\_matchdata::logKillstreakEvent( "dogs", self.origin );

	ownerDeathCount = lifeId;

	self thread maps\mp\_dogs::dog_manager_spawn_dogs( team, otherTeam, ownerDeathCount );
	self thread playerDisconnectWatcher();

	return true;
}

playerDisconnectWatcher()
{
	level endon( "dogs done" );
	level endon( "dogs leaving" );

	self waittill( "disconnect" );

	level notify( "dogs leaving" );
}

pick_random_nodes( from, count )
{
	to = [];

	if ( from.size < count )
	{
		to = from;
	}
	else
	{
		for ( i = 0; i < count; i++ )
		{
			to[i] = from[randomInt(from.size)];
		}
	}

	return to;
}

init_node_arrays()
{
	nodes = getallnodes();

	// Developer test hook: behave as on a map without path nodes
	// (codol_dogsNoPathNodes 1: substitute bot waypoints; 2: spawn points).
	if ( getDvarInt( "developer" ) > 0 && getDvarInt( "codol_dogsNoPathNodes" ) > 0 )
		nodes = [];

	pathnodes = [];

	for ( i = 0; i < nodes.size; i++ )
	{
		// anything with a scriptworthy is automatically a non-patrol node
		if ( isdefined(nodes[i].script_noteworthy) /* || nodes[i].script_noteworthy != "" */)
			continue;

		if ( isdefined(nodes[i].targetname) && nodes[i].targetname == "traverse" )
			continue;

		// CoDOL: every node this VM stores comes from the map entity string;
		// one without an origin key can neither be teleported to nor pathed to.
		if ( !isdefined( nodes[i].origin ) )
			continue;

		pathnodes[pathnodes.size] = nodes[i];
	}

	level.dog_pathnodes = pathnodes;

	if ( pathnodes.size == 0 )
	{
		level.no_pathnodes = true;
	}
	else
	{
		level.no_pathnodes = false;
	}

	level.patrolnodes = [];

	level.patrolnodes = pick_random_nodes( pathnodes, 200 );

	level.dogspawnnodes = [];
	level.dogspawnnodes = getnodearray( "spawn", "script_noteworthy");

	// CoDOL: no CoDOL MP map authors T5's script_noteworthy "spawn" dog nodes.
	// When they are absent the per-team fallback in dog_spawn_nodes_for_team()
	// derives spawn nodes from the gametype's own spawn points instead of
	// T5's random pick below, which stays as the last resort.
	level.dog_spawn_nodes_authored = ( level.dogspawnnodes.size > 0 );
	level.dog_team_spawn_nodes = [];

	if ( level.dogspawnnodes.size == 0 )
	{
/#
		println("DOG PATHING:  Could not find spawn nodes");
#/
		// pick a random set of spawn nodes so we do not tax the spawn logic to much
		level.dogspawnnodes = pick_random_nodes( pathnodes, 20 );
	}

	level.dogexitnodes = [];
	level.dogexitnodes = getnodearray( "exit", "script_noteworthy");

	if ( level.dogexitnodes.size == 0 )
	{
/#
		println("DOG PATHING:  Could not find exit nodes");
#/
		// pick a random set of spawn nodes so we do not tax the spawn logic to much
		level.dogexitnodes = pick_random_nodes( pathnodes, 20 );
	}
}

dog_dvar_update()
{
	level.dog_time = dog_get_dvar_int("scr_dog_time", "60" );
	level.dog_health = dog_get_dvar_int("scr_dog_health", "100" );
	level.dog_count = dog_get_dvar_int("scr_dog_count", "8" );
	level.dog_count_max_at_once = dog_get_dvar_int("scr_dog_max_at_once", "4" );

	if ( level.dog_count < level.dog_count_max_at_once )
	{
		level.dog_count_max_at_once = level.dog_count;
	}

	level.dog_debug = dog_get_dvar_int("debug_dogs", "0" );
	level.dog_debug_sound = dog_get_dvar_int("debug_dog_sound", "0" );
	level.dog_debug_anims = dog_get_dvar_int("debug_dog_anims", "0" );
	level.dog_debug_anims_ent = dog_get_dvar_int("debug_dog_anims_ent", "0" );
	level.dog_debug_turns = dog_get_dvar_int("debug_dog_turns", "0" );
	level.dog_debug_orient = dog_get_dvar_int("debug_dog_orient", "0" );
	level.dog_debug_usage = dog_get_dvar_int("debug_dog_usage", "1" );

	// CoDOL: T5 code owns these two turn thresholds; this engine does not
	// register them, and maps\mp\animscripts\dog_turn reads them straight
	// through GetDvarFloat.  Registering them here keeps the T5 values
	// (OpenBLOPS Game/Shared/bgame/bg_dog.cpp Dog_RegisterDvars: 70, 135 and
	// dog_turn_min_goal_dist 40; the "0"/"110" this port first used were not
	// T5's).
	dog_get_dvar( "dog_turn90_angle", "70" );
	dog_get_dvar( "dog_turn180_angle", "135" );
	dog_get_dvar( "dog_turn_min_goal_dist", "40" );
}

dog_dvar_updater()
{
	dogs_in_the_bsp = count_preexisting_dogs();
	while(1)
	{
		dog_dvar_update();

		// 16 is max allowed by engine
		if ( level.dog_count + dogs_in_the_bsp > 16 )
		{
			level.dog_count = 16 - dogs_in_the_bsp;
		}
		wait (1);
	}
}

count_preexisting_dogs()
{
		dogs = getentarray( "actor_enemy_dog_mp", "classname" );

	 	alive_count = 0;
	 	for ( i = 0; i < dogs.size; i ++ )
	 	{
	 		if ( !isdefined(dogs[i]) )
	 			continue;

	 		if ( !isai(dogs[i]) )
	 			continue;

	 		alive_count++;
	 	}

	 	return alive_count;
}

init_all_preexisting_dogs()
{
	array_thread( getentarray( "actor_enemy_dog_mp", "classname" ), ::preexisting_init_dog );
}

preexisting_init_dog()
{
	self init_dog();
}

dog_set_model()
{
	self setModel(level.friendlyDogModel);
	self setEnemyModel(level.enemyDogModel);
}

init_dog()
{
	if ( !isai(self) )
		return;

	self.aiteam = "axis";

	self.animTree = "dog.atr";
	self.type = "dog";
	self.accuracy = 0.2;
	self.health = level.dog_health;
	self.maxhealth = level.dog_health;  // this currently does not hook to code maxhealth
	self.aiweapon = level.dogBiteWeapon;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeAmmo = 0;
	self.goalradius = 128;
	self.noDodgeMove = true;
	self.ignoreSuppression = true;
	self.suppressionThreshold = 1;
	self.disableArrivals = false;
	self.pathEnemyFightDist = 512;
	self.halt_patrol = false;

	self.meleeAttackDist = 102;

	self dog_set_model();

	self thread dogHealthRegen();

	self thread selfDefenseChallenge();
}

get_spawn_node( team )
{
	nodes = dog_spawn_nodes_for_team( team );

	if ( !level.teambased )
	{
		node = dog_pick_node_away_from_enemy( nodes, team );
	}
	else
	{
		node = dog_pick_node_near_team( nodes, team );
	}

	return node;
}

// CoDOL: the dog spawn candidates for a team.  Authored "spawn" nodes win.
// Otherwise the owning team's spawn points (maps\mp\gametypes\_spawnlogic
// level.teamSpawnPoints[team]; level.spawnpoints when the gametype is not
// team based or has none for the team) are each mapped to their nearest path
// node, so the dog spawns where players spawn and can path from there.  The
// spawn point's angles ride along on the node for the teleport (path nodes
// from the entity string carry no "angles" key - that undefined is what killed
// the 2026-09-16 call-in at forceteleport).  Cached per team.
dog_spawn_nodes_for_team( team )
{
	if ( level.dog_spawn_nodes_authored )
		return level.dogspawnnodes;

	if ( isdefined( level.dog_team_spawn_nodes[team] ) )
		return level.dog_team_spawn_nodes[team];

	points = [];
	source = "level.spawnpoints";
	if ( level.teambased && isdefined( level.teamSpawnPoints ) && isdefined( level.teamSpawnPoints[team] ) && level.teamSpawnPoints[team].size > 0 )
	{
		points = level.teamSpawnPoints[team];
		source = "level.teamSpawnPoints[" + team + "]";
	}
	else if ( isdefined( level.spawnpoints ) )
	{
		points = level.spawnpoints;
	}

	nodes = [];
	for ( i = 0; i < points.size; i++ )
	{
		if ( !isdefined( points[i] ) || !isdefined( points[i].origin ) )
			continue;

		node = dog_get_closest_node( points[i].origin, level.dog_pathnodes );
		if ( !isdefined( node ) )
			continue;

		// one entry per node, whichever spawn point claimed it first
		if ( isdefined( node.dog_spawn_team ) && node.dog_spawn_team == team )
			continue;

		node.dog_spawn_team = team;
		if ( isdefined( points[i].angles ) )
			node.dog_spawn_angles = points[i].angles;

		nodes[nodes.size] = node;
	}

	if ( nodes.size == 0 )
	{
		print( "CoDOL dogs: no authored dog spawn nodes and no usable " + source + " for team " + team + "; using " + level.dogspawnnodes.size + " random path nodes\n" );
		nodes = level.dogspawnnodes;
	}
	else
	{
		print( "CoDOL dogs: no authored dog spawn nodes; using the " + nodes.size + " path nodes nearest the " + points.size + " " + source + " for team " + team + "\n" );
	}

	level.dog_team_spawn_nodes[team] = nodes;
	return nodes;
}

dog_watch_for_owner_team_change(owner)
{
	self endon("death");
	owner endon("disconnect");

	while(1)
	{
		owner waittill("joined_team");

		if ( owner.team != self.aiteam )
		{
			self clearentityowner();
			self notify("clear_owner");
			return;
		}
	}
}

dog_set_owner( owner, team, requiredDeathCount )
{
/#
	// no owner so he attacks the person who called him in
	if ( level.dog_debug )
		return;
#/

	if ( !isdefined( owner ) )
		return;

	// if the owner switches teams while dogs are still spawning
	// do not set the owner
	if ( level.teambased && isplayer(owner) && owner.team != team )
		return;

	self setentityowner(owner);

	self.requiredDeathCount = requiredDeathCount;

	self thread dog_watch_for_owner_team_change(owner);

	self endon("death");
	owner waittill("disconnect");

	self clearentityowner();
}

dog_manager_spawn_dog( owner, team, spawn_node, index, requiredDeathCount )
{
	// CoDOL: a failed dog is skipped, never a script error.  The native side
	// prints a "CoDOL dogs: ..." warning naming the cause before returning
	// undefined; the alive tracker already tolerates undefined level.dogs[].
	if ( !isdefined( spawn_node ) )
	{
		print( "CoDOL dogs: no spawn node for dog " + index + " (no path nodes on this map?); skipping it\n" );
		return undefined;
	}

	if ( !isdefined( level.dog_spawner ) )
	{
		print( "CoDOL dogs: dog spawner is gone; skipping dog " + index + "\n" );
		return undefined;
	}

	dog = level.dog_spawner spawnactor();

	if ( !isdefined( dog ) )
	{
		print( "CoDOL dogs: spawnactor produced no dog " + index + " for node at " + spawn_node.origin + " (the CoDOL dogs warning above names the cause); skipping it\n" );
		return undefined;
	}

	if ( !isdefined( spawn_node.origin ) )
	{
		print( "CoDOL dogs: spawn node has no origin; skipping dog " + index + "\n" );
		dog delete();
		return undefined;
	}

	// Path nodes read from the map entity string carry no "angles" key; use the
	// spawn point's angles the node was derived from, else a random facing.
	angles = spawn_node.angles;
	if ( !isdefined( angles ) )
		angles = spawn_node.dog_spawn_angles;
	if ( !isdefined( angles ) )
		angles = ( 0, randomint( 360 ), 0 );

	dog forceteleport( spawn_node.origin, angles );
	dog dog_set_goal_node( spawn_node );
	dog.spawnnode = spawn_node;
	dog show();

	dog init_dog();
	dog thread maps\mp\animscripts\dog_attack_log::watch();
	dog dog_set_team(team);
	dog dog_set_model();
	dog thread dog_set_owner(owner, team, requiredDeathCount );

	dog thread dog_usage(index);
	dog thread dog_owner_kills();
	dog thread dog_clean_up();
	dog thread dog_notify_level_on_death();
	dog dog_thread_behavior_function();
	dog thread monitor_dog_special_grenades();
	dog dog_create_spawn_influencer();

	return dog;
}

// T5 maps\mp\gametypes\_spawning::create_dog_influencers: a sphere of
// scr_spawn_dog_influencer_radius (15 player heights; T5's player height is
// 70, the value the RC-XD port uses) scoring scr_spawn_dog_influencer_score
// (-150), steep curve, on the spawn points of the dog's enemies (everyone in
// FFA).  It rides the native attached-sphere influencer the RC-XD uses
// (codol_rc_spawn_influence.c), which getBestWeightedSpawnpoint already adds
// to every candidate's weight, and it ends with the dog.
dog_create_spawn_influencer()
{
	protectedTeam = self.aiteam;
	if ( !level.teambased )
		protectedTeam = "none";
	self codol_setRCSpawnInfluencer( getFloatProperty( "scr_spawn_dog_influencer_radius", 15.0 * 70.0 ),
		getFloatProperty( "scr_spawn_dog_influencer_score", -150 ), 2, protectedTeam );
}

get_debug_team( team )
{
/#
	if ( level.teambased )
	{
		otherteam = getotherteam(team);
		if ( level.dog_debug )
			return otherteam;
	}
#/

	if ( !level.teambased )
		return "free";

	return team;
}

dog_manager_spawn_dogs( team, enemyTeam, deathCount )
{
	// this can hit if the round ends as the dogs are getting called in
	level endon("dogs done");
	level endon("dogs leaving");
	self endon("disconnect");

	if ( level.no_dogs )
		return;

	team = get_debug_team(team);

	requiredDeathCount = deathCount;

	if ( !dog_acquire_spawner() )
		return;

	level.dogs = [];

	level thread dog_manager_game_ended();
	level thread dog_manager_dog_alive_tracker( team );
	level thread dog_manager_dog_time_limit();
	level thread dog_usage_monitor();

	for ( i = 0; i < level.dog_count_max_at_once; i++ )
	{
		node = self get_spawn_node( team );

		level.dogs[i] = dog_manager_spawn_dog( self, team, node, i, requiredDeathCount );

		wait ( randomfloat( level.spawnTimeWaitMin, level.spawnTimeWaitMax ) );
	}

	level thread dog_manager_spawn_more_dogs_on_death( self, level.dog_count - level.dog_count_max_at_once, team );
}

// CoDOL: T5 dogs need the map's path nodes to spawn, patrol and leave.  On a
// map without them the dogs use substitute nodes (structs carrying .origin,
// marked .dog_struct): the bot waypoint graph's standable waypoints, else
// the gametype's spawn points.  Built once, at the first call-in, because the
// waypoint graph and the spawn points are loaded after _dogs::init.  Returns
// false only when there is nowhere a dog could stand.
dog_ensure_nodes()
{
	if ( !level.no_pathnodes )
		return true;

	points = [];
	source = "none";
	if ( getDvarInt( "developer" ) <= 0 || getDvarInt( "codol_dogsNoPathNodes" ) < 2 )
	{
		points = codol_getWalkableWaypoints();
		source = "bot waypoints";
	}
	if ( points.size == 0 )
	{
		source = "spawn points";
		if ( isdefined( level.spawnpoints ) )
			foreach ( spawnpoint in level.spawnpoints )
				points[points.size] = spawnpoint.origin;
	}

	nodes = [];
	foreach ( point in points )
	{
		node = spawnStruct();
		node.origin = point;
		node.dog_struct = true;
		nodes[nodes.size] = node;
	}

	print( "CoDOL dogs: the map has no path nodes; " + nodes.size + " substitute nodes from the " + source + "\n" );
	if ( nodes.size == 0 )
		return false;

	level.dog_pathnodes = nodes;
	level.patrolnodes = pick_random_nodes( nodes, 200 );
	level.dogspawnnodes = pick_random_nodes( nodes, 20 );
	level.dog_spawn_nodes_authored = false;
	level.dog_team_spawn_nodes = [];
	level.dogexitnodes = pick_random_nodes( nodes, 20 );
	level.no_pathnodes = false;
	return true;
}

// setgoalnode() for a map path node, setgoalpos() for a substitute node.
dog_set_goal_node( node )
{
	if ( isdefined( node.dog_struct ) )
		self setgoalpos( node.origin );
	else
		self setgoalnode( node );
}

// The dog spawner, found or built once per map (level.dog_spawner).  Split out
// of T5's dog_manager_spawn_dogs so useKillstreakDogs can refuse before the
// reward is spent.  Returns false (and sets level.no_dogs) when none can exist.
dog_acquire_spawner()
{
	if ( isdefined( level.dog_spawner ) )
		return true;

	if ( level.no_dogs )
		return false;

	// getent() raises "used with more than one entity" when several entities
	// carry the targetname, and an entity named "dog_spawner" need not be an
	// actor spawner at all: mp_nuked authors classname "actor_enemy_dog_so"
	// with spawnflags 1, which the MP entity spawn path never builds as a
	// spawner, and spawnactor() then refused it and killed the match. Scan the
	// whole set and take the first entity the native spawnactor() accepts;
	// isactorspawner() is that exact test and dumps every entity it rejects.
	level.dog_spawner = undefined;
	candidates = getentarray( "dog_spawner", "targetname" );
	rejected = 0;
	foreach ( candidate in candidates )
	{
		usable = candidate isactorspawner();
		if ( isdefined( usable ) && usable )
		{
			level.dog_spawner = candidate;
			break;
		}
		rejected++;
	}

	if ( isdefined( level.dog_spawner ) )
		print( "CoDOL dogs: using a map dog_spawner entity (" + candidates.size + " candidates, " + rejected + " rejected)\n" );
	else if ( candidates.size > 0 )
		print( "CoDOL dogs: none of the " + candidates.size + " map \"dog_spawner\" entities is an actor spawner (the CoDOL dogs warnings above dump them); building one with spawnactorspawner( \"actor_enemy_dog_mp\" )\n" );
	else
		print( "CoDOL dogs: no map dog_spawner entity; building one with spawnactorspawner( \"actor_enemy_dog_mp\" )\n" );

	// CoDOL adaptation. T5's MP maps ship an actor_enemy_dog_mp entity with the
	// SPAWNER spawnflag and targetname "dog_spawner"; no CoDOL map authors one.
	// spawnactorspawner() builds that same template through the engine's own
	// actor-spawner path (src/codol/player/codol_actor_spawn.cpp), so the
	// aitype's spawner() still runs on it and spawnactor() still has authored
	// fields to duplicate. It returns undefined when the dog aitype and
	// animscripts did not load, which is exactly the no-dogs case below.
	// Origin does not matter: every dog is forceteleported to its spawn node.
	if ( !isdefined( level.dog_spawner ) )
	{
		level.dog_spawner = spawnactorspawner( "actor_enemy_dog_mp", (0,0,0) );

		if ( !isdefined( level.dog_spawner ) )
		{
			print( "CoDOL dogs: spawnactorspawner returned no spawner (the CoDOL dogs warning above names the cause); dogs are disabled for this map\n" );
			level.no_dogs = true;
			return false;
		}

		level.dog_spawner.targetname = "dog_spawner";
	}

	return true;
}

dog_manager_spawn_more_dogs_on_death( owner, count, team )
{
	level endon("dogs done");
	level endon("dogs leaving");

	while( count > 0 )
	{
		level waittill("dog died");

		// wait a bit before sending in the next dog
		wait ( randomfloat( level.spawnTimeWaitMin, level.spawnTimeWaitMax ) );

		node = get_spawn_node( team );
		level.dogs[level.dogs.size] = dog_manager_spawn_dog( owner, team, node, level.dogs.size );
		count -= 1;
	}

/#
	iprintln("All dogs spawned");
#/
	level notify("all dogs spawned");
}

dog_manager_dog_time_limit()
{
	level endon("dogs done");
	level endon("dogs leaving");
	wait( level.dog_time );

/#
	dog_debug_print( "time limit hit notify dogs leaving" );
#/
	// this will shut this thread down
	level notify("dogs leaving");
}

dog_cleanup_wait( wait_for, notify_name )
{
	self endon( notify_name );
	self waittill( wait_for );
	self notify( notify_name, wait_for );
}

dog_cleanup_waiter()
{
	self thread dog_cleanup_wait( "all dogs spawned", "start_tracker");
	self thread dog_cleanup_wait( "dogs leaving", "start_tracker" );

	self waittill( "start_tracker", wait_for );
/#
	self dog_debug_print("starting dog_manager_dog_alive_tracker reason " + wait_for );
#/
}

dog_manager_dog_alive_tracker( team )
{
	level dog_cleanup_waiter();

	while (1)
	{
	 	alive_count = 0;
	 	for ( i = 0; i < level.dogs.size; i ++ )
	 	{
	 		if ( !isdefined(level.dogs[i]) )
	 			continue;

	 		if ( !isalive(level.dogs[i]) )
	 			continue;

	 		alive_count++;
	 	}

	 	if ( alive_count == 0 )
	 	{
 			wait(1);
	 		dog_manager_delete_dogs();
	 		level notify("dogs done");
	 		return;
		}

		wait (1);
	}
}

dog_manager_delete_dogs()
{
		for ( i = 0; i < level.dogs.size; i ++ )
	 	{
	 		if ( !isdefined(level.dogs[i]) )
	 			continue;

	 		level.dogs[i] delete();
		}

		level.dogs = undefined;
}

dog_manager_game_ended()
{
	level waittill("game_ended");
	make_all_dogs_leave();
}

make_all_dogs_leave()
{
/#
	dog_debug_print( "make_all_dogs_leave notify dogs leaving" );
#/
	level notify("dogs leaving");
}

dog_set_team( team )
{
	self.aiteam = team;
}

dog_clean_up()
{
	self endon("death");
	self endon("leaving");
	level waittill("dogs leaving");

	thread dog_leave();
}

dog_notify_level_on_death()
{
	self endon("leaving");
	self waittill("death");

	// do not access self past this point as its not valid

	level notify("dog died");
}

// ================================================
// T5 maps\mp\gametypes\_globallogic_actor, the MP actor damage and death
// callbacks (installed by init() as level.callbackActorDamage /
// level.callbackActorKilled; the engine reaches them through
// _callbacksetup::CodeCallback_ActorDamage / CodeCallback_ActorKilled).
// CoDOL changes:
//   - cac_modified_damage is not called: T5's returns the damage unchanged
//     unless the victim is a player, and the local one is player-only.
//   - the "onlyheadshots" tweakable, MOD_BURNED flame handling and the
//     explodable barrel/car weapon names are dropped (no counterpart here).
//   - hit markers go through the local _damagefeedback ("standard", the type
//     _damage.gsc draws for an unarmoured hit).
//   - "dogkill" XP: T5 registers 30 (team modes) / 20 (FFA) and scales it by
//     level.xpScale inside giveRankXP.  Neither the local _rank table nor its
//     giveRankXP knows "dogkill", so the value and the scale are applied here.
//   - the "killstreak_dogs" destroyed item stat and
//     level.globalKillstreaksDestroyed have no counterpart and are dropped.

Callback_ActorDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
{
	self.iDFlags = iDFlags;
	self.iDFlagsTime = getTime();

	if ( game["state"] == "postgame" )
		return;

	if ( self.aiteam == "spectator" )
		return;

	if ( isDefined( eAttacker ) && isPlayer( eAttacker ) && isDefined( eAttacker.canDoCombat ) && !eAttacker.canDoCombat )
		return;

	eAttacker = dog_figure_out_attacker( eAttacker );

	// Don't do knockback if the damage direction was not specified
	if ( !isDefined( vDir ) )
		iDFlags |= level.iDFLAGS_NO_KNOCKBACK;

	// check for completely getting out of the damage
	if ( !(iDFlags & level.iDFLAGS_NO_PROTECTION) )
	{
		if ( level.teamBased && isDefined( eAttacker ) && isPlayer( eAttacker ) && (self != eAttacker) && (self.aiteam == eAttacker.pers["team"]) )
		{
			if ( level.friendlyfire == 0 ) // no one takes damage
			{
				return;
			}
			else if ( level.friendlyfire == 1 ) // the friendly takes damage
			{
				// Make sure at least one point of damage is done
				if ( iDamage < 1 )
					iDamage = 1;

				self.lastDamageWasFromEnemy = false;

				self finishActorDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
			}
			else if ( level.friendlyfire == 2 ) // no one takes damage
			{
				return;
			}
			else if ( level.friendlyfire == 3 ) // both friendly and attacker take damage
			{
				iDamage = int( iDamage * .5 );

				// Make sure at least one point of damage is done
				if ( iDamage < 1 )
					iDamage = 1;

				self.lastDamageWasFromEnemy = false;

				self finishActorDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
			}
		}
		else
		{
			// no damage from the dogs owner unless in hardcore
			if ( isDefined( eAttacker ) && isDefined( self.script_owner ) && eAttacker == self.script_owner && !level.hardcoreMode )
				return;

			// dogs with the same owner can not damage each other
			if ( isDefined( eAttacker ) && isDefined( self.script_owner ) && isDefined( eAttacker.script_owner ) && eAttacker.script_owner == self.script_owner )
				return;

			// Make sure at least one point of damage is done
			if ( iDamage < 1 )
				iDamage = 1;

			if ( isDefined( eAttacker ) && isPlayer( eAttacker ) && isDefined( sWeapon ) && !isSubStr( sMeansOfDeath, "MOD_MELEE" ) )
				eAttacker thread maps\mp\gametypes\_weapons::checkHit( sWeapon, self );

			if ( isSubStr( sMeansOfDeath, "MOD_GRENADE" ) && isDefined( eInflictor ) && isDefined( eInflictor.isCooked ) )
				self.wasCooked = getTime();
			else
				self.wasCooked = undefined;

			self.lastDamageWasFromEnemy = ( isDefined( eAttacker ) && (eAttacker != self) );

			self finishActorDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
		}

		if ( isDefined( eAttacker ) && eAttacker != self && isPlayer( eAttacker ) )
		{
			if ( sWeapon != "artillery_mp" && ( !isDefined( eInflictor ) || !isai( eInflictor ) ) )
			{
				if ( iDamage > 0 )
					eAttacker thread maps\mp\gametypes\_damagefeedback::updateDamageFeedback( "standard" );
			}
		}
	}

	// T5 logs every actor hit as an "AD;" line.
	lpselfnum = self getEntityNumber();
	lpselfteam = self.aiteam;
	if ( isDefined( eAttacker ) && isPlayer( eAttacker ) )
	{
		lpattacknum = eAttacker getEntityNumber();
		lpattackGuid = eAttacker getGuid();
		lpattackname = eAttacker.name;
		lpattackerteam = eAttacker.pers["team"];
	}
	else
	{
		lpattacknum = -1;
		lpattackGuid = "";
		lpattackname = "";
		lpattackerteam = "world";
	}
	logPrint( "AD;" + lpselfnum + ";" + lpselfteam + ";" + lpattackGuid + ";" + lpattacknum + ";" + lpattackerteam + ";" + lpattackname + ";" + sWeapon + ";" + iDamage + ";" + sMeansOfDeath + ";" + sHitLoc + "\n" );
}

Callback_ActorKilled( eInflictor, attacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime )
{
	if ( game["state"] == "postgame" )
		return;

	if ( isDefined( attacker ) && isai( attacker ) && isDefined( attacker.script_owner ) )
	{
		// if the person who called the dogs in switched teams make sure they don't
		// get penalized for the kill
		if ( attacker.script_owner.team != self.aiteam )
			attacker = attacker.script_owner;
	}

	if ( isDefined( attacker ) && attacker.classname == "script_vehicle" && isDefined( attacker.owner ) )
		attacker = attacker.owner;

	if ( isDefined( attacker ) && isPlayer( attacker ) )
	{
		if ( !level.teamBased || (self.aiteam != attacker.pers["team"]) )
		{
			// T5 _rank: 30 in team modes, 20 in FFA, unless the gametype
			// overrides it (scr_<gametype>_score_dogkill).
			value = 30;
			if ( !level.teamBased )
				value = 20;
			if ( getDvar( "scr_" + level.gameType + "_score_dogkill" ) != "" )
				value = getDvarInt( "scr_" + level.gameType + "_score_dogkill" );
			attacker thread maps\mp\gametypes\_rank::giveRankXP( "dogkill", int( value * level.xpScale ) );
			if ( getDvarInt( "developer" ) > 0 )
				print( "CoDOL dogs: dog " + self getEntityNumber() + " killed by " + attacker.name + " (" + sWeapon + ", " + sMeansOfDeath + "): dogkill " + int( value * level.xpScale ) + " XP\n" );

			// Dogs no longer give team or player score
		}
	}
}

// T5 _globallogic_player::figureOutAttacker: a dog hurt by another dog is hurt
// by that dog's owner, unless the owner has since joined the victim's team; a
// vehicle or turret attacker is its owner.
dog_figure_out_attacker( eAttacker )
{
	if ( isDefined( eAttacker ) )
	{
		if ( isai( eAttacker ) && isDefined( eAttacker.script_owner ) )
		{
			if ( eAttacker.script_owner.team != self.aiteam )
				eAttacker = eAttacker.script_owner;
		}

		if ( eAttacker.classname == "script_vehicle" && isDefined( eAttacker.owner ) )
			eAttacker = eAttacker.owner;
		else if ( ( eAttacker.classname == "auto_turret" || eAttacker.classname == "misc_turret" ) && isDefined( eAttacker.owner ) )
			eAttacker = eAttacker.owner;
	}

	return eAttacker;
}

// T5 maps\mp\gametypes\_weapons::monitor_dog_special_grenades.  A flash or
// concussion grenade reaches a dog as a tiny amount of splash damage; T5 turns
// any hit under 5 into a flash of every dog within 128 units (smoke and tabun
// handle themselves - see equipment\_tabun).
monitor_dog_special_grenades() // self == dog
{
	// watch and see if the dog gets damage from a flash or concussion
	//	smoke and tabun handle themselves
	self endon("death");

	while(1)
	{
		self waittill( "damage", damage, attacker );

		if( damage < 5 && isDefined( attacker ) )
		{
			damage_area = Spawn( "trigger_radius", self.origin, 0, 128, 128 );
			attacker thread maps\mp\_dogs::flash_dogs( damage_area );
			wait(0.05);
			damage_area delete();
		}
	}
}

dog_thread_behavior_function()
{
		self thread dog_patrol_when_no_enemy();
//		self thread dog_attack_when_enemy();
}

dog_leave()
{
	self notify("leaving");

	self thread dog_leave_failsafe();

	// have them run to an exit node
	self clearentitytarget();
	self.ignoreall = true;
	self.goalradius = 30;
	self dog_set_goal_node( self dog_get_exit_node() );

	self waittill("goal");
	self delete();
}

dog_leave_failsafe()
{
	self endon("death");

	start_origin = self.origin;

	wait(2);

	if ( distance( start_origin, self.origin ) < 10 )
	{
/#
		println( "DOG DELETE FAILSAFE:  Dog appears to be stuck at " + self.origin );
#/
		self delete();
		return;
	}

	wait(20);
/#
	println( "DOG DELETE FAILSAFE:  Dog has not gotten to it's delete point after 20 seconds.  Currently at " + self.origin );
#/
	self delete();
}


dog_patrol_when_no_enemy()
{
	self endon("death");
	self endon("leaving");

	while(1)
	{
		if ( !isdefined(self.enemy) )
		{
			self dog_debug_print( "no enemy starting patrol" );
			self thread dog_patrol();
		}

		self waittill("enemy");
	}
}

dog_patrol_wait( wait_for, notify_name )
{
	self endon("attacking");
	dog_wait( wait_for, notify_name );
}

dog_wait( wait_for, notify_name )
{
	self endon("death");
	self endon("leaving");
	self endon( notify_name );

	self waittill( wait_for );
	self notify( notify_name, wait_for );
}

dog_patrol_path_waiter()
{
	self thread dog_patrol_wait( "bad_path", "next_patrol_point");
	self thread dog_patrol_wait( "goal", "next_patrol_point" );

	self waittill( "next_patrol_point", wait_for );
/#
	self dog_debug_print("ending patrol wait recieved " + wait_for );
#/
}

dog_patrol()
{
	self endon("death");
	self endon("enemy");
	self endon("leaving");
	self endon("attacking");
	self notify("on patrol");

	self dog_patrol_debug();

	node = level.patrolnodes[randomInt(level.patrolnodes.size)];

	while(1)
	{
		if ( self.halt_patrol )
		{
			self setgoalpos( self.origin );
			wait( 1 );
			continue;
		}

		for( count = 0; count < 25; count++ )
		{
			newnode = level.patrolnodes[randomInt(level.patrolnodes.size)];

			if ( DistanceSquared( node.origin, newnode.origin ) > ( 1024 * 1024 ) )
			{
				node = newnode;
				break;
			}

			node = newnode;
		}

		// ignore all nodes with a script_noteworthy on them
		if ( !isdefined( node.script_noteworthy ) )
		{
/#
			self dog_debug_print("patroling to node at " + node.origin );
#/
			self dog_set_goal_node( node );
			self dog_patrol_path_waiter();
		}
	}
}

dog_wait_print( wait_for )
{
/#
	self endon("kill dog_wait_prints");

	self waittill( wait_for );
	self notify("kill dog_wait_prints");
	self dog_debug_print( "PATROL ENDING " + wait_for );
#/
}

dog_patrol_debug()
{
	self thread dog_wait_print("death");
	self thread dog_wait_print("enemy");
	self thread dog_wait_print("leaving");
	self thread dog_wait_print("attacking");
}

dog_get_dvar_int( dvar, def )
{
	return int( dog_get_dvar( dvar, def ) );
}

// dvar set/fetch/check
dog_get_dvar( dvar, def )
{
	if ( getdvar( dvar ) != "" )
		return getdvarfloat( dvar );
	else
	{
		setdvar( dvar, def );
		return def;
	}
}

dog_usage_init()
{
	level.dog_usage = [];

	for ( index = 0; index < level.dog_count; index++ )
	{
		level.dog_usage[index] = spawnStruct();
		level.dog_usage[index].spawn_time = 0;
		level.dog_usage[index].death_time = 0;
		level.dog_usage[index].kills = 0;
		level.dog_usage[index].died = false;
	}
}

dog_usage_monitor()
{
	start_time = GetTime();

	level waittill("dogs done");

	index = 0;
	total_kills = 0;
	last_alive = 0;
	all_dead = true;
	alive_count = 0;
	never_spawned_count = 0;
	total_count = 0;

	for ( index = 0; index < level.dog_count; index++ )
	{
		total_count++;

		if ( level.dog_usage[index].spawn_time == 0 )
		{
			never_spawned_count++;
			continue;
		}
		else if ( !level.dog_usage[index].died )
		{
			alive_count++;
			all_dead = false;
		}

		seconds = (level.dog_usage[index].death_time - level.dog_usage[index].spawn_time) / 1000;
		if ( seconds > last_alive )
		{
			last_alive = seconds;
		}

		total_kills += level.dog_usage[index].kills;
	}

/#
	seconds = (GetTime() - start_time) / 1000;
	msg = "Dogs- Time: " + seconds + " Kills: " + total_kills + " Last: " + last_alive;

	// make sure that the dogs have printed everything
	wait (1);
	iprintln( msg );
	println( msg );
#/
}

dog_usage(index)
{
	level.dog_usage[index].spawn_time = GetTime();
	level.dog_usage[index].death_time = 0;
	level.dog_usage[index].kills = 0;
	level.dog_usage[index].died = false;

	self thread dog_usage_kills(index);
	self thread dog_usage_time_alive(index);
}

dog_usage_kills(index)
{
	self endon("death");

	while(1)
	{
		self waittill("killed", player);
		level.dog_usage[index].kills++;
	}
}

dog_owner_kills(index)
{
	if ( !isdefined( self.script_owner ) )
		return;

	self endon("clear_owner");
	self endon("death");
	self.script_owner endon("disconnect");

	while(1)
	{
		self waittill("killed", player);

		if ( IsDefined( self.script_owner ) )
		{
			self.script_owner notify( "dog_handler" );
		}
	}
}

dog_usage_time_alive(index)
{
	self endon("leaving");

	self waittill("death");
	level.dog_usage[index].death_time = GetTime();
	level.dog_usage[index].died = true;

	seconds = (level.dog_usage[index].death_time - level.dog_usage[index].spawn_time) / 1000 ;
/#
	iprintln( "Dog#" + index + " killed. Alive for: "+ seconds + " seconds. Kills: " + level.dog_usage[index].kills );
	println( "Dog#" + index + " killed. Alive for: "+ seconds + " seconds. Kills: " + level.dog_usage[index].kills );
#/
}


dogHealthRegen()
{
	self endon("death");
	self endon("end_healthregen");

	if ( self.health <= 0 )
	{
		assert( !isalive( self ) );
		return;
	}

	maxhealth = self.health;
	oldhealth = maxhealth;
	dog = self;
	health_add = 0;

	regenRate = 0.1; // 0.017;
	veryHurt = false;

	lastSoundTime_Recover = 0;
	hurtTime = 0;
	newHealth = 0;

	for (;;)
	{
		wait (0.05);
		if (dog.health == maxhealth)
		{
			veryHurt = false;
			continue;
		}

		if (dog.health <= 0)
			return;

		wasVeryHurt = veryHurt;
		ratio = dog.health / maxHealth;
		if (ratio <= 0.55)
		{
			veryHurt = true;
			if (!wasVeryHurt)
			{
				hurtTime = gettime();
			}
		}

		if (dog.health >= oldhealth)
		{
			if (gettime() - hurttime < level.dogHealth_RegularRegenDelay)
				continue;

			if ( level.dogHealthRegenDisabled )
				continue;

			if (veryHurt)
			{
				newHealth = ratio;
				if (gettime() > hurtTime + 3000)
					newHealth += regenRate;
			}
			else
				newHealth = 1;

			if ( newHealth >= 1.0 )
			{
				newHealth = 1.0;
			}

			if (newHealth <= 0)
			{
				// dog is dead
				return;
			}

			dog setnormalhealth (newHealth);
			oldhealth = dog.health;
			continue;
		}

		oldhealth = dog.health;

		health_add = 0;
		hurtTime = gettime();
	}
}


selfDefenseChallenge()
{
	self waittill ("death", attacker);

	if ( isdefined( attacker ) && isPlayer( attacker ) )
	{
		if (isdefined ( self.script_owner ) && self.script_owner == attacker)
			return;
		if ( level.teambased && isdefined ( self.script_owner ) && self.script_owner.team == attacker.team )
			return;

		attacker notify ("selfdefense_dog");
	}

}

dog_get_exit_node()
{
	return dog_get_closest_node( self.origin, level.dogexitnodes );
}

// CoDOL: T5 calls getclosest(), which it defines in maps\mp\_utility.  The
// CoDOL maps\mp\_utility has no such helper, so the T5 body is carried here
// unchanged (minus the unused maxdist argument, which _dogs never passes).
dog_get_closest_node( org, array )
{
	if ( !isdefined( array ) || array.size == 0 )
		return undefined;

	best = undefined;
	bestdist = undefined;

	for ( i = 0; i < array.size; i++ )
	{
		if ( !isdefined( array[i] ) )
			continue;

		dist = distanceSquared( org, array[i].origin );

		if ( !isdefined( bestdist ) || dist < bestdist )
		{
			bestdist = dist;
			best = array[i];
		}
	}

	return best;
}

dog_debug_print( message )
{
/#
	if ( level.dog_debug )
	{
		if ( isai( self ) )
		{
			println( " " + gettime() + " DOG " + self getentnum() + ": " + message );
		}
		else
		{
			println( " " + gettime() + " DOGS: " + message );
		}
	}
#/
}

// ================================================

getAllOtherPlayers()
{
	aliveplayers = [];

	// Make a list of fully connected, non-spectating, alive players
	for(i = 0; i < level.players.size; i++)
	{
		if ( !isdefined( level.players[i] ) )
			continue;
		player = level.players[i];

		if ( player.sessionstate != "playing" || player == self )
			continue;

		aliveplayers[aliveplayers.size] = player;
	}
	return aliveplayers;
}

dog_pick_node_near_team( nodes, team )
{
	// There are no valid nodes in the map
	if(!isdefined(nodes))
		return undefined;

	if ( !level.teambased )
		return dog_pick_node_away_from_enemy(level.dogspawnnodes, team);

	//prof_begin("basic_spawnlogic");

	initWeights(nodes);
	update_all_nodes( nodes, team );

	//prof_begin(" getteams");
	obj = spawnstruct();
	getAllAlliedAndEnemyPlayers(obj, team);
	//prof_end(" getteams");

	numplayers = obj.allies.size + obj.enemies.size;

	alliedDistanceWeight = 2;
	dogDistanceWeight = 3;

	//prof_begin(" sumdists");
	myTeam = team;

	enemyTeam = getOtherTeam( myTeam );
	for (i = 0; i < nodes.size; i++)
	{
		node = nodes[i];

		node.weight = 0;

		if ( node.numPlayersAtLastUpdate > 0 )
		{
			allyDistSum = node.distSum[ myTeam ];
			enemyDistSum = node.distSum[ enemyTeam ];

			// high enemy distance is good, high ally distance is bad
			node.weight = (enemyDistSum - alliedDistanceWeight*allyDistSum) / node.numPlayersAtLastUpdate;
		}

		if ( node.numDogsAtLastUpdate > 0 )
		{
			dogDistSum = node.distSum[ "dogs" ];

			// high ally distance is bad
			node.weight -= (dogDistSum*dogDistSum) / node.numDogsAtLastUpdate;
		}
	}
	//prof_end(" sumdists");

	//prof_end("basic_spawnlogic");

	//prof_begin("complex_spawnlogic");

	avoidSpawnReuse(nodes);
	avoidEnemies(nodes, team, true);

	//prof_end("complex_spawnlogic");

	result = dog_pick_node_final(nodes, team, obj.enemies);

	return result;
}


dog_pick_node_away_from_enemy(nodes, team)
{
	// There are no valid nodes in the map
	if(!isdefined(nodes))
		return undefined;

	initWeights(nodes);
	update_all_nodes( nodes, team );

	aliveplayers = getAllOtherPlayers();

	// new logic: we want most players near idealDist units away.
	// players closer than badDist units will be considered negatively
	idealDist = 1600;
	badDist = 1200;

	if (aliveplayers.size > 0)
	{
		for (i = 0; i < nodes.size; i++)
		{
			totalDistFromIdeal = 0;
			nearbyBadAmount = 0;
			for (j = 0; j < aliveplayers.size; j++)
			{
				dist = distance(nodes[i].origin, aliveplayers[j].origin);

				if (dist < badDist)
					nearbyBadAmount += (badDist - dist) / badDist;

				distfromideal = abs(dist - idealDist);
				totalDistFromIdeal += distfromideal;
			}
			avgDistFromIdeal = totalDistFromIdeal / aliveplayers.size;

			wellDistancedAmount = (idealDist - avgDistFromIdeal) / idealDist;

			// wellDistancedAmount is between -inf and 1, 1 being best (likely around 0 to 1)
			// nearbyBadAmount is between 0 and inf,
			// and it is very important that we get a bad weight if we have a high nearbyBadAmount.

			nodes[i].weight = wellDistancedAmount - nearbyBadAmount * 2 + randomfloat(.2);
		}
	}

	avoidSpawnReuse(nodes);
	avoidEnemies(nodes, team, true);

	return dog_pick_node_final(nodes, team, aliveplayers);
}

dog_pick_node_random( nodes, team )
{
	// There are no valid nodes in the map
	if(!isdefined(nodes))
		return undefined;

	// randomize order
	for(i = 0; i < nodes.size; i++)
	{
		j = randomInt(nodes.size);
		node = nodes[i];
		nodes[i] = nodes[j];
		nodes[j] = node;
	}

	return dog_pick_node_final(nodes, team, undefined, false);
}

// selects a node, preferring ones with heigher weights (or toward the beginning of the array if no weights).
// also does final things like setting self.lastnode to the one chosen.
// this takes care of avoiding telefragging, so it doesn't have to be considered by any other function.
dog_pick_node_final( nodes, team, enemies, useweights )
{
	//prof_begin( "dog_pick_node_final" );

	bestnode = undefined;

	if ( !isdefined( nodes ) || nodes.size == 0 )
		return undefined;

	if ( !isdefined( useweights ) )
		useweights = true;

	if ( useweights )
	{
		// choose node with best weight
		// (if a tie, choose randomly from the best)
		bestnode = getBestWeightedNode( nodes, team, enemies );
	}
	else
	{
		// (only place we actually get here from is dog_pick_node_random() )
		// no weights. prefer nodes toward beginning of array
		for ( i = 0; i < nodes.size; i++ )
		{
			if ( positionWouldTelefrag( nodes[i].origin ) )
				continue;

			bestnode = nodes[i];
			break;
		}
	}

	if ( !isdefined( bestnode ) )
	{
		// couldn't find a useable node! all will telefrag.
		if ( useweights )
		{
			// at this point, forget about weights. just take a random one.
			bestnode = nodes[randomint(nodes.size)];
		}
		else
		{
			bestnode = nodes[0];
		}
	}

	self finalizeNodeChoice( bestnode );

	//prof_end( "dog_pick_node_final" );

	return bestnode;
}

getBestWeightedNode( nodes, team, enemies )
{
	maxSightTracedNodes = 3;
	for ( try = 0; try <= maxSightTracedNodes; try++ )
	{
		bestnodes = [];
		bestweight = undefined;
		bestnode = undefined;
		for ( i = 0; i < nodes.size; i++ )
		{
			if ( !isdefined( bestweight ) || nodes[i].weight > bestweight )
			{
				if ( positionWouldTelefrag( nodes[i].origin ) )
					continue;

				bestnodes = [];
				bestnodes[0] = nodes[i];
				bestweight = nodes[i].weight;
			}
			else if ( nodes[i].weight == bestweight )
			{
				if ( positionWouldTelefrag( nodes[i].origin ) )
					continue;

				bestnodes[bestnodes.size] = nodes[i];
			}
		}
		if ( bestnodes.size == 0 )
			return undefined;

		// pick randomly from the available nodes with the best weight
		bestnode = bestnodes[randomint( bestnodes.size )];

		if ( try == maxSightTracedNodes )
			return bestnode;

		if ( isdefined( bestnode.lastSightTraceTime ) && bestnode.lastSightTraceTime == gettime() )
			return bestnode;

		if ( !lastMinuteSightTraces( bestnode, team, enemies ) )
			return bestnode;

		penalty = getLosPenalty();
		bestnode.weight -= penalty;

		bestnode.lastSightTraceTime = gettime();
	}
}

finalizeNodeChoice( node )
{
	time = getTime();

	self.lastnode = node;
	self.lastspawntime = time;
	node.lastspawneddog = self;
	node.lastspawntime = time;
}

getLosPenalty()
{
	return 100000;
}

lastMinuteSightTraces( node, dog_team, enemies )
{
	//prof_begin("lastMinuteSightTraces");

	team = "all";
	if ( level.teambased )
		team = getOtherTeam( dog_team );

	if ( !isdefined( enemies ) )
		return false;

	closest = undefined;
	closestDistsq = undefined;
	secondClosest = undefined;
	secondClosestDistsq = undefined;

	for ( i = 0; i < enemies.size; i++ )
	{
		player = node.nearbyPlayers[team][i];

		if ( !isdefined( player ) )
			continue;
		if ( player.sessionstate != "playing" )
			continue;
		if ( player == self )
			continue;

		distsq = distanceSquared( node.origin, player.origin );
		if ( !isdefined( closest ) || distsq < closestDistsq )
		{
			secondClosest = closest;
			secondClosestDistsq = closestDistsq;

			closest = player;
			closestDistSq = distsq;
		}
		else if ( !isdefined( secondClosest ) || distsq < secondClosestDistSq )
		{
			secondClosest = player;
			secondClosestDistSq = distsq;
		}
	}

	if ( isdefined( closest ) )
	{
		if ( bullettracepassed( closest.origin  + (0,0,50), node.origin + (0,0,50), false, undefined) )
			return true;
	}
	if ( isdefined( secondClosest ) )
	{
		if ( bullettracepassed( secondClosest.origin + (0,0,50), node.origin + (0,0,50), false, undefined) )
			return true;
	}

	return false;
}

update_all_nodes( nodes, team )
{
	for ( i = 0; i < nodes.size; i++ )
	{
		nodeUpdate( nodes[i], team );
	}
}

avoidEnemies(nodes, team,teambased)
{
	lospenalty = getLosPenalty();

	otherteam = "axis";
	if ( team == "axis" )
		otherteam = "allies";

	minDistTeam = otherteam;

	if ( !teambased )
	{
		minDistTeam = "all";
	}

	// CoDOL: T5 read these with GetDvarFloat( #"..." ).  Neither dvar is
	// registered by this engine, so they come through the local property
	// helper with the T5 code defaults.
	avoidWeight = getFloatProperty( "scr_spawn_enemyavoidweight", 1 );
	if ( avoidWeight != 0 )
	{
		nearbyEnemyOuterRange = getFloatProperty( "scr_spawn_enemyavoiddist", 1000 );
		nearbyEnemyOuterRangeSq = nearbyEnemyOuterRange * nearbyEnemyOuterRange;
		nearbyEnemyPenalty = 1500 * avoidWeight; // typical base weights tend to peak around 1500 or so. this is large enough to upset that while only locally dominating it.
		nearbyEnemyMinorPenalty = 800 * avoidWeight; // additional negative weight for distances up to 2 * nearbyEnemyOuterRange

		lastAttackerOrigin = (-99999,-99999,-99999);
		lastDeathPos = (-99999,-99999,-99999);

		for ( i = 0; i < nodes.size; i++ )
		{
			// penalty for nearby enemies
			mindist = nodes[i].minDist[minDistTeam];
			if ( mindist < nearbyEnemyOuterRange*2 )
			{
				penalty = nearbyEnemyMinorPenalty * (1 - mindist / (nearbyEnemyOuterRange*2));
				if ( mindist < nearbyEnemyOuterRange )
					penalty += nearbyEnemyPenalty * (1 - mindist / nearbyEnemyOuterRange);
				if ( penalty > 0 )
				{
					nodes[i].weight -= penalty;
				}
			}
		}
	}

	// DEBUG
	//prof_end(" spawn_sc");
}

nodeUpdate( node, team )
{
		if ( level.teambased )
		{
			node.sights["axis"] = 0;
			node.sights["allies"] = 0;

			node.nearbyPlayers["axis"] = [];
			node.nearbyPlayers["allies"] = [];
		}
		else
		{
			node.sights = 0;

			node.nearbyPlayers["all"] = [];
		}

		node.nearbyDogs = [];

		nodedir = node.forward;

		debug = false;

		node.distSum["all"] = 0;
		node.distSum["allies"] = 0;
		node.distSum["axis"] = 0;
		node.distSum["dogs"] = 0;

		node.minDist["all"] = 9999999;
		node.minDist["allies"] = 9999999;
		node.minDist["axis"] = 9999999;
		node.minDist["dogs"] = 9999999;

		node.numPlayersAtLastUpdate = 0;
		node.numDogsAtLastUpdate = 0;

		for (i = 0; i < level.players.size; i++)
		{
			player = level.players[i];

			if ( player.sessionstate != "playing" )
				continue;

			diff = player.origin - node.origin;
			diff = (diff[0], diff[1], 0);
			dist = length( diff ); // needs to be actual distance for distSum value

			player_team = "all";
			if ( level.teambased )
				player_team = player.team;

			if ( dist < 1024 )
			{
				node.nearbyPlayers[player_team][node.nearbyPlayers[player_team].size] = player;
			}

			if ( dist < node.minDist[player_team] )
				node.minDist[player_team] = dist;

			node.distSum[ player_team ] += dist;
			node.numPlayersAtLastUpdate++;
		}

		for (i = 0; i < level.dogs.size; i++)
		{
			dog = level.dogs[i];

			if ( !isdefined(dog) || !isalive(dog) )
				continue;

			diff = dog.origin - node.origin;
			diff = (diff[0], diff[1], 0);
			dist = length( diff ); // needs to be actual distance for distSum value

			if ( dist < 1024 )
			{
				node.nearbyDogs[node.nearbyDogs.size] = dog;
			}

			if ( dist < node.minDist["dogs"] )
				node.minDist["dogs"] = dist;

			node.distSum[ "dogs" ] += dist;
			node.numDogsAtLastUpdate++;
		}
}

initWeights(nodes)
{
	for (i = 0; i < nodes.size; i++)
		nodes[i].weight = 0;
}

getAllAlliedAndEnemyPlayers( obj, team )
{
	if ( level.teambased )
	{
		if ( team == "allies" )
		{
			obj.allies = level.alivePlayers["allies"];
			obj.enemies = level.alivePlayers["axis"];
		}
		else
		{
			assert( team == "axis" );
			obj.allies = level.alivePlayers["axis"];
			obj.enemies = level.alivePlayers["allies"];
		}
	}
	else
	{
		obj.allies = [];
		obj.enemies = level.activePlayers;
	}
}

avoidSpawnReuse(nodes)
{
	time = getTime();

	maxtime = 3*1000;
	maxdistSq = 1024 * 1024;

	for (i = 0; i < nodes.size; i++)
	{
		node = nodes[i];

		if (!isdefined(node.lastspawntime))
			continue;

		timepassed = time - node.lastspawntime;
		if (timepassed < maxtime)
		{
			worsen = 1000 * (1 - timepassed/maxtime);
			node.weight -= worsen;
		}
	}
}

flash_dogs( area )
{
	self endon("disconnect");

	if ( isdefined(level.dogs) )
	{
		for (i = 0; i < level.dogs.size; i++)
		{
			dog = level.dogs[i];

			if ( !isalive(dog) )
					continue;
			if ( dog istouching(area) )
			{
				do_flash = true;
				if ( isPlayer( self ) )
				{
					if ( level.teamBased && (dog.aiteam == self.team) )
					{
						do_flash = false;
					}
					else if ( !level.teambased && isdefined(dog.script_owner) && self == dog.script_owner )
					{
						do_flash = false;
					}
				}

				if ( isdefined( dog.lastFlashed ) && dog.lastFlashed + 1500 > gettime()  )
				{
					do_flash = false;
				}

				if ( do_flash )
				{
					dog setFlashBanged( true, 500 );
					dog.lastFlashed = gettime();
				}
			}
		}
	}
}
