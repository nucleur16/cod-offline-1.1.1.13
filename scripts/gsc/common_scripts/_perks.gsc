// common_scripts/_perks.gsc -- the mode-agnostic perk core: give, take and
// query perks, apply a loadout's perk slots, and the script half of the
// perk effects the engine does not apply itself.
//
// WHY A SHARED MODULE.  The perk table every category reads is ONE table,
// mp/coPerkStatsTable.csv in code_post_gfx_mp (mounted for mp, coop and zm
// alike), and the native setPerk / unsetPerk / hasPerk / clearPerks methods
// (src/codol/script/codol_script_perks.c) work in every category; the coop
// rows specialty_coop_* (ids 64-87) are rows of that same table
// (docs/perk_audit_20260925/coPerkStatsTable.rows.txt).  What was NOT
// shareable was the script around them: maps\mp\perks\_perks.gsc and
// maps\mp\_utility.gsc's _setPerk family include the multiplayer HUD and
// utility chain, so naming them from coop compiled the ~100-file multiplayer
// closure (docs/CODOL_CATEGORY_SCRIPT_SETS_20260917.md).  This file is that
// script with no dependency at all.
//
// PROVENANCE.  IW4 maps\mp\_utility.gsc _setPerk / _unsetPerk / _hasPerk /
// _clearPerks (the level.perkSetFuncs / level.perkUnsetFuncs registry), as
// adapted in this tree's maps/mp/_utility.gsc:1835-1880 (C1 SetPerk takes one
// argument); maps/mp/perks/_perks.gsc perkTableModifierDelta; and
// maps/mp/gametypes/_class.gsc codolPerkBase / loadoutAllPerks (a slot naming
// a Pro grants its base perk too).  The multiplayer files are unchanged and
// keep their own copies, so a multiplayer match compiles nothing new.
//
// WHAT THE ENGINE DOES NOT APPLY.  The perk's numeric modifiers reach C1
// through the native perk commit (CODOL_CommitPerkState), but a few are
// server-script quantities in every IW engine: modMaxHealth (coop
// specialty_coop_armorvest, +50) is self.maxhealth.  applyHealthPerks() does
// that one; the rest are applied natively or by C1.

PERK_TABLE = "mp/coPerkStatsTable.csv";

init()
{
	if ( !isDefined( level.perkSetFuncs ) )
		level.perkSetFuncs = [];
	if ( !isDefined( level.perkUnsetFuncs ) )
		level.perkUnsetFuncs = [];
}

isNullPerk( perkName )
{
	return !isDefined( perkName ) || perkName == "" || perkName == "none" || perkName == "specialty_null";
}

// IW4 _utility::_setPerk: the native slot, the script record self.perks
// (what _hasPerk reads) and the mode's set hook.
_setPerk( perkName )
{
	if ( isNullPerk( perkName ) )
		return;
	if ( !isDefined( self.perks ) )
		self.perks = [];

	self setPerk( perkName );
	self.perks[ perkName ] = true;

	if ( isDefined( level.perkSetFuncs ) && isDefined( level.perkSetFuncs[ perkName ] ) )
		self thread [[ level.perkSetFuncs[ perkName ] ]]();
}

_unsetPerk( perkName )
{
	if ( isNullPerk( perkName ) )
		return;

	self unsetPerk( perkName );
	if ( isDefined( self.perks ) )
		self.perks[ perkName ] = undefined;

	if ( isDefined( level.perkUnsetFuncs ) && isDefined( level.perkUnsetFuncs[ perkName ] ) )
		self thread [[ level.perkUnsetFuncs[ perkName ] ]]();
}

_hasPerk( perkName )
{
	return isDefined( self.perks ) && isDefined( self.perks[ perkName ] );
}

_clearPerks()
{
	if ( isDefined( self.perks ) && isDefined( level.perkUnsetFuncs ) )
	{
		keys = getArrayKeys( self.perks );
		for ( i = 0; i < keys.size; i++ )
		{
			if ( isDefined( level.perkUnsetFuncs[ keys[ i ] ] ) )
				self [[ level.perkUnsetFuncs[ keys[ i ] ] ]]();
		}
	}

	self.perks = [];
	self clearPerks();
}

// _class.gsc codolPerkBase: a Pro row (slot "upgrade<n>") names its base perk
// in the base row's column 8, so a slot naming a Pro grants that base too.
perkBase( perkName )
{
	if ( isNullPerk( perkName ) )
		return perkName;
	if ( !isSubStr( tableLookup( PERK_TABLE, 1, perkName, 5 ), "upgrade" ) )
		return perkName;
	base = tableLookup( PERK_TABLE, 8, perkName, 1 );
	if ( isNullPerk( base ) )
		return perkName;
	return base;
}

// The Pro a base perk upgrades to (column 8), or "specialty_null".
perkUpgrade( perkName )
{
	if ( isNullPerk( perkName ) )
		return "specialty_null";
	upgrade = tableLookup( PERK_TABLE, 1, perkName, 8 );
	if ( isNullPerk( upgrade ) )
		return "specialty_null";
	return upgrade;
}

// _perks.gsc perkTableModifierDelta: the delta one row gives a modifier
// (names in columns 15/17/19, each value one column to the right; 0 when the
// row does not name it or names it as a flag).
perkModifierDelta( perkName, modifier )
{
	if ( isNullPerk( perkName ) )
		return 0;
	for ( column = 15; column <= 19; column += 2 )
	{
		if ( tableLookup( PERK_TABLE, 1, perkName, column ) != modifier )
			continue;
		value = tableLookup( PERK_TABLE, 1, perkName, column + 1 );
		if ( value == "" )
			return 0;
		return float( value );
	}
	return 0;
}

// Only a name the perk table knows is handed to setPerk; the native method
// treats an unknown name as absent anyway, this keeps the record honest.
isKnownPerk( perkName )
{
	if ( isNullPerk( perkName ) )
		return false;
	return tableLookup( PERK_TABLE, 1, perkName, 0 ) != "";
}

// Give a loadout's perk slots: _class.gsc loadoutAllPerks without the
// multiplayer purchase rules.  perks is an array of slot values; a slot that
// names a Pro grants its base and the Pro (the coop class table names the Pro
// itself in its _up columns).  Clears what the player had first.  Returns the
// perks actually given, for the caller's log.
giveLoadoutPerks( perks )
{
	self _clearPerks();

	given = [];
	if ( !isDefined( perks ) )
		return given;

	for ( i = 0; i < perks.size; i++ )
	{
		selected = perks[ i ];
		if ( !isKnownPerk( selected ) )
			continue;

		base = perkBase( selected );
		if ( isKnownPerk( base ) && !self _hasPerk( base ) )
		{
			self _setPerk( base );
			given[ given.size ] = base;
		}
		if ( selected != base && !self _hasPerk( selected ) )
		{
			self _setPerk( selected );
			given[ given.size ] = selected;
		}
	}
	return given;
}

// The summed delta of one modifier over the perks the player holds.
heldModifierDelta( modifier )
{
	total = 0;
	if ( !isDefined( self.perks ) )
		return total;
	keys = getArrayKeys( self.perks );
	for ( i = 0; i < keys.size; i++ )
		total += perkModifierDelta( keys[ i ], modifier );
	return total;
}

// modMaxHealth is self.maxhealth (specialty_coop_armorvest: +50 over the
// mode's base).  Call after giveLoadoutPerks, at spawn; fills the bar.
applyHealthPerks( baseHealth )
{
	bonus = int( self heldModifierDelta( "modMaxHealth" ) );
	self.maxhealth = baseHealth + bonus;
	self.health = self.maxhealth;
	return bonus;
}

// A readable list for log lines.
perkList( perks )
{
	text = "";
	if ( !isDefined( perks ) )
		return "none";
	for ( i = 0; i < perks.size; i++ )
	{
		if ( text != "" )
			text += " ";
		text += perks[ i ];
	}
	if ( text == "" )
		return "none";
	return text;
}
