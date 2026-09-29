// sh1x server native adaptation of IW4 _class.gsc for Call of Duty Online.
//
// CoDOL stores loadouts in mp/playerdata.def's weaponClasses[] (eight classes of
// marketplace item ids), not IW4's customClasses[].weaponSetups[]. The cac_*
// accessors below read them through the native loadout bridge
// (codol_getLoadout*), which resolves ids against mp/coMarketplaceItems.csv and
// answers "none" for an empty slot. Profile weapons are composed and published
// natively with their full attachment records; IW4 validity tables must not
// replace a CoDOL selection with a stock class-table weapon.
#include common_scripts\utility;
// check if below includes are removable
#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;

init()
{
	level.classMap["class0"] = 0;
	level.classMap["class1"] = 1;
	level.classMap["class2"] = 2;
	level.classMap["class3"] = 3;
	level.classMap["class4"] = 4;
	level.classMap["class5"] = 5;
	level.classMap["class6"] = 6;
	level.classMap["class7"] = 7;
	level.classMap["class8"] = 8;
	level.classMap["class9"] = 9;
	level.classMap["class10"] = 10;
	level.classMap["class11"] = 11;
	level.classMap["class12"] = 12;
	level.classMap["class13"] = 13;
	level.classMap["class14"] = 14;
	
	level.classMap["custom1"] = 0;
	level.classMap["custom2"] = 1;
	level.classMap["custom3"] = 2;
	level.classMap["custom4"] = 3;
	level.classMap["custom5"] = 4;
	level.classMap["custom6"] = 5;
	level.classMap["custom7"] = 6;
	level.classMap["custom8"] = 7;
	level.classMap["custom9"] = 8;
	level.classMap["custom10"] = 9;
	
	level.classMap["copycat"] = -1;
	
	/#
	// classes testclients may choose from.
	level.botClasses = [];
	level.botClasses[0] = "class0";
	level.botClasses[1] = "class0";
	level.botClasses[2] = "class0";
	level.botClasses[3] = "class0";
	level.botClasses[4] = "class0";
	#/
	
	level.defaultClass = "CLASS_ASSAULT";
	
	level.classTableName = "mp/classTable.csv";
	
	//precacheShader( "waypoint_bombsquad" );
	precacheShader( "specialty_pistoldeath" );
	precacheShader( "specialty_finalstand" );

	level thread onPlayerConnecting();
}


getClassChoice( response )
{
	assert( isDefined( level.classMap[response] ) );
	
	return response;
}

getWeaponChoice( response )
{
	tokens = strtok( response, "," );
	if ( tokens.size > 1 )
		return int(tokens[1]);
	else
		return 0;
}


logClassChoice( class, primaryWeapon, specialType, perks )
{
	if ( class == self.lastClass )
		return;

	self logstring( "choseclass: " + class + " weapon: " + primaryWeapon + " special: " + specialType );		
	for( i=0; i<perks.size; i++ )
		self logstring( "perk" + i + ": " + perks[i] );
	
	self.lastClass = class;
}


cac_getWeapon( classIndex, weaponIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "none";
	if ( weaponIndex == 0 )
		return self codol_getLoadoutItem( classIndex, "primaryBase" );
	return self codol_getLoadoutItem( classIndex, "secondaryBase" );
}

cac_getWeaponAttachment( classIndex, weaponIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "none";
	return self codol_getLoadoutAttachment( classIndex, weaponIndex, 0 );
}

cac_getWeaponAttachmentTwo( classIndex, weaponIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "none";
	return self codol_getLoadoutAttachment( classIndex, weaponIndex, 1 );
}

cac_getWeaponCamo( classIndex, weaponIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "none";
	return self codol_getLoadoutCamo( classIndex, weaponIndex );
}

// IW4 perk slot 0 is the equipment (CoDOL's lethal member), slots 1..3 are
// the perks, slot 4 is the deathstreak (its own member in CoDOL).
cac_getPerk( classIndex, perkIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "specialty_null";
	if ( perkIndex == 0 )
		return self codol_getLoadoutItem( classIndex, "lethal" );
	if ( perkIndex == 4 )
		return self codol_getLoadoutItem( classIndex, "deathstreak" );
	return self codol_getLoadoutPerk( classIndex, perkIndex - 1 );
}

cac_getKillstreak( classIndex, streakIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "none";
	return self codol_getLoadoutKillstreak( classIndex, streakIndex );
}

cac_getDeathstreak( classIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "specialty_null";
	return self codol_getLoadoutItem( classIndex, "deathstreak" );
}

cac_getOffhand( classIndex )
{
	if ( !codol_hasLoadoutClass( classIndex ) )
		return "none";
	return self codol_getLoadoutItem( classIndex, "tactical" );
}

// Preserve all three profile members. The profile's tactical/special order
// differs from the C1 button order (+frag, +smoke, +tact).
codol_readEquipment( classIndex )
{
	items = [];
	items[0] = self codol_getLoadoutItem( classIndex, "lethal" );
	items[1] = self codol_getLoadoutItem( classIndex, "tactical" );
	items[2] = self codol_getLoadoutItem( classIndex, "special" );
	return items;
}

codol_giveEquipment( items )
{
	weapons = [];
	for ( slot = 0; slot < 3; slot++ )
	{
		item = items[slot];
		weapons[slot] = "none";
		if ( item == "none" || item == "" || item == "specialty_null" )
			continue;
		if ( item == "specialty_blastshield" )
		{
			self maps\mp\perks\_perks::givePerk( item );
			continue;
		}
		if ( item == "specialty_tacticalinsertion" )
		{
			self maps\mp\perks\_perks::givePerk( item );
			weapon = "flare_mp";
		}
		else if ( isSubStr( item, "_mp" ) )
			weapon = item;
		else
			weapon = item + "_mp";
		// An equipment row with no weapon of that name (C1's call for support
		// is specialty_callforsupport; its flare_callforsupport behaviour is not
		// ported) would make giveWeapon raise a script error and stop the server.
		if ( !codol_isWeaponMounted( weapon ) )
		{
			codolPrintLn( "CoDOL loadout: " + self.name + " equipment " + item + " has no weapon " + weapon + "; skipped" );
			continue;
		}
		self giveWeapon( weapon );
		self giveStartAmmo( weapon );
		weapons[slot] = weapon;
	}
	self setOffhandEquipment( weapons[0], weapons[2], weapons[1] );
	self.codolEquipment = items;
}

// weaponClasses[] holds eight records; IW4's classMap also names custom9/10.
codol_hasLoadoutClass( classIndex )
{
	return ( isDefined( classIndex ) && classIndex >= 0 && classIndex < 8 );
}



table_getWeapon( tableName, classIndex, weaponIndex )
{
	if ( weaponIndex == 0 )
		return tableLookup( tableName, 0, "loadoutPrimary", classIndex + 1 );
	else
		return tableLookup( tableName, 0, "loadoutSecondary", classIndex + 1 );
}

table_getWeaponAttachment( tableName, classIndex, weaponIndex, attachmentIndex )
{
	tempName = "none";
	
	if ( weaponIndex == 0 )
	{
		if ( !isDefined( attachmentIndex ) || attachmentIndex == 0 )
			tempName = tableLookup( tableName, 0, "loadoutPrimaryAttachment", classIndex + 1 );
		else
			tempName = tableLookup( tableName, 0, "loadoutPrimaryAttachment2", classIndex + 1 );
	}
	else
	{
		if ( !isDefined( attachmentIndex ) || attachmentIndex == 0 )
			tempName = tableLookup( tableName, 0, "loadoutSecondaryAttachment", classIndex + 1 );
		else
			tempName = tableLookup( tableName, 0, "loadoutSecondaryAttachment2", classIndex + 1 );
	}
	
	if ( tempName == "" || tempName == "none" )
		return "none";
	else
		return tempName;
	
	
}

table_getWeaponCamo( tableName, classIndex, weaponIndex )
{
	if ( weaponIndex == 0 )
		return tableLookup( tableName, 0, "loadoutPrimaryCamo", classIndex + 1 );
	else
		return tableLookup( tableName, 0, "loadoutSecondaryCamo", classIndex + 1 );
}

table_getEquipment( tableName, classIndex, perkIndex )
{
	assert( perkIndex < 5 );
	return tableLookup( tableName, 0, "loadoutEquipment", classIndex + 1 );
}

table_getPerk( tableName, classIndex, perkIndex )
{
	assert( perkIndex < 5 );
	return tableLookup( tableName, 0, "loadoutPerk" + perkIndex, classIndex + 1 );
}

table_getOffhand( tableName, classIndex )
{
	return tableLookup( tableName, 0, "loadoutOffhand", classIndex + 1 );
}

table_getKillstreak( tableName, classIndex, streakIndex )
{
//	return tableLookup( tableName, 0, "loadoutStreak" + streakIndex, classIndex + 1 );
	return ( "none" );
}

table_getDeathstreak( tableName, classIndex )
{
	return tableLookup( tableName, 0, "loadoutDeathstreak", classIndex + 1 );
}

getClassIndex( className )
{
	assert( isDefined( level.classMap[className] ) );
	
	return level.classMap[className];
}

/*
getPerk( perkIndex )
{
	if( isSubstr( self.pers["class"], "CLASS_CUSTOM" ) )
		return cac_getPerk( self.class_num, perkIndex );
	else
		return table_getPerk( level.classTableName, self.class_num, perkIndex );	
}

getWeaponCamo( weaponIndex )
{
	if( isSubstr( self.pers["class"], "CLASS_CUSTOM" ) )
		return cac_getWeaponCamo( self.class_num, weaponIndex );
	else
		return table_getWeaponCamo( level.classTableName, self.class_num, weaponIndex );	
}
*/

cloneLoadout()
{
	clonedLoadout = [];
	
	class = self.curClass;
	
	if ( class == "copycat" )
		return ( undefined );
	
	if ( isDefined( self.codolCombatBot ) && self.codolCombatBot )
	{
		// Copycat must clone the bot's CSV class, not the human class0 table.
		class_num = self.codolBotClass;
		loadoutPrimary = self codol_getBotLoadoutItem( class_num, "loadoutPrimary" );
		loadoutSecondary = self codol_getBotLoadoutItem( class_num, "loadoutSecondary" );
		loadoutPrimaryAttachment = "none";
		loadoutPrimaryAttachment2 = "none";
		loadoutSecondaryAttachment = "none";
		loadoutSecondaryAttachment2 = "none";
		loadoutPrimaryCamo = "none";
		loadoutSecondaryCamo = "none";
		loadoutEquipment = "specialty_null";
		loadoutPerk1 = self codol_getBotLoadoutItem( class_num, "loadoutPerk1" );
		loadoutPerk2 = self codol_getBotLoadoutItem( class_num, "loadoutPerk2" );
		loadoutPerk3 = self codol_getBotLoadoutItem( class_num, "loadoutPerk3" );
		loadoutOffhand = "none";
	}
	else if( isSubstr( class, "custom" ) )
	{
		class_num = getClassIndex( class );

		loadoutPrimaryAttachment2 = "none";
		loadoutSecondaryAttachment2 = "none";

		loadoutPrimary = cac_getWeapon( class_num, 0 );
		loadoutPrimaryAttachment = cac_getWeaponAttachment( class_num, 0 );
		loadoutPrimaryAttachment2 = cac_getWeaponAttachmentTwo( class_num, 0 );
		loadoutPrimaryCamo = cac_getWeaponCamo( class_num, 0 );
		loadoutSecondaryCamo = cac_getWeaponCamo( class_num, 1 );
		loadoutSecondary = cac_getWeapon( class_num, 1 );
		loadoutSecondaryAttachment = cac_getWeaponAttachment( class_num, 1 );
		loadoutSecondaryAttachment2 = cac_getWeaponAttachmentTwo( class_num, 1 );
		loadoutSecondaryCamo = cac_getWeaponCamo( class_num, 1 );
		loadoutEquipment = cac_getPerk( class_num, 0 );
		loadoutPerk1 = cac_getPerk( class_num, 1 );
		loadoutPerk2 = cac_getPerk( class_num, 2 );
		loadoutPerk3 = cac_getPerk( class_num, 3 );
		loadoutOffhand = cac_getOffhand( class_num );
		loadoutDeathStreak = cac_getDeathstreak( class_num );
	}
	else
	{
		class_num = getClassIndex( class );
		
		loadoutPrimary = table_getWeapon( level.classTableName, class_num, 0 );
		loadoutPrimaryAttachment = table_getWeaponAttachment( level.classTableName, class_num, 0 , 0);
		loadoutPrimaryAttachment2 = table_getWeaponAttachment( level.classTableName, class_num, 0, 1 );
		loadoutPrimaryCamo = table_getWeaponCamo( level.classTableName, class_num, 0 );
		loadoutSecondary = table_getWeapon( level.classTableName, class_num, 1 );
		loadoutSecondaryAttachment = table_getWeaponAttachment( level.classTableName, class_num, 1 , 0);
		loadoutSecondaryAttachment2 = table_getWeaponAttachment( level.classTableName, class_num, 1, 1 );;
		loadoutSecondaryCamo = table_getWeaponCamo( level.classTableName, class_num, 1 );
		loadoutEquipment = table_getEquipment( level.classTableName, class_num, 0 );
		loadoutPerk1 = table_getPerk( level.classTableName, class_num, 1 );
		loadoutPerk2 = table_getPerk( level.classTableName, class_num, 2 );
		loadoutPerk3 = table_getPerk( level.classTableName, class_num, 3 );
		loadoutOffhand = table_getOffhand( level.classTableName, class_num );
		loadoutDeathstreak = table_getDeathstreak( level.classTableName, class_num );
	}
	
	clonedLoadout["inUse"] = false;
	clonedLoadout["loadoutPrimary"] = loadoutPrimary;
	clonedLoadout["loadoutPrimaryAttachment"] = loadoutPrimaryAttachment;
	clonedLoadout["loadoutPrimaryAttachment2"] = loadoutPrimaryAttachment2;
	clonedLoadout["loadoutPrimaryCamo"] = loadoutPrimaryCamo;
	clonedLoadout["loadoutSecondary"] = loadoutSecondary;
	clonedLoadout["loadoutSecondaryAttachment"] = loadoutSecondaryAttachment;
	clonedLoadout["loadoutSecondaryAttachment2"] = loadoutSecondaryAttachment2;
	clonedLoadout["loadoutSecondaryCamo"] = loadoutSecondaryCamo;
	clonedLoadout["loadoutEquipment"] = loadoutEquipment;
	clonedLoadout["loadoutPerk1"] = loadoutPerk1;
	clonedLoadout["loadoutPerk2"] = loadoutPerk2;
	clonedLoadout["loadoutPerk3"] = loadoutPerk3;
	clonedLoadout["loadoutOffhand"] = loadoutOffhand;
	// Preserve the recipes actually equipped, including all attachment slots.
	clonedLoadout["primaryRecipe"] = self.primaryWeapon;
	clonedLoadout["secondaryRecipe"] = self.secondaryWeapon;
	if ( isDefined( self.codolEquipment ) )
		clonedLoadout["codolEquipment"] = self.codolEquipment;
	
	return ( clonedLoadout );
}

giveLoadout( team, class, allowCopycat )
{
	self takeAllWeapons();
	botLoadout = isDefined( self.codolCombatBot ) && self.codolCombatBot;
	profileWeapons = false;
	primaryRecipe = "none";
	secondaryRecipe = "none";
	profileEquipment = undefined;
	self.codolEquipment = undefined;
	
	primaryIndex = 0;
	
	// initialize specialty array
	self.specialty = [];

	if ( !isDefined( allowCopycat ) )
		allowCopycat = true;

	primaryWeapon = undefined;

	if ( botLoadout )
    {
        self.class_num = self.codolBotClass;
        primaryRecipe = self codol_getBotLoadoutWeapon( self.class_num, 0 );
        secondaryRecipe = self codol_getBotLoadoutWeapon( self.class_num, 1 );
        profileWeapons = true;
        loadoutPrimary = self codol_getBotLoadoutItem( self.class_num, "loadoutPrimary" );
        loadoutSecondary = self codol_getBotLoadoutItem( self.class_num, "loadoutSecondary" );
        loadoutPrimaryAttachment = "none";
        loadoutPrimaryAttachment2 = "none";
        loadoutSecondaryAttachment = "none";
        loadoutSecondaryAttachment2 = "none";
        loadoutPrimaryCamo = "none";
        loadoutSecondaryCamo = "none";
        loadoutEquipment = "specialty_null";
        loadoutPerk1 = self codol_getBotLoadoutItem( self.class_num, "loadoutPerk1" );
        loadoutPerk2 = self codol_getBotLoadoutItem( self.class_num, "loadoutPerk2" );
        loadoutPerk3 = self codol_getBotLoadoutItem( self.class_num, "loadoutPerk3" );
        loadoutDeathStreak = self codol_getBotLoadoutItem( self.class_num, "loadoutDeathStreak" );
        loadoutOffhand = "none";
        profileEquipment = [];
        profileEquipment[0] = self codol_getBotLoadoutItem( self.class_num, "loadoutLethal" );
        profileEquipment[1] = self codol_getBotLoadoutItem( self.class_num, "loadoutTactical" );
        profileEquipment[2] = self codol_getBotLoadoutItem( self.class_num, "loadoutSpecial" );
    }
    else if ( isDefined( self.pers["copyCatLoadout"] ) && self.pers["copyCatLoadout"]["inUse"] && allowCopycat )
	{
		self maps\mp\gametypes\_class::setClass( "copycat" );
		self.class_num = getClassIndex( "copycat" );

		clonedLoadout = self.pers["copyCatLoadout"];
		if ( isDefined( clonedLoadout["codolEquipment"] ) )
			profileEquipment = clonedLoadout["codolEquipment"];
		if ( isDefined( clonedLoadout["primaryRecipe"] ) && isDefined( clonedLoadout["secondaryRecipe"] ) )
		{
			primaryRecipe = clonedLoadout["primaryRecipe"];
			secondaryRecipe = clonedLoadout["secondaryRecipe"];
			profileWeapons = true;
		}

		loadoutPrimary = clonedLoadout["loadoutPrimary"];
		loadoutPrimaryAttachment = clonedLoadout["loadoutPrimaryAttachment"];
		loadoutPrimaryAttachment2 = clonedLoadout["loadoutPrimaryAttachment2"] ;
		loadoutPrimaryCamo = clonedLoadout["loadoutPrimaryCamo"];
		loadoutSecondary = clonedLoadout["loadoutSecondary"];
		loadoutSecondaryAttachment = clonedLoadout["loadoutSecondaryAttachment"];
		loadoutSecondaryAttachment2 = clonedLoadout["loadoutSecondaryAttachment2"];
		loadoutSecondaryCamo = clonedLoadout["loadoutSecondaryCamo"];
		loadoutEquipment = clonedLoadout["loadoutEquipment"];
		loadoutPerk1 = clonedLoadout["loadoutPerk1"];
		loadoutPerk2 = clonedLoadout["loadoutPerk2"];
		loadoutPerk3 = clonedLoadout["loadoutPerk3"];
		loadoutOffhand = clonedLoadout["loadoutOffhand"];
		loadoutDeathStreak = "specialty_copycat";		
	}
	else if ( isSubstr( class, "custom" ) )
	{
		class_num = getClassIndex( class );
		self.class_num = class_num;
		// Read the selected profile class, never a pinned weapon/class zero.
		profileEquipment = codol_readEquipment( class_num );
		// The native getter validates, builds and publishes the whole recipe.
		primaryRecipe = self codol_getLoadoutWeapon( class_num, 0 );
		secondaryRecipe = self codol_getLoadoutWeapon( class_num, 1 );
		profileWeapons = true;

		loadoutPrimary = cac_getWeapon( class_num, 0 );
		loadoutPrimaryAttachment = cac_getWeaponAttachment( class_num, 0 );
		loadoutPrimaryAttachment2 = cac_getWeaponAttachmentTwo( class_num, 0 );
		loadoutPrimaryCamo = cac_getWeaponCamo( class_num, 0 );
		loadoutSecondaryCamo = cac_getWeaponCamo( class_num, 1 );
		loadoutSecondary = cac_getWeapon( class_num, 1 );
		loadoutSecondaryAttachment = cac_getWeaponAttachment( class_num, 1 );
		loadoutSecondaryAttachment2 = cac_getWeaponAttachmentTwo( class_num, 1 );
		loadoutSecondaryCamo = cac_getWeaponCamo( class_num, 1 );
		loadoutEquipment = cac_getPerk( class_num, 0 );
		loadoutPerk1 = cac_getPerk( class_num, 1 );
		loadoutPerk2 = cac_getPerk( class_num, 2 );
		loadoutPerk3 = cac_getPerk( class_num, 3 );
		loadoutOffhand = cac_getOffhand( class_num );
		loadoutDeathStreak = cac_getDeathstreak( class_num );
	}
	else
	{
		class_num = getClassIndex( class );
		self.class_num = class_num;
		
		loadoutPrimary = table_getWeapon( level.classTableName, class_num, 0 );
		loadoutPrimaryAttachment = table_getWeaponAttachment( level.classTableName, class_num, 0 , 0);
		loadoutPrimaryAttachment2 = table_getWeaponAttachment( level.classTableName, class_num, 0, 1 );
		loadoutPrimaryCamo = table_getWeaponCamo( level.classTableName, class_num, 0 );
		loadoutSecondaryCamo = table_getWeaponCamo( level.classTableName, class_num, 1 );
		loadoutSecondary = table_getWeapon( level.classTableName, class_num, 1 );
		loadoutSecondaryAttachment = table_getWeaponAttachment( level.classTableName, class_num, 1 , 0);
		loadoutSecondaryAttachment2 = table_getWeaponAttachment( level.classTableName, class_num, 1, 1 );;
		loadoutSecondaryCamo = table_getWeaponCamo( level.classTableName, class_num, 1 );
		loadoutEquipment = table_getEquipment( level.classTableName, class_num, 0 );
		loadoutPerk1 = table_getPerk( level.classTableName, class_num, 1 );
		loadoutPerk2 = table_getPerk( level.classTableName, class_num, 2 );
		loadoutPerk3 = table_getPerk( level.classTableName, class_num, 3 );
		loadoutOffhand = table_getOffhand( level.classTableName, class_num );
		loadoutDeathstreak = table_getDeathstreak( level.classTableName, class_num );
	}

	if ( !(isDefined( self.pers["copyCatLoadout"] ) && self.pers["copyCatLoadout"]["inUse"] && allowCopycat) )
	{
		isCustomClass = botLoadout || isSubstr( class, "custom" );
		if ( !profileWeapons )
		{
		
		if ( !isValidPrimary( loadoutPrimary ) || (isCustomClass && !self isItemUnlocked( loadoutPrimary )) )
			loadoutPrimary = table_getWeapon( level.classTableName, 10, 0 );
		
		if ( !isValidAttachment( loadoutPrimaryAttachment ) || (isCustomClass && !self isItemUnlocked( loadoutPrimary + " " + loadoutPrimaryAttachment )) )
			loadoutPrimaryAttachment = table_getWeaponAttachment( level.classTableName, 10, 0 , 0);
		
		if ( !isValidAttachment( loadoutPrimaryAttachment2 ) || (isCustomClass && !self isItemUnlocked( loadoutPrimary + " " + loadoutPrimaryAttachment2 )) )
			loadoutPrimaryAttachment2 = table_getWeaponAttachment( level.classTableName, 10, 0, 1 );
		
		if ( !isValidCamo( loadoutPrimaryCamo ) || (isCustomClass && !self isItemUnlocked( loadoutPrimary + " " + loadoutPrimaryCamo )) )
			loadoutPrimaryCamo = table_getWeaponCamo( level.classTableName, 10, 0 );
		
		if ( !isValidSecondary( loadoutSecondary ) || (isCustomClass && !self isItemUnlocked( loadoutSecondary )) )
			loadoutSecondary = table_getWeapon( level.classTableName, 10, 1 );
		
		if ( !isValidAttachment( loadoutSecondaryAttachment ) || (isCustomClass && !self isItemUnlocked( loadoutSecondary + " " + loadoutSecondaryAttachment )) )
			loadoutSecondaryAttachment = table_getWeaponAttachment( level.classTableName, 10, 1 , 0);
		
		if ( !isValidAttachment( loadoutSecondaryAttachment2 ) || (isCustomClass && !self isItemUnlocked( loadoutSecondary + " " + loadoutSecondaryAttachment2 )) )
			loadoutSecondaryAttachment2 = table_getWeaponAttachment( level.classTableName, 10, 1, 1 );;
		
		if ( !isValidCamo( loadoutSecondaryCamo ) || (isCustomClass && !self isItemUnlocked( loadoutSecondary + " " + loadoutSecondaryCamo )) )
			loadoutSecondaryCamo = table_getWeaponCamo( level.classTableName, 10, 1 );
		}
		
		if ( !isDefined( profileEquipment ) && (!isValidEquipment( loadoutEquipment ) || (isCustomClass && !self isItemUnlocked( loadoutEquipment ))) )
			loadoutEquipment = table_getEquipment( level.classTableName, 10, 0 );
		
		// Custom getters validate CoDOL marketplace type and pk1/pk2/pk3.
		// IW4's lists put Lightweight in a different slot and omit C1 perks.
		if ( !isCustomClass && !isValidPerk1( loadoutPerk1 ) )
			loadoutPerk1 = table_getPerk( level.classTableName, 10, 1 );
		
		if ( !isCustomClass && !isValidPerk2( loadoutPerk2 ) )
			loadoutPerk2 = table_getPerk( level.classTableName, 10, 2 );
		
		if ( !isCustomClass && !isValidPerk3( loadoutPerk3 ) )
			loadoutPerk3 = table_getPerk( level.classTableName, 10, 3 );
		
		if ( !isDefined( profileEquipment ) && !isValidOffhand( loadoutOffhand ) )
			loadoutOffhand = table_getOffhand( level.classTableName, 10 );
		
		if ( !isCustomClass && !isValidDeathstreak( loadoutDeathstreak ) )
			loadoutDeathstreak = table_getDeathstreak( level.classTableName, 10 );
	}

	if ( loadoutPerk1 != "specialty_bling" )
	{
		loadoutPrimaryAttachment2 = "none";
		loadoutSecondaryAttachment2 = "none";
	}
	
	if ( loadoutPerk1 != "specialty_onemanarmy" && loadoutSecondary == "onemanarmy" )
		loadoutSecondary = table_getWeapon( level.classTableName, 10, 1 );

	loadoutSecondaryCamo = "none";


	if ( level.killstreakRewards )
	{
        if ( botLoadout )
        {
            loadoutKillstreak1 = self codol_getBotLoadoutItem( self.class_num, "loadoutKillstreak1" );
            loadoutKillstreak2 = self codol_getBotLoadoutItem( self.class_num, "loadoutKillstreak2" );
            loadoutKillstreak3 = self codol_getBotLoadoutItem( self.class_num, "loadoutKillstreak3" );
        }
        else
        {
		// CoDOL keeps killstreaks per class; IW4 kept one global set.
		loadoutKillstreak1 = cac_getKillstreak( self.class_num, 0 );
		loadoutKillstreak2 = cac_getKillstreak( self.class_num, 1 );
		loadoutKillstreak3 = cac_getKillstreak( self.class_num, 2 );
        }
	}
	else
	{
		loadoutKillstreak1 = "none";
		loadoutKillstreak2 = "none";
		loadoutKillstreak3 = "none";
	}
	
	if ( profileWeapons )
		secondaryName = secondaryRecipe;
	else
		secondaryName = buildWeaponName( loadoutSecondary, loadoutSecondaryAttachment, loadoutSecondaryAttachment2 );
	if ( secondaryName != "none" )
		self _giveWeapon( secondaryName, int(tableLookup( "mp/camoTable.csv", 1, loadoutSecondaryCamo, 0 ) ) );

	self.loadoutPrimaryCamo = int(tableLookup( "mp/camoTable.csv", 1, loadoutPrimaryCamo, 0 ));
	self.loadoutPrimary = loadoutPrimary;
	self.loadoutSecondary = loadoutSecondary;
	self.loadoutSecondaryCamo = int(tableLookup( "mp/camoTable.csv", 1, loadoutSecondaryCamo, 0 ));
	
	self SetOffhandPrimaryClass( "other" );
	
	// Action Slots
    // C1 action slots are independent from all three offhand classes.
    // Selected rewards own1..3, final slot5 owns package rewards; slot4 reserved.
    for ( actionSlot = 1; actionSlot <= 5; actionSlot++ )
        self _SetActionSlot( actionSlot, "" );

	// Perks
	self _clearPerks();
	self _detachAll();
	// updateMoveSpeedScale multiplies the weapon-class scale by this value.
	// A first spawn has no previous perk callback to initialize it. Lightweight
	// leaves it at 1: its modMoveSpeedScale is applied natively.
	self.moveSpeedScaler = 1;
	
	// these special case giving pistol death have to come before
	// perk loadout to ensure player perk icons arent overwritten
	if ( level.dieHardMode )
		self maps\mp\perks\_perks::givePerk( "specialty_pistoldeath" );
	
	// C1's class tables spell an empty deathstreak "none" (mp/classtable.csv
	// classes 11+, mp/botclasstable.csv); without this it read as a streak of
	// 0 deaths and every spawn gave perk "none" and queued a "none" splash.
	if ( !isDefined( loadoutDeathStreak ) || loadoutDeathStreak == "none" || loadoutDeathStreak == "" )
		loadoutDeathStreak = "specialty_null";

	// only give the deathstreak for the initial spawn for this life.
	if ( loadoutDeathStreak != "specialty_null" && getTime() == self.spawnTime )
	{
		// coPerkStatsTable column 6: deaths in a row that earn it (4 for all
		// four C1 deathstreaks: Copycat, Painkiller, Martyrdom, Final Stand).
		deathVal = int( tableLookup( "mp/coPerkStatsTable.csv", 1, loadoutDeathStreak, 6 ) );
				
		// Hardline Pro's modDeathstreakShift (PERKS_DESC_ROLLOVER: deaths
		// needed for a deathstreak -1). Perks are cleared above and granted
		// below, so the selected aggregate does not exist yet; the rows the
		// loadout will grant supply the shift from the table instead.
		deathVal += self loadoutModifierDelta( "modDeathstreakShift", loadoutPerk1, loadoutPerk2, loadoutPerk3 );
		
		if ( deathVal <= 0 )
			codolPrintLn( "CoDOL deathstreak: " + self.name + " " + loadoutDeathStreak + " has no death count in mp/coPerkStatsTable.csv; not granted" );
		else if ( self.pers["cur_death_streak"] == deathVal )
		{
			self thread maps\mp\perks\_perks::givePerk( loadoutDeathStreak );
			self thread maps\mp\gametypes\_hud_message::splashNotify( loadoutDeathStreak );
		}
		else if ( self.pers["cur_death_streak"] > deathVal )
		{
			self thread maps\mp\perks\_perks::givePerk( loadoutDeathStreak );
		}
	}

	if ( isDefined( profileEquipment ) )
		self loadoutAllPerks( "specialty_null", loadoutPerk1, loadoutPerk2, loadoutPerk3 );
	else
		self loadoutAllPerks( loadoutEquipment, loadoutPerk1, loadoutPerk2, loadoutPerk3 );
		
	self setKillstreaks( loadoutKillstreak1, loadoutKillstreak2, loadoutKillstreak3 );
		
	// Hacker/Engineer modObjectiveUseTimeScale (1 + 0.5). The C1 texts
	// (PERKS_DESC_HACKER, PERKS_DESC_ENGINEER) say it makes planting,
	// defusing and capturing faster, so the value is the rate multiplier
	// _gameobjects applies (useRate *= objectiveScaler), as IW4 Saboteur's
	// 1.2 was.
	self.objectiveScaler = 1;
	objectiveScale = self getSelectedPerkModifierValue( "modObjectiveUseTimeScale" );
	if ( isDefined( objectiveScale ) && objectiveScale > 0 )
	{
		self.objectiveScaler = objectiveScale;
		codolPrintLn( "CoDOL perks: " + self.name + " objectiveScaler " + objectiveScale );
	}

	if ( self hasPerk( "specialty_extraammo", true ) && getWeaponClass( secondaryName ) != "weapon_projectile" )
		self giveMaxAmmo( secondaryName );

	// Primary Weapon
	if ( profileWeapons )
		primaryName = primaryRecipe;
	else
		primaryName = buildWeaponName( loadoutPrimary, loadoutPrimaryAttachment, loadoutPrimaryAttachment2 );
	if ( primaryName != "none" )
		self _giveWeapon( primaryName, self.loadoutPrimaryCamo );
	
	// fix changing from a riotshield class to a riotshield class during grace period not giving a shield
	if ( primaryName == "riotshield_mp" && level.inGracePeriod )
		self notify ( "weapon_change", "riotshield_mp" );

	if ( self hasPerk( "specialty_extraammo", true ) )
		self giveMaxAmmo( primaryName );

	if ( primaryName != "none" )
		self setSpawnWeapon( primaryName );
	else if ( secondaryName != "none" )
		self setSpawnWeapon( secondaryName );
	
	self.pers["primaryWeapon"] = getBaseWeaponName( primaryName );
	
	// Primary Offhand was given by givePerk (it's your perk1)
	
	// Secondary Offhand
	if ( isDefined( profileEquipment ) )
		codol_giveEquipment( profileEquipment );
	else
	{
	offhandSecondaryWeapon = loadoutOffhand + "_mp";
	if ( loadoutOffhand == "flash_grenade" )
		self SetOffhandSecondaryClass( "flash" );
	else
		self SetOffhandSecondaryClass( "smoke" );
	
	self giveWeapon( offhandSecondaryWeapon );
	if( loadOutOffhand == "smoke_grenade" )
		self setWeaponAmmoClip( offhandSecondaryWeapon, 1 );
	else if( loadOutOffhand == "flash_grenade" )
		self setWeaponAmmoClip( offhandSecondaryWeapon, 2 );
	else if( loadOutOffhand == "concussion_grenade" )
		self setWeaponAmmoClip( offhandSecondaryWeapon, 2 );
	else
		self setWeaponAmmoClip( offhandSecondaryWeapon, 1 );
	
	}
	// Scavenger Pro (modExtraExplosive): one extra starting frag / semtex.
	self maps\mp\perks\_codol_equipperks::giveExtraExplosive();
	primaryWeapon = primaryName;
	self.primaryWeapon = primaryWeapon;
	self.secondaryWeapon = secondaryName;

	self maps\mp\gametypes\_teams::playerModelForWeapon( self.pers["primaryWeapon"], getBaseWeaponName( secondaryName ) );
		
	self maps\mp\gametypes\_weapons::updateMoveSpeedScale( "primary" );

	// cac specialties that require loop threads
	self maps\mp\perks\_perks::cac_selector();
	
	self notify ( "changed_kit" );
	self notify ( "giveLoadout" );
}

_detachAll()
{
	if ( isDefined( self.hasRiotShield ) && self.hasRiotShield )
	{
		if ( self.hasRiotShieldEquipped )
		{
			self DetachShieldModel( "weapon_riot_shield_mp", "tag_weapon_left" );
			self.hasRiotShieldEquipped = false;
		}
		else
		{
			self DetachShieldModel( "weapon_riot_shield_mp", "tag_shield_back" );
		}
		
		self.hasRiotShield = false;
	}
	
	self detachAll();
}

isPerkUpgraded( perkName )
{
	return self getPerkUpgrade( perkName ) != "specialty_null";
}

/*
	CoDOL: a Pro perk is earned, never assumed.  C1 names the gate of every
	upgradable perk on the base perk's own mp/coPerkStatsTable.csv row: the
	challenge in column 13 and the tier in column 14 (ch_flakjacket_pro, 4).
	The client's unlock flags say the same thing (uf_specialty_flakjacket_pro =
	ch_flakjacket_pro tier 4), and the lobby grants a challenge-gated flag when
	the derived challenge state exceeds the tier -- IW4's strictly-greater
	rule.  The upgrade row itself (column 8) carries no gate.

	IW4's mp/unlocktable.csv cannot answer this: it has no row at all for
	flakjacket_pro, fastoffhand, engineerpro, quickswap, paint_pro, hackerpro
	or marksmanpro, and isItemUnlocked answers an unlisted item "unlocked", so
	every Flak Jacket, Quickdraw and Engineer (all perk 2) came out Pro and the
	spawn perk card, which draws getperk( "upgrade2" ), showed it.  The rows it
	does have name IW4 challenges (Stalker Pro on ch_scrambler_pro).

	A row with no challenge (Hacker) has no C1 unlock flag either, so nothing
	can earn it; a player without a bound profile, or a challenge that cannot
	be resolved, is locked too (codol_tryChallengeState says why).
*/
perkUpgradeUnlocked( perkName )
{
	challenge = tablelookup( "mp/coPerkStatsTable.csv", 1, perkName, 13 );
	tier = int( tablelookup( "mp/coPerkStatsTable.csv", 1, perkName, 14 ) );
	if ( challenge == "" || tier <= 0 )
		return false;

	state = self codol_tryChallengeState( challenge );
	if ( !isDefined( state ) )
	{
		codolPrintLn( "CoDOL perks: " + self.name + " cannot resolve " + challenge + " for " + perkName + " Pro; not granted" );
		return false;
	}
	return state > tier;
}

// Sum of one modifier's deltas over the rows a loadout grants: each selected
// perk and, as loadoutAllPerks does, its unlocked upgrade.
loadoutModifierDelta( modifier, loadoutPerk1, loadoutPerk2, loadoutPerk3 )
{
	perks = [];
	perks[0] = loadoutPerk1;
	perks[1] = loadoutPerk2;
	perks[2] = loadoutPerk3;
	total = 0;
	foreach ( perk in perks )
	{
		if ( !isDefined( perk ) || perk == "" || perk == "specialty_null" )
			continue;
		total += maps\mp\perks\_perks::perkTableModifierDelta( perk, modifier );
		upgrade = self getPerkUpgrade( perk );
		if ( upgrade != "specialty_null" )
			total += maps\mp\perks\_perks::perkTableModifierDelta( upgrade, modifier );
	}
	return total;
}

getPerkUpgrade( perkName )
{
	// CSV bot classes grant selected perks, not account-earned upgrades.
	if ( self.isbot )
		return "specialty_null";
	if ( !isDefined( perkName ) || perkName == "" || perkName == "specialty_null" )
		return "specialty_null";
	perkUpgrade = tablelookup( "mp/coPerkStatsTable.csv", 1, perkName, 8 );
	
	if ( perkUpgrade == "" || perkUpgrade == "specialty_null" )
		return "specialty_null";
		
	if ( !self perkUpgradeUnlocked( perkName ) )
		return "specialty_null";
		
	return ( perkUpgrade );
}

// Last Stand (specialty_pistoldeath, item 3018) and its Pro
// (specialty_laststandoffhand, 3036) have coPerkStatsTable rows 14/29, but C1's
// mp/coMarketplaceItems.csv (patch_code_post_gfx_mp) marks both not offered
// (column 6 = 0, as for the zm-only perks and Hacker; the other MP perks
// have 1), and the user confirms CoDOL MP has no Last Stand perk.
// Going down in CoDOL MP is only the Final Stand deathstreak.
codolOfferedPerk( perkName )
{
	if ( perkName != "specialty_pistoldeath" && perkName != "specialty_laststandoffhand" )
		return perkName;
	codolPrintLn( "CoDOL perks: " + self.name + " loadout names " + perkName + ", which CoDOL MP does not offer; no perk" );
	return "specialty_null";
}

loadoutAllPerks( loadoutEquipment, loadoutPerk1, loadoutPerk2, loadoutPerk3 )
{
	loadoutEquipment = maps\mp\perks\_perks::validatePerk( 1, loadoutEquipment );
	loadoutPerk1 = maps\mp\perks\_perks::validatePerk( 1, loadoutPerk1 );
	loadoutPerk2 = maps\mp\perks\_perks::validatePerk( 2, loadoutPerk2 );
	loadoutPerk3 = maps\mp\perks\_perks::validatePerk( 3, loadoutPerk3 );

	// A profile, class table or copied (Copycat) class can still name a perk
	// C1 does not offer in multiplayer; it is replaced by no perk.
	loadoutPerk1 = self codolOfferedPerk( loadoutPerk1 );
	loadoutPerk2 = self codolOfferedPerk( loadoutPerk2 );
	loadoutPerk3 = self codolOfferedPerk( loadoutPerk3 );

	self maps\mp\perks\_perks::givePerk( loadoutEquipment );
	self maps\mp\perks\_perks::givePerk( loadoutPerk1 );
	self maps\mp\perks\_perks::givePerk( loadoutPerk2 );
	self maps\mp\perks\_perks::givePerk( loadoutPerk3 );
	
	// Bot perks come entirely from the class CSV.
	if ( self.isbot )
		return;
	perkUpgrd[0] = self getPerkUpgrade( loadoutPerk1 );
	perkUpgrd[1] = self getPerkUpgrade( loadoutPerk2 );
	perkUpgrd[2] = self getPerkUpgrade( loadoutPerk3 );
	
	foreach( upgrade in perkUpgrd )
	{
		if ( upgrade != "specialty_null" )
			self maps\mp\perks\_perks::givePerk( upgrade );
	}

}

trackRiotShield()
{
	self endon ( "death" );
	self endon ( "disconnect" );

	self.hasRiotShield = self hasWeapon( "riotshield_mp" );
	self.hasRiotShieldEquipped = (self.currentWeaponAtSpawn == "riotshield_mp");
	
	// note this function must play nice with _detachAll().
	
	if ( self.hasRiotShield )
	{
		if ( self.hasRiotShieldEquipped )
		{
			self AttachShieldModel( "weapon_riot_shield_mp", "tag_weapon_left" );
		}
		else
		{
			self AttachShieldModel( "weapon_riot_shield_mp", "tag_shield_back" );
		}
	}
	
	for ( ;; )
	{
		self waittill ( "weapon_change", newWeapon );
		
		if ( newWeapon == "riotshield_mp" )
		{
			// defensive check in case we somehow get an extra "weapon_change"
			if ( self.hasRiotShieldEquipped )
				continue;
			
			if ( self.hasRiotShield )
				self MoveShieldModel( "weapon_riot_shield_mp", "tag_shield_back", "tag_weapon_left" );
			else
				self AttachShieldModel( "weapon_riot_shield_mp", "tag_weapon_left" );
			
			self.hasRiotShield = true;
			self.hasRiotShieldEquipped = true;
		}
		else if ( (self IsMantling()) && (newWeapon == "none") )
		{
			// Do nothing, we want to keep that weapon on their arm.
		}
		else if ( self.hasRiotShieldEquipped )
		{
			assert( self.hasRiotShield );
			self.hasRiotShield = self hasWeapon( "riotshield_mp" );
			
			if ( self.hasRiotShield )
				self MoveShieldModel( "weapon_riot_shield_mp", "tag_weapon_left", "tag_shield_back" );
			else
				self DetachShieldModel( "weapon_riot_shield_mp", "tag_weapon_left" );
			
			self.hasRiotShieldEquipped = false;
		}
		else if ( self.hasRiotShield )
		{
			if ( !self hasWeapon( "riotshield_mp" ) )
			{
				// we probably just lost all of our weapons (maybe switched classes)
				self DetachShieldModel( "weapon_riot_shield_mp", "tag_shield_back" );
				self.hasRiotShield = false;
			}
		}
	}
}


tryAttach( placement ) // deprecated; hopefully we won't need to bring this defensive function back
{
	if ( !isDefined( placement ) || placement != "back" )
		tag = "tag_weapon_left";
	else
		tag = "tag_shield_back";
	
	attachSize = self getAttachSize();
	
	for ( i = 0; i < attachSize; i++ )
	{
		attachedTag = self getAttachTagName( i );
		if ( attachedTag == tag &&  self getAttachModelName( i ) == "weapon_riot_shield_mp" )
		{
			return;
		}
	}
	
	self AttachShieldModel( "weapon_riot_shield_mp", tag );
}

tryDetach( placement ) // deprecated; hopefully we won't need to bring this defensive function back
{
	if ( !isDefined( placement ) || placement != "back" )
		tag = "tag_weapon_left";
	else
		tag = "tag_shield_back";
	
	
	attachSize = self getAttachSize();
	
	for ( i = 0; i < attachSize; i++ )
	{
		attachedModel = self getAttachModelName( i );
		if ( attachedModel == "weapon_riot_shield_mp" )
		{
			self DetachShieldModel( attachedModel, tag);
			return;
		}
	}
	return;
}



buildWeaponName( baseName, attachment1, attachment2 )
{
	if ( !isDefined( level.letterToNumber ) )
		level.letterToNumber = makeLettersToNumbers();

	// disable bling when perks are disabled
	if ( getDvarInt ( "scr_game_perks" ) == 0 )
	{
		attachment2 = "none";

		if ( baseName == "onemanarmy" )
			return ( "beretta_mp" );
	}

	weaponName = baseName;
	attachments = [];

	if ( attachment1 != "none" && attachment2 != "none" )
	{
		if ( level.letterToNumber[attachment1[0]] < level.letterToNumber[attachment2[0]] )
		{
			
			attachments[0] = attachment1;
			attachments[1] = attachment2;
			
		}
		else if ( level.letterToNumber[attachment1[0]] == level.letterToNumber[attachment2[0]] )
		{
			if ( level.letterToNumber[attachment1[1]] < level.letterToNumber[attachment2[1]] )
			{
				attachments[0] = attachment1;
				attachments[1] = attachment2;
			}
			else
			{
				attachments[0] = attachment2;
				attachments[1] = attachment1;
			}	
		}
		else
		{
			attachments[0] = attachment2;
			attachments[1] = attachment1;
		}		
	}
	else if ( attachment1 != "none" )
	{
		attachments[0] = attachment1;
	}
	else if ( attachment2 != "none" )
	{
		attachments[0] = attachment2;	
	}
	
	foreach ( attachment in attachments )
	{
		weaponName += "_" + attachment;
	}

	if ( !isValidWeapon( weaponName + "_mp" ) )
		return ( baseName + "_mp" );
	else
		return ( weaponName + "_mp" );
}


makeLettersToNumbers()
{
	array = [];
	
	array["a"] = 0;
	array["b"] = 1;
	array["c"] = 2;
	array["d"] = 3;
	array["e"] = 4;
	array["f"] = 5;
	array["g"] = 6;
	array["h"] = 7;
	array["i"] = 8;
	array["j"] = 9;
	array["k"] = 10;
	array["l"] = 11;
	array["m"] = 12;
	array["n"] = 13;
	array["o"] = 14;
	array["p"] = 15;
	array["q"] = 16;
	array["r"] = 17;
	array["s"] = 18;
	array["t"] = 19;
	array["u"] = 20;
	array["v"] = 21;
	array["w"] = 22;
	array["x"] = 23;
	array["y"] = 24;
	array["z"] = 25;
	
	return array;
}

// A selection that is undefined (a getter that raised), empty or has no
// mp/coKillstreakStatsTable.csv row is no selection: a bad profile value must
// cost that slot, not the match (int( tableLookup( ..., undefined, ... ) )
// is a fatal script error inside spawnClient).
codol_validKillstreak( streak, slot )
{
	if ( !isDefined( streak ) )
	{
		codolPrintLn( "CoDOL loadout: " + self.name + " killstreak slot " + slot + " is undefined; using none" );
		return "none";
	}
	if ( streak == "none" || streak == "" || streak == "specialty_null" )
		return "none";
	if ( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak, 4 ) == "" )
	{
		codolPrintLn( "CoDOL loadout: " + self.name + " killstreak " + streak + " has no mp/coKillstreakStatsTable.csv row; using none" );
		return "none";
	}
	return streak;
}

setKillstreaks( streak1, streak2, streak3 )
{
	streak1 = self codol_validKillstreak( streak1, 0 );
	streak2 = self codol_validKillstreak( streak2, 1 );
	streak3 = self codol_validKillstreak( streak3, 2 );
	self.killStreaks = [];
	// Keep the actual selected class order for account challenge consumers.
	// CoDOL has no IW4 global playerdata("killstreaks") selection array.
	self.codolSelectedKillstreaks = [];
	self.codolSelectedKillstreaks[0] = streak1;
	self.codolSelectedKillstreaks[1] = streak2;
	self.codolSelectedKillstreaks[2] = streak3;
	self.firstKillstreakEarned = undefined;
	// C1 has five HUD entries; clear stale entries on every class change.
	for ( hudSlot = 0; hudSlot < 5; hudSlot++ )
	{
		self setKillstreakID( 0, hudSlot );
		self setKillstreakTarget( 0, hudSlot );
	}

	modifier = self getSelectedPerkModifierValue( "modKillstreakShift" );
	if ( !isDefined( modifier ) )
		modifier = 0;
	modifier = int( modifier );
	
	/*if ( streak1 == "none" && streak2 == "none" && streak3 == "none" )
	{
		streak1 = "uav";
		streak2 = "precision_airstrike";
		streak3 = "helicopter";
	}*/

	killStreaks = [];

	if ( streak1 != "none" )
	{
		//if ( !level.splitScreen )
			streakVal = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak1, 4 ) );
		//else
		//	streakVal = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak1, 5 ) );
		killStreaks[streakVal + modifier] = streak1;
	}

	if ( streak2 != "none" )
	{
		//if ( !level.splitScreen )
			streakVal = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak2, 4 ) );
		//else
		//	streakVal = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak2, 5 ) );
		killStreaks[streakVal + modifier] = streak2;
	}

	if ( streak3 != "none" )
	{
		//if ( !level.splitScreen )
			streakVal = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak3, 4 ) );
		//else
		//	streakVal = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streak3, 5 ) );
		killStreaks[streakVal + modifier] = streak3;
	}


	// Sort by required kills before publishing the selected rewards.
	maxVal = 0;
	foreach ( streakVal, streakName in killStreaks )
	{
		if ( streakVal > maxVal )
			maxVal = streakVal;
	}

	hudSlot = 0;
	for ( streakIndex = 0; streakIndex <= maxVal; streakIndex++ )
	{
		if ( !isDefined( killStreaks[streakIndex] ) )
			continue;
			
		streakName = killStreaks[streakIndex];
			
		self.killStreaks[ streakIndex ] = killStreaks[ streakIndex ];

	}
    // HUD index0 belongs to action slot1 (extra care-package reward).
    // Three selected rewards occupy action slots2-4 / HUD indices1-3.
    // The fifth action slot does not grant a fourth loadout reward.
    for ( hudSlot = 0; hudSlot < 3; hudSlot++ )
    {
        streakName = self.codolSelectedKillstreaks[hudSlot];
        if ( !isDefined( streakName ) || streakName == "none" )
            continue;
        streakID = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streakName, 0 ) );
        streakTarget = int( tableLookup( "mp/coKillstreakStatsTable.csv", 1, streakName, 4 ) ) + modifier;
        self setKillstreakID( streakID, hudSlot + 1 );
        self setKillstreakTarget( streakTarget, hudSlot + 1 );
    }
	// end lameness

	// defcon rollover
	maxRollOvers = 10;
	// GSC arrays share storage on assignment. Build a separate destination so
	// later cycles never enumerate the rollover entries added by earlier ones.
	newKillstreaks = [];
	foreach ( streakVal, streakName in self.killstreaks )
		newKillstreaks[streakVal] = streakName;
	for ( rollOver = 1; rollOver <= maxRollOvers; rollOver++ )
	{
		foreach ( streakVal, streakName in self.killstreaks )
		{
			newKillstreaks[ streakVal + (maxVal*rollOver) ] = streakName + "-rollover" + rollOver;
		}
	}
	
	self.killstreaks = newKillstreaks;
}


replenishLoadout() // used by ammo hardpoint.
{
	team = self.pers["team"];
	class = self.pers["class"];

    weaponsList = self GetWeaponsListAll();
    for( idx = 0; idx < weaponsList.size; idx++ )
    {
		weapon = weaponsList[idx];

		self giveMaxAmmo( weapon );
		self SetWeaponAmmoClip( weapon, 9999 );

		if ( weapon == "claymore_mp" || weapon == "claymore_detonator_mp" )
			self setWeaponAmmoStock( weapon, 2 );
    }
	
	if ( self getAmmoCount( level.classGrenades[class]["primary"]["type"] ) < level.classGrenades[class]["primary"]["count"] )
 		self SetWeaponAmmoClip( level.classGrenades[class]["primary"]["type"], level.classGrenades[class]["primary"]["count"] );

	if ( self getAmmoCount( level.classGrenades[class]["secondary"]["type"] ) < level.classGrenades[class]["secondary"]["count"] )
 		self SetWeaponAmmoClip( level.classGrenades[class]["secondary"]["type"], level.classGrenades[class]["secondary"]["count"] );	
}


onPlayerConnecting()
{
	for(;;)
	{
		level waittill( "connected", player );

		if ( !isDefined( player.pers["class"] ) )
		{
			player.pers["class"] = "";
		}
		player.class = player.pers["class"];
		player.lastClass = "";
		// Spawn scoring runs before the first class selection and reads this as
		// a boolean. IW4 normally establishes it during class setup.
		player.detectExplosives = false;
		player.bombSquadIcons = [];
		player.bombSquadIds = [];
	}
}


fadeAway( waitDelay, fadeDelay )
{
	wait waitDelay;
	
	self fadeOverTime( fadeDelay );
	self.alpha = 0;
}


setClass( newClass )
{
	self.curClass = newClass;
}

getPerkForClass( perkSlot, className )
{
    // This query uses slots0..2; CSV/profile loadout names use1..3.
    if ( className == "copycat" && isDefined( self.pers["copyCatLoadout"] ) )
        return self.pers["copyCatLoadout"]["loadoutPerk" + ( perkSlot + 1 )];
    if ( isDefined( self.codolCombatBot ) && self.codolCombatBot )
        return self codol_getBotLoadoutItem( self.codolBotClass, "loadoutPerk" + ( perkSlot + 1 ) );
    class_num = getClassIndex( className );

    if( isSubstr( className, "custom" ) )
        return cac_getPerk( class_num, perkSlot + 1 );
    else
        return table_getPerk( level.classTableName, class_num, perkSlot + 1 );
}


classHasPerk( className, perkName )
{
	return( getPerkForClass( 0, className ) == perkName || getPerkForClass( 1, className ) == perkName || getPerkForClass( 2, className ) == perkName );
}

isValidPrimary( refString )
{
	switch ( refString )
	{
		case "riotshield":
		case "ak47":
		case "m16":
		case "m4":
		case "fn2000":
		case "masada":
		case "famas":
		case "fal":
		case "scar":
		case "tavor":
		case "mp5k":
		case "uzi":
		case "p90":
		case "kriss":
		case "ump45":
		case "barrett":
		case "wa2000":
		case "m21":
		case "cheytac":
		case "rpd":
		case "sa80":
		case "mg4":
		case "m240":
		case "aug":
			return true;
		default:
			assertMsg( "Replacing invalid primary weapon: " + refString );
			return false;
	}
}

isValidSecondary( refString )
{
	switch ( refString )
	{
		case "beretta":
		case "usp":
		case "deserteagle":
		case "coltanaconda":
		case "glock":
		case "beretta393":
		case "pp2000":
		case "tmp":
		case "m79":
		case "rpg":
		case "at4":
		case "stinger":
		case "javelin":
		case "ranger":
		case "model1887":
		case "striker":
		case "aa12":
		case "m1014":
		case "spas12":
		case "onemanarmy":
			return true;
		default:
			assertMsg( "Replacing invalid secondary weapon: " + refString );
			return false;
	}
}

isValidAttachment( refString )
{
	switch ( refString )
	{
		case "none":
		case "acog":
		case "reflex":
		case "silencer":
		case "grip":
		case "gl":
		case "akimbo":
		case "thermal":
		case "shotgun":
		case "heartbeat":
		case "fmj":
		case "rof":
		case "xmags":
		case "eotech":  
		case "tactical":
			return true;
		default:
			assertMsg( "Replacing invalid equipment weapon: " + refString );
			return false;
	}
}

isValidCamo( refString )
{
	switch ( refString )
	{
		case "none":
		case "woodland":
		case "desert":
		case "arctic":
		case "digital":
		case "red_urban":
		case "red_tiger":
		case "blue_tiger":
		case "orange_fall":
			return true;
		default:
			assertMsg( "Replacing invalid camo: " + refString );
			return false;
	}
}

isValidEquipment( refString )
{
	switch ( refString )
	{
		case "frag_grenade_mp":
		case "semtex_mp":
		case "throwingknife_mp":
		case "specialty_tacticalinsertion":
		case "specialty_blastshield":
		case "claymore_mp":
		case "c4_mp":
			return true;
		default:
			assertMsg( "Replacing invalid equipment: " + refString );
			return false;
	}
}


isValidOffhand( refString )
{
	switch ( refString )
	{
		case "flash_grenade":
		case "concussion_grenade":
		case "smoke_grenade":
			return true;
		default:
			assertMsg( "Replacing invalid offhand: " + refString );
			return false;
	}
}

isValidPerk1( refString )
{
	switch ( refString )
	{
		case "specialty_marathon":
		case "specialty_fastreload":
		case "specialty_scavenger":
		case "specialty_bling":
		case "specialty_onemanarmy":
			return true;
		default:
			assertMsg( "Replacing invalid perk1: " + refString );
			return false;
	}
}

isValidPerk2( refString )
{
	switch ( refString )
	{
		case "specialty_bulletdamage":
		case "specialty_lightweight":
		case "specialty_hardline":
		case "specialty_coldblooded":
		case "specialty_explosivedamage":
			return true;
		default:
			assertMsg( "Replacing invalid perk2: " + refString );
			return false;
	}
}

isValidPerk3( refString )
{
	switch ( refString )
	{
		case "specialty_extendedmelee":
		case "specialty_bulletaccuracy":
		case "specialty_localjammer":
		case "specialty_heartbreaker":
		case "specialty_detectexplosive":
			return true;
		default:
			assertMsg( "Replacing invalid perk3: " + refString );
			return false;
	}
}

isValidDeathStreak( refString )
{
	switch ( refString )
	{
		case "specialty_null":
		case "specialty_copycat":
		case "specialty_combathigh":
		case "specialty_grenadepulldeath":
		case "specialty_finalstand":
			return true;
		default:
			assertMsg( "Replacing invalid death streak: " + refString );
			return false;
	}
}

isValidWeapon( refString )
{
	if ( !isDefined( level.weaponRefs ) )
	{
		level.weaponRefs = [];

		foreach ( weaponRef in level.weaponList )
			level.weaponRefs[ weaponRef ] = true;
	}

	if ( isDefined( level.weaponRefs[ refString ] ) )
		return true;

	assertMsg( "Replacing invalid weapon/attachment combo: " + refString );
	
	return false;
}
