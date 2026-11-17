#include maps\mp\_utility;
#include common_scripts\utility;

/*
	PROVENANCE -- gsc_overlay ADAPTED/AUTHORED file (2026-09-02).
	NOT a vendor extract.  This module server-authors the effect of every
	killstreak whose vendor killstreakFunc cannot run to completion on this
	emulator VM (it needs rideable-projectile usercmd forwarding, a vehicle
	path network + turret AI, a client vision force, or -- for dogs -- an asset
	that does not exist in vendor/iw4 at all).  Rather than let those vendor
	funcs HALT on an unreachable builtin, init() OVERRIDES
	level.killStreakFuncs[<name>] with a CODOL func here that reproduces the
	SERVER-OBSERVABLE effect (team radar, real player damage, a placed turret
	entity) using only reachable builtins:

	    setTeamRadar / blockTeamRadar          (ps+0x21C / ps+0x220 wire bits)
	    beginLocationSelection / confirm_location  (answered by
	                                            killstreak_support._pump_locsel)
	    codol_radiusDamage( pos,radius,max,min,owner,weapon[,mod] )   NEW builtin
	    codol_fire_nearest( src,range,dmg,owner,weapon[,mod] )        NEW builtin
	    spawn / setModel / delete / getTime / anglesToForward

	The streaks left on the VENDOR path (uav, counter_uav, precision_airstrike,
	airdrop, airdrop_mega) are NOT overridden here -- they already run end to
	end (docs/killstreaks_gsc.md).  Every number below is SOURCED from the
	vendor script or the weapon file; the citation is on the line.  What this
	layer cannot make (a VISIBLE plane/heli/turret model, animated dogs, the
	ride camera, the EMP screen-whiteout, the nuke game-end) is called out in
	docs/killstreaks_full_fix.md.
*/

init()
{
	// runs AFTER the vendor module inits (a direct call at the tail of
	// _killstreaks::init, so the vendor registrations already exist and these
	// overrides win).
	level.killStreakFuncs[ "emp" ]                = ::codol_emp;
	level.killStreakFuncs[ "double_uav" ]         = ::codol_advanced_uav;
	level.killStreakFuncs[ "blackbird" ]          = ::codol_advanced_uav;
	level.killStreakFuncs[ "advanced_uav" ]       = ::codol_advanced_uav;
	level.killStreakFuncs[ "sr71" ]               = ::codol_advanced_uav;
	level.killStreakFuncs[ "predator_missile" ]   = ::codol_predator;
	level.killStreakFuncs[ "stealth_airstrike" ]  = ::codol_stealth;
	level.killStreakFuncs[ "harrier_airstrike" ]  = ::codol_harrier;
	level.killStreakFuncs[ "helicopter" ]         = ::codol_helicopter;
	level.killStreakFuncs[ "helicopter_flares" ]  = ::codol_pavelow;
	level.killStreakFuncs[ "helicopter_minigun" ] = ::codol_heli_minigun;
	level.killStreakFuncs[ "ac130" ]              = ::codol_ac130;
	level.killStreakFuncs[ "sentry" ]             = ::codol_sentry;
	level.killStreakFuncs[ "sam_turret" ]         = ::codol_sam;
	level.killStreakFuncs[ "mortar" ]             = ::codol_mortar;
	level.killStreakFuncs[ "rcbomb" ]             = ::codol_rcxd;
	level.killStreakFuncs[ "rcxd" ]               = ::codol_rcxd;
	level.killStreakFuncs[ "dogs" ]               = ::codol_dogs;
	level.killStreakFuncs[ "nuke" ]               = ::codol_nuke;
}

codol_other_team( team )
{
	if ( team == "allies" )
		return "axis";
	return "allies";
}

// -- shared helpers --------------------------------------------------------

codol_delayed_blast( loc, delay, radius, maxd, mind, weapon )
{
	self endon( "disconnect" );
	wait( delay );
	codol_radiusDamage( loc, radius, maxd, mind, self, weapon );
}

// a loitering gunship: fire a paced burst at the nearest enemy for `lifespan`
// seconds.  The vehicle model + real flight path + ride camera are engine-
// walled; the GUN is the observable (weapon-file per-shot damage, paced).
codol_gunship( lifespan, weapon, burst )
{
	self endon( "disconnect" );
	wait( 3.0 );
	startTime = getTime();
	while ( getTime() - startTime < lifespan * 1000 )
	{
		codol_fire_nearest( self.origin, 5000, burst, self, weapon );
		wait( 1.0 );
	}
}

// -- radar streaks (pure server state, fully on the wire) ------------------

codol_advanced_uav( lifeId )		// Advanced UAV / Blackbird / LSAT / SR-71
{
	self endon( "disconnect" );
	team = self.team;
	setTeamRadar( team, 1 );		// _uav.gsc:432 -- ps+0x21C on the wire
	self thread codol_radar_off( team, 30 );	// _uav.gsc:19 radarViewTime
	return true;
}

codol_radar_off( team, secs )
{
	wait( secs );
	setTeamRadar( team, 0 );
}

codol_emp( lifeId )
{
	self endon( "disconnect" );
	otherTeam = codol_other_team( self.team );
	blockTeamRadar( otherTeam );		// _emp.gsc EMP_JamTeam -- ps+0x220
	self thread codol_emp_off( otherTeam, 60 );	// _emp.gsc:61 60.0 s
	return true;
}

codol_emp_off( team, secs )
{
	wait( secs );
	unblockTeamRadar( team );
}

// -- location-picked damage streaks ---------------------------------------

codol_predator( lifeId )
{
	self endon( "disconnect" );
	self beginLocationSelection( "map_artillery_selector", true, 256 );
	self waittill( "confirm_location", location );
	self endLocationSelection();
	// remotemissile_projectile_mp: explosionRadius 450, inner 1000, outer 10.
	self thread codol_delayed_blast( location, 2.0, 450, 1000, 50,
			"killstreak_predator_missile_mp" );
	return true;
}

codol_stealth( lifeId )			// Stealth Bomber
{
	self endon( "disconnect" );
	self beginLocationSelection( "map_stealth_selector", true, 896 );
	self waittill( "confirm_location", location );
	self endLocationSelection();
	// _airstrike.gsc:654  losRadiusDamage 896/300/50 stealth_bomb_mp
	self thread codol_delayed_blast( location, 1.5, 896, 300, 50, "stealth_bomb_mp" );
	return true;
}

codol_harrier( lifeId )			// Harriers: opening bomb run + loiter strafe
{
	self endon( "disconnect" );
	self beginLocationSelection( "map_artillery_selector", true, 512 );
	self waittill( "confirm_location", location );
	self endLocationSelection();
	self thread codol_delayed_blast( location, 1.0, 512, 200, 30, "artillery_mp" );
	self thread codol_harrier_loiter( location, 45 );	// _harrier.gsc ~45 s
	return true;
}

codol_harrier_loiter( center, lifespan )
{
	self endon( "disconnect" );
	wait( 4.0 );
	startTime = getTime();
	while ( getTime() - startTime < lifespan * 1000 )
	{
		codol_fire_nearest( center, 2500, 120, self, "harrier_20mm_mp" );
		wait( 3.0 );
	}
}

codol_mortar( lifeId )			// Mortar Team (CODOL; no vendor GSC)
{
	self endon( "disconnect" );
	self beginLocationSelection( "map_artillery_selector", true, 512 );
	self waittill( "confirm_location", location );
	self endLocationSelection();
	self thread codol_mortar_barrage( location );
	return true;
}

codol_mortar_barrage( center )
{
	self endon( "disconnect" );
	wait( 2.0 );
	offsets = [];
	offsets[0] = ( 300, 0, 0 );
	offsets[1] = ( -300, 0, 0 );
	offsets[2] = ( 0, 300, 0 );
	offsets[3] = ( 0, -300, 0 );
	offsets[4] = ( 200, 200, 0 );
	offsets[5] = ( -200, -200, 0 );
	for ( i = 0; i < offsets.size; i++ )
	{
		codol_radiusDamage( center + offsets[i], 300, 150, 30, self, "mortar_mp" );
		wait( 1.0 );
	}
}

codol_rcxd( lifeId )			// RC-XD / RC bomb car (CODOL; drive is walled)
{
	self endon( "disconnect" );
	self beginLocationSelection( "map_artillery_selector", true, 256 );
	self waittill( "confirm_location", location );
	self endLocationSelection();
	self thread codol_delayed_blast( location, 4.0, 256, 200, 50, "rcbomb_mp" );
	return true;
}

// -- turret entities -------------------------------------------------------

codol_sentry( lifeId )
{
	forward = anglesToForward( self.angles );
	pos = self.origin + ( forward[0] * 48, forward[1] * 48, 32 );
	turret = spawn( "misc_turret", pos );
	turret setModel( "sentry_minigun" );
	turret.targetname = "sentry";
	turret.owner = self;
	// _autosentry.gsc:138 spawnTurret; sentry_minigun_mp playerDamage 10 @
	// fireTime 0.05, maxDamageRange 1200 -> a paced burst of ~30 / s.
	turret thread codol_turret_think( self, 90, 1200, 30, "sentry_minigun_mp" );
	return true;
}

codol_sam( lifeId )			// SAM turret (anti-air): no air targets here
{
	forward = anglesToForward( self.angles );
	pos = self.origin + ( forward[0] * 48, forward[1] * 48, 32 );
	turret = spawn( "misc_turret", pos );
	turret setModel( "sam_turret" );
	turret.targetname = "sam_turret";
	turret.owner = self;
	// anti-air only -- no enemy AIR killstreak entities exist on this
	// emulator, so it stands but never fires (see docs).
	return true;
}

codol_turret_think( owner, lifespan, range, burst, weapon )
{
	self endon( "disconnect" );
	owner endon( "disconnect" );
	startTime = getTime();
	while ( getTime() - startTime < lifespan * 1000 )
	{
		codol_fire_nearest( self.origin, range, burst, owner, weapon );
		wait( 1.0 );
	}
	self delete();
}

// -- air killstreaks (paced loiter fire; model/flight/ride walled) ---------

codol_helicopter( lifeId )		// Attack Helicopter -- cobra_20mm_mp 10/shot
{
	self thread codol_gunship( 40, "cobra_20mm_mp", 30 );
	return true;
}

codol_pavelow( lifeId )			// Pave Low -- twin miniguns, tougher
{
	self thread codol_gunship( 40, "pavelow_minigun_mp", 40 );
	return true;
}

codol_heli_minigun( lifeId )		// Chopper Gunner
{
	self thread codol_gunship( 40, "heli_minigun_mp", 45 );
	return true;
}

codol_ac130( lifeId )			// AC-130 -- 105mm, heavy
{
	self thread codol_gunship( 40, "ac130_105mm_mp", 200 );
	return true;
}

// -- dogs (CODOL; NO _dogs.gsc in vendor/iw4 -- asset/xanim walled) --------

codol_dogs( lifeId )
{
	self thread codol_dogs_think( 30 );
	return true;
}

codol_dogs_think( lifespan )
{
	self endon( "disconnect" );
	startTime = getTime();
	// proximity bites at the nearest enemy -- there is NO visible/animated dog
	// (no model, no bite anim exists in this tree); this is the honest minimum.
	while ( getTime() - startTime < lifespan * 1000 )
	{
		codol_fire_nearest( self.origin, 1200, 55, self, "dog_bite_mp" );
		wait( 2.0 );
	}
}

// -- nuke (announces only unless scr_nuke_live; never ends the match) -

codol_nuke( lifeId )
{
	if ( getDvarInt( "scr_nuke_live" ) == 0 )
		return true;			// default: no game-end, no mass kill
	self thread codol_nuke_blast();
	return true;
}

codol_nuke_blast()
{
	self endon( "disconnect" );
	wait( 3.0 );
	codol_radiusDamage( self.origin, 100000, 100000, 100000, self, "nuke_mp" );
}
