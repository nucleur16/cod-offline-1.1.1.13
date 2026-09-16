#include common_scripts\utility;
#include maps\mp\_utility;

/*
	maps\mp\perks\_deathstreaks.gsc  --  CODOL 1.1.1.13 deathstreaks

	PROVENANCE (2026-09-02)
	=======================
	NEW overlay file.  vendor/iw4 ships NO _deathstreaks.gsc -- the four
	deathstreaks are scattered across three vendor files, which this module
	consolidates behind one init()/onPlayerKilled/onPlayerSpawned trigger so it
	can be driven straight off the runtime's connect/spawn/death notifies:

	  * counting          vendor _damage.gsc:658  victim.pers["cur_death_streak"]++
	                      vendor _damage.gsc:286  attacker.pers["...streak"] = 0
	  * arming + splash   vendor _class.gsc:473-490 giveLoadout (deathVal == /
	                      > compare, splashNotify on the exact-equals life)
	  * thresholds        vendor mp/perkTable.csv col 6
	  * Painkiller        vendor _perkfunctions.gsc:114 setCombatHigh (maxhealth up)
	  * Martyrdom         engine drop of frag_grenade_short_mp on death
	  * Copycat           vendor _damage.gsc:235 cloneLoadout of the killer
	  * Final Stand       vendor _damage.gsc:1568 Callback_PlayerLastStand

	It reads the streak from self.pers["cur_death_streak"] (kept by the vendor
	_damage callbacks that already run in this VM), so it never double-counts.

	WIRING NOTES
	============
	Captions (DesignDoc MP #16) use setClientSubtitle(), the new builtin that
	emits the measured `] <time> <text>` channel-6 subtitle command; nothing
	else on this build can show a subtitle.  The perk ICON is setPerk().  The
	native splash is showHudSplash( row ) into this build's mp/splashtable.csv.

	Server-authoritative effects (the grenade entity, the health boost, the
	last-stand interception, the copycat loadout) are NOT the VM's to apply on
	this server -- G4State + projectile_sim own them in game-server.py.  They
	are requested through the host bridges magicGrenade()/giveDeathstreakHealth()
	/enterLastStand()/setCopycatLoadout(), which game-server.py drains.  See
	docs/subtitles_deathstreaks_fix.md for the anchored glue.
*/

PAINKILLER    = "specialty_combathigh";
MARTYRDOM     = "specialty_grenadepulldeath";
FINALSTAND    = "specialty_finalstand";
COPYCAT       = "specialty_copycat";

init()
{
	precacheShader( "specialty_painkiller" );
	precacheShader( "specialty_finalstand" );
	precacheShader( "specialty_copycat" );
	precacheShader( "specialty_grenadepulldeath" );

	// perkTable.csv col 6: consecutive deaths that arm each deathstreak
	level.deathstreakVal = [];
	level.deathstreakVal[ PAINKILLER ] = 3;
	level.deathstreakVal[ MARTYRDOM ]  = 4;
	level.deathstreakVal[ FINALSTAND ] = 4;
	level.deathstreakVal[ COPYCAT ]    = 4;

	// mp/splashtable.csv row index (this build's patch_code_post_gfx_mp,
	// parsed + validated against the known killstreak rows uav=67/nuke=79)
	level.deathstreakSplash = [];
	level.deathstreakSplash[ MARTYRDOM ]  = 61;
	level.deathstreakSplash[ FINALSTAND ] = 62;
	level.deathstreakSplash[ PAINKILLER ] = 63;
	level.deathstreakSplash[ COPYCAT ]    = 66;

	// caption reference (Localized client-side by the `]` handler); these are
	// the perk title refs vendor splashNotify() shows.
	level.deathstreakCaption = [];
	level.deathstreakCaption[ MARTYRDOM ]  = "PERKS_MARTYRDOM";
	level.deathstreakCaption[ FINALSTAND ] = "PERKS_FINALSTAND";
	level.deathstreakCaption[ PAINKILLER ] = "PERKS_PAINKILLER";
	level.deathstreakCaption[ COPYCAT ]    = "PERKS_COPYCAT";

	level thread onPlayerConnect();
}

onPlayerConnect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread onPlayerSpawned();
		player thread onPlayerKilled();
	}
}

// -------------------------------------------------------------------------
// the equipped deathstreak for this player.  The class-selection glue sets
// self.loadoutDeathstreak from the profile's F_DEATHSTREAK item; a dvar
// override drives the offline harness and a live A/B.
// -------------------------------------------------------------------------
equippedDeathstreak()
{
	if ( isDefined( self.loadoutDeathstreak ) && self.loadoutDeathstreak != ""
	     && self.loadoutDeathstreak != "specialty_null" )
		return self.loadoutDeathstreak;

	forced = getDvar( "scr_forcedeathstreak" );
	if ( isDefined( forced ) && forced != "" && forced != "specialty_null" )
		return forced;

	return undefined;
}

curDeathStreak()
{
	if ( isDefined( self.pers ) && isDefined( self.pers[ "cur_death_streak" ] ) )
		return self.pers[ "cur_death_streak" ];
	if ( isDefined( self.cur_death_streak ) )
		return self.cur_death_streak;
	return 0;
}

// -------------------------------------------------------------------------
// DEATH: martyrdom drops a live grenade; copycat captures the killer.  The
// vendor counters run in _damage, so we only read the (already-incremented)
// streak here.
// -------------------------------------------------------------------------
onPlayerKilled()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "death" );

		ds = self equippedDeathstreak();
		if ( !isDefined( ds ) )
			continue;
		if ( self curDeathStreak() < level.deathstreakVal[ ds ] )
			continue;

		if ( ds == MARTYRDOM )
		{
			// engine drop of the short-fuse frag at the corpse; the
			// GrenadeManager flies + detonates it (fuse from the weapon file).
			org = ( 0, 0, 0 );
			if ( isDefined( self.origin ) )
				org = self.origin;
			self magicGrenade( "frag_grenade_short_mp",
			                   org + ( 0, 0, 8 ), ( 0, 0, 40 ) );
		}
		else if ( ds == COPYCAT )
		{
			// clone the killer's loadout; -1 lets the damage owner resolve the
			// killer from its own last-attacker record.
			attacker = -1;
			if ( isDefined( self.attacker ) && isPlayer( self.attacker ) )
				attacker = self.attacker getEntityNumber();
			self setCopycatLoadout( attacker );
		}
	}
}

// -------------------------------------------------------------------------
// SPAWN: on the arming life, give the perk + caption + splash; painkiller and
// final stand also arm their server effect.  Mirrors _class.gsc: splash only
// on the exact-equals life, perk on every life at/over the threshold.
// -------------------------------------------------------------------------
onPlayerSpawned()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "spawned_player" );

		ds = self equippedDeathstreak();
		if ( !isDefined( ds ) )
			continue;

		streak = self curDeathStreak();
		deathVal = level.deathstreakVal[ ds ];
		if ( streak < deathVal )
			continue;

		self giveDeathstreakPerk( ds );

		// splash + caption only on the exact-equals life (vendor `==` branch)
		if ( streak == deathVal )
		{
			self nativeSplash( 0, level.deathstreakSplash[ ds ], 0 );
			self setClientSubtitle( level.deathstreakCaption[ ds ], 3 );
		}
	}
}

giveDeathstreakPerk( ds )
{
	if ( ds == PAINKILLER )
	{
		self setPerk( "specialty_combathigh" );
		// vendor setCombatHigh: maxhealth up + refill, for a brief window
		self giveDeathstreakHealth( 200, 5 );
	}
	else if ( ds == FINALSTAND )
	{
		// the perk icon; the actual last-stand interception is applied by the
		// damage owner on the next lethal round (enterLastStand host bridge).
		self setPerk( "specialty_finalstand" );
	}
	else if ( ds == COPYCAT )
	{
		self setPerk( "specialty_copycat" );
	}
	else if ( ds == MARTYRDOM )
	{
		self setPerk( "specialty_grenadepulldeath" );
	}
}
