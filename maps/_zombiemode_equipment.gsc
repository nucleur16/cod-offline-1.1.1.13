// zm character equipment: the loadout each Cyborg carries, and the registry
// the item modules hook into.
//
// C1 GIVES EVERY ZOMBIES CHARACTER A FIXED KIT.  MEASURED: the mounted
// mp/cyborgclasstable.csv (dumped live with scr_equiptest "table",
// 2026-09-24; the same rows alpha0022 catalogue/zm_characters.py extracts)
// has one row per character:
//
//   0 key | 1 code | 2 sex | 3 head | 4 portrait | 5 pistol | 6 (unused)
//   | 7 lethal | 8 special | 9 trap | 10 perkpop a | 11 perkpop b
//
//   zm1 lb    m1911_zm    semtex_grenade_zm teleporter_zm          trap_shock_zm
//   zm2 danby m1911dw_zm  money_ball_zm     zombie_cymbal_monkey   trap_freeze_zm
//   zm3 hh    m1911_zm    bouncingbetty_zm  zombie_black_hole_bomb trap_fire_zm
//   zm4 lg    m1911_zm    money_ball_zm     revive_knife_zm        trap_shrink_zm
//
// Several cells carry stray tabs and spaces in the shipped table; every cell
// is trimmed.  The row is the one codol_getZombieCharacter() returns for the
// player's profile (the head C1 composes); a player with no roster character
// (a test client has no profile) takes row 0, which C1 treats as the first
// core character.
//
// THE THREE OFFHANDS ARE C1's THREE OFFHAND SLOTS.  setOffhandEquipment arms
// lethal / tactical / equipment, fired by the frag (0x4000), special (0x8000)
// and equipment (0x10000) buttons (codol_weapon_state.c
// CODOL_OFFHAND_BUTTON_*).  Every special and trap weapon above is offhand
// class 2 and every lethal class 1 or 5 (codol_checkweapon, 2026-09-24), so
// lethal goes to slot 0, the special to slot 1 and the trap to slot 2.
//
// Perk pops (columns 10/11) are not offhands: they are the zombies
// "survival streak" rewards (PERKS_PERKPOP_ZM, and they have their own rows
// with a cost in mp/coKillstreakStatsTable.csv).  _zombiemode_perkpops owns
// them; this file only records the two names.
//
// ITEM MODULES register a grenade_fire callback per weapon:
//   maps\_zombiemode_equipment::register_equipment( weapon, ::cb );
// cb runs as  <grenade> thread [[ cb ]]( <player>, <weapon> )  on the
// engine's own "grenade_fire" for that weapon, so no module watches the
// player itself.

#include maps\_zombiemode_utility;

ROSTER_TABLE = "mp/cyborgclasstable.csv";

// Callback_StartGameType, which is this server's precache window.
init()
{
	if ( !isDefined( level.zombie_equipment_callbacks ) )
		level.zombie_equipment_callbacks = [];
	level.zombie_roster = [];
	level.zombie_roster_order = [];

	for ( row = 0; row < 16; row++ )
	{
		key = trim( tableLookupByRow( ROSTER_TABLE, row, 0 ) );
		if ( key == "" )
			break;

		info = spawnStruct();
		info.key = key;
		info.portrait = resolve_portrait( trim( tableLookupByRow( ROSTER_TABLE, row, 4 ) ) );
		info.pistol = resolve_item( trim( tableLookupByRow( ROSTER_TABLE, row, 5 ) ) );
		info.lethal = resolve_item( trim( tableLookupByRow( ROSTER_TABLE, row, 7 ) ) );
		info.special = resolve_item( trim( tableLookupByRow( ROSTER_TABLE, row, 8 ) ) );
		info.trap = resolve_item( trim( tableLookupByRow( ROSTER_TABLE, row, 9 ) ) );
		info.perkpops = [];
		for ( col = 10; col <= 11; col++ )
		{
			name = trim( tableLookupByRow( ROSTER_TABLE, row, col ) );
			if ( name != "" && name != "none" )
				info.perkpops[ info.perkpops.size ] = name;
		}

		level.zombie_roster[ key ] = info;
		level.zombie_roster_order[ level.zombie_roster_order.size ] = key;
		{}
	}

	if ( level.zombie_roster_order.size == 0 )
		{}

	level thread on_player_connect();
}

// strTok drops every separator, so a cell of tabs and spaces around one token
// comes back as that token.
trim( value )
{
	if ( !isDefined( value ) )
		return "";
	tokens = strTok( value, " \t" );
	if ( tokens.size == 0 )
		return "";
	return tokens[ 0 ];
}

// Column 4 is the character's HUD portrait (hud_character_lubu, _cowboy,
// _punk, _sarge; common_gametype_zm).  C1's zombies player HUD (0x00460590)
// draws the LOCAL player's portrait from playerState 0x540, which only
// SetClientMaterial( "head_icon_self", ... ) writes, and every OTHER player's
// from that player's entity head icon (es+0xa0, i.e. self.headicon).  With
// neither set it falls back to its built-in hud_character_soldier - the one
// generic icon every player had.  C1 skips the over-the-head draw of head
// icons in a "zm" gametype (0x0045aeb6), so the head icon is only the portrait
// here.  Both precaches belong to this precache window.
resolve_portrait( name )
{
	if ( name == "" || name == "none" )
		return undefined;
	precacheShader( name );
	precacheHeadIcon( name );
	return name;
}

// A name that is not a loaded weapon on this map is left out of the kit
// rather than given as a weapon the client cannot draw.
resolve_item( name )
{
	if ( name == "" || name == "none" )
		return undefined;
	precacheItem( name );
	if ( getWeaponIndexFromName( name ) > 0 )
		return name;
	{}
	return undefined;
}

describe( name )
{
	if ( !isDefined( name ) )
		return "-";
	return name;
}

register_equipment( weapon, callback )
{
	if ( !isDefined( level.zombie_equipment_callbacks ) )
		level.zombie_equipment_callbacks = [];
	level.zombie_equipment_callbacks[ weapon ] = callback;
}

// self = player.  The roster entry for this player's character.
character_info()
{
	if ( !isDefined( level.zombie_roster_order ) || level.zombie_roster_order.size == 0 )
		return undefined;

	key = self codol_getZombieCharacter();
	if ( isDefined( key ) && isDefined( level.zombie_roster[ key ] ) )
		return level.zombie_roster[ key ];

	return level.zombie_roster[ level.zombie_roster_order[ 0 ] ];
}

// self = player.  The character's pistol, or undefined to keep the default.
character_pistol()
{
	info = self character_info();
	if ( !isDefined( info ) )
		return undefined;
	return info.pistol;
}

// self = player.  Arms the three offhand slots from the character's row.
// The lethal starts empty, as T5 round_start :3486 gives it with a zero clip
// and award_lethal_grenades fills it at each round start.
give_character_loadout()
{
	info = self character_info();
	if ( !isDefined( info ) )
		return;

	self.zombie_character = info.key;
	self publish_portrait( info );
	self.zombie_lethal = info.lethal;
	self.zombie_special = info.special;
	self.zombie_trap = info.trap;
	self.zombie_perkpop_names = info.perkpops;

	slots = [];
	slots[ 0 ] = "none";
	slots[ 1 ] = "none";
	slots[ 2 ] = "none";

	if ( isDefined( info.lethal ) )
	{
		self giveWeapon( info.lethal );
		self setWeaponAmmoClip( info.lethal, 0 );
		slots[ 0 ] = info.lethal;
	}
	if ( isDefined( info.special ) )
	{
		self giveWeapon( info.special );
		self giveStartAmmo( info.special );
		slots[ 1 ] = info.special;
	}
	if ( isDefined( info.trap ) )
	{
		self giveWeapon( info.trap );
		self giveStartAmmo( info.trap );
		slots[ 2 ] = info.trap;
	}

	self setOffhandEquipment( slots[ 0 ], slots[ 1 ], slots[ 2 ] );
}

// self = player.  The character's HUD portrait: the local player's from
// playerState 0x540, everyone else's from this player's head icon.  Both live
// in state ClientSpawn clears, so this runs on every spawn.
publish_portrait( info )
{
	if ( !isDefined( info.portrait ) )
		return;
	self.headicon = info.portrait;
	self.headiconteam = "none";
	self setClientMaterial( "head_icon_self", info.portrait );
}

// self = player.  This player's lethal, or the level default when the
// character has none (T5's single level-wide lethal).
lethal_grenade()
{
	if ( isDefined( self.zombie_lethal ) )
		return self.zombie_lethal;
	return level.zombie_lethal_grenade;
}

on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread watch_equipment_fire();
	}
}

watch_equipment_fire()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "grenade_fire", grenade, weapon );
		if ( !isDefined( grenade ) || !isDefined( weapon ) )
			continue;
		if ( !isDefined( level.zombie_equipment_callbacks[ weapon ] ) )
			continue;
		grenade thread [[ level.zombie_equipment_callbacks[ weapon ] ]]( self, weapon );
	}
}
