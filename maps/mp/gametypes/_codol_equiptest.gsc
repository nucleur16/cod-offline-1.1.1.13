// CoDOL equipment test hooks, shared by multiplayer and zombies.
//
// No C1 client is needed: a test client is made to press the real offhand
// button through codol_botPressButtons, so the engine's own weapon code does
// the throw and raises the real "grenade_fire". Nothing here simulates an
// effect. Every hook is idle unless its dvar is set.
//
//   set scr_codol_equiptest "<clientnum> <slot> <weapon>"
//       slot 0 lethal (frag button), 1 tactical, 2 equipment. Arms <weapon>
//       in that slot with the other two kept, then holds the slot's button.
//   set scr_codol_equiptest "addbots <n>"
//       adds n test clients (zombies only; multiplayer has scr_codol_bots).
//   set scr_codol_equiptest "down <clientnum>"
//       puts a client into last stand through level.codolEquipTestDown
//       (zombies sets it; idle elsewhere).
//   set scr_codol_equiptest "place <clientnum> <anchor clientnum> <dist>"
//       moves a client <dist> units in front of the anchor client, on the
//       ground (a target for thrown equipment).
//   set scr_codol_equiptest "warp <clientnum> <x> <y> <z>"
//       moves a client to a position.
//   set scr_codol_equiptest "togrenade <clientnum>"
//       moves a client to where the last thrown grenade is (or came to rest).
//   set scr_codol_equiptest "tozombie <clientnum> <dist>"
//       stands a client <dist> units west of the nearest live zombie in
//       level.zombies, facing it (a target for a thrown lethal).
//   set scr_codol_equiptest "knife <thrower> <victim>"
//       arms <thrower> with throwingknife_mp, stands <victim> 300 units in
//       front of it and throws the knife at the victim's chest; the knife's
//       position is logged twice a second for 12 s. A trailing "tough"
//       lets the victim survive the knife and kills it 2.5 s later.
//   set scr_codol_equiptest "knifeat <thrower> <x> <y> <z>"
//       the same throw at a point; "knifeat <thrower> wall 0 0" aims at
//       the first wall 250..700 units away, "... ground 0 0" at the floor.
//   set scr_codol_equiptest "knifepick <client>"
//       stands a client on the last knife and holds use; logs its knife ammo
//       before and after.
//   set scr_codol_equiptest "car <clientnum> <dist>"
//       freezes a client <dist> units from the nearest intact destructible
//       vehicle and damages the car until it burns and explodes, logging the
//       server time of the explosion and of the client's death (pair with
//       codol_soundTrace 1 for the sound events).
//   set scr_codol_equiptest "state <clientnum>"
//       prints a client's alive flag, session state, health and team.
//   set scr_codol_equiptest "weapflags <clientnum>" | "weapflags name <weapon>"
//       prints the C1 attachment flags (thermal, gps, idlock, hpbt) and
//       script mods of each held weapon, or of one weapon name.
//   set scr_codol_equiptest "gpsdrop <owner> <picker> <weapon>"
//       <owner> drops <weapon> through the death-drop path and <picker>
//       picks it up with the use button (multiplayer only).
//   set scr_codol_equiptest "gpstrack <holder> [<owner> <weapon>]"
//       starts a GPS track directly, or reports a holder's track abilities.
//
// Deliberately free of maps\mp\killstreaks and maps\mp\gametypes\_class so
// the zombies chain can include it without compiling the multiplayer one.

init()
{
    if ( isDefined( level.codolEquipTestStarted ) )
        return;
    level.codolEquipTestStarted = true;
    setDvar( "scr_codol_equiptest", "" );
    level thread watchDvar();
    level thread logGrenadeFire();
}

watchDvar()
{
	// Test hooks (they can set health, freeze players and grant rewards) only run
	// on a developer server; the live server runs developer 0.
	if ( getDvarInt( "developer" ) <= 0 )
		return;
    level endon( "game_ended" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_equiptest" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_equiptest", "" );
        tokens = strTok( request, " " );
        if ( tokens[0] == "addbots" && tokens.size > 1 )
        {
            level thread addTestBots( int( tokens[1] ) );
            continue;
        }
        if ( tokens[0] == "table" && tokens.size > 1 )
        {
            dumpTable( tokens[1] );
            continue;
        }
        if ( tokens[0] == "down" || tokens[0] == "place" || tokens[0] == "warp" || tokens[0] == "togrenade" ||
            tokens[0] == "tozombie" )
        {
            positionVerb( tokens );
            continue;
        }
        if ( ( tokens[0] == "knife" && tokens.size > 2 ) || ( tokens[0] == "knifeat" && tokens.size > 4 ) ||
            ( tokens[0] == "knifepick" && tokens.size > 1 ) )
        {
            level thread knifeTrial( tokens );
            continue;
        }
        // "car <client> <dist>": stands a client <dist> units from the nearest
        // destructible vehicle and shoots the car with small radius damage
        // until it burns and explodes; logs the server time of each step.
        if ( tokens[0] == "car" && tokens.size > 2 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread carTrial( float( tokens[2] ) );
            else
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
            continue;
        }
        // "pipe <client>": stands a client beside the nearest shootable pipe
        // and hits the pipe once (a "damage" notify at the middle of the
        // pipe), so its hit sound and steam/water loop start.
        if ( tokens[0] == "pipe" && tokens.size > 1 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread pipeTrial();
            else
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
            continue;
        }
        if ( tokens[0] == "perk" && tokens.size > 2 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread keepPerk( tokens[2] );
            else
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
            continue;
        }
        if ( tokens[0] == "state" && tokens.size > 1 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
            {
                team = "none";
                if ( isDefined( player.team ) )
                    team = player.team;
                codolPrintLn( "CoDOL equiptest: state " + player.name + " alive " + isAlive( player ) + " session " +
                    player.sessionstate + " health " + player.health + " maxhealth " + player.maxhealth + " team " + team );
            }
            continue;
        }
        // "akimbo <client> <C1 button mask> <seconds> <weapon ...>": arm the
        // weapon (a composed name may contain spaces), hold the buttons and
        // log each hand's magazine every server frame. See akimboTrial.
        if ( tokens[0] == "akimbo" && tokens.size > 4 )
        {
            player = playerByNumber( int( tokens[1] ) );
            weapon = tokens[4];
            for ( i = 5; i < tokens.size; i++ )
                weapon = weapon + " " + tokens[i];
            if ( isDefined( player ) )
                player thread akimboTrial( weapon, int( tokens[2] ), float( tokens[3] ) );
            else
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
            continue;
        }
        // "akimbosprint <client> <delay> <seconds>": empty the held akimbo
        // weapon's left gun, let its reload run <delay> seconds, then sprint
        // forward for <seconds>, logging both magazines. See akimboSprintTrial.
        if ( tokens[0] == "akimbosprint" && tokens.size > 3 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread akimboSprintTrial( float( tokens[2] ), float( tokens[3] ) );
            else
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
            continue;
        }
        // "weapflags <client>": each held weapon with the C1 script-only
        // definition flags and script mods.
        // "weapflags name <weapon ...>": the same for one weapon name.
        if ( tokens[0] == "weapflags" && tokens.size > 2 && tokens[1] == "name" )
        {
            weapon = tokens[2];
            for ( i = 3; i < tokens.size; i++ )
                weapon = weapon + " " + tokens[i];
            weaponFlagsLine( "name", weapon );
            continue;
        }
        if ( tokens[0] == "weapflags" && tokens.size > 1 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player weaponFlagsReport();
            else
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
            continue;
        }
        // "gpstrack <holder> <owner> <weapon ...>": give <holder> the weapon
        // (a composed name may contain spaces) and start the GPS track that a
        // pickup of <owner>'s dropped weapon starts; "gpstrack <holder>" alone
        // reports the holder's two compass abilities.
        if ( tokens[0] == "gpstrack" && tokens.size > 1 )
        {
            holder = playerByNumber( int( tokens[1] ) );
            if ( !isDefined( holder ) )
            {
                codolPrintLn( "CoDOL equiptest: no client " + tokens[1] );
                continue;
            }
            if ( tokens.size > 3 && isDefined( level.codolEquipTestGpsTrack ) )
            {
                owner = playerByNumber( int( tokens[2] ) );
                weapon = tokens[3];
                for ( i = 4; i < tokens.size; i++ )
                    weapon = weapon + " " + tokens[i];
                if ( isDefined( owner ) )
                {
                    holder giveWeapon( weapon );
                    holder switchToWeapon( weapon );
                    holder thread [[ level.codolEquipTestGpsTrack ]]( owner, weapon );
                }
            }
            wait 0.3;
            codolPrintLn( "CoDOL equiptest: gpstrack " + holder.name + " (" + holder getEntityNumber() + ", " + holder.team +
                ") alive " + isAlive( holder ) + " ownerTrack " + holder hasAbilityModifier( "modOwnerTrack" ) +
                " radarArrow " + holder hasAbilityModifier( "modRadarArrow" ) );
            continue;
        }
        // "gpsdrop <owner> <picker> <weapon ...>": <owner> drops <weapon>
        // through the death-drop path, then <picker> stands on it and holds
        // use, so the real pickup notify drives the GPS track.
        if ( tokens[0] == "gpsdrop" && tokens.size > 3 && isDefined( level.codolEquipTestDropWeapon ) )
        {
            owner = playerByNumber( int( tokens[1] ) );
            picker = playerByNumber( int( tokens[2] ) );
            weapon = tokens[3];
            for ( i = 4; i < tokens.size; i++ )
                weapon = weapon + " " + tokens[i];
            if ( isDefined( owner ) && isDefined( picker ) )
                level thread gpsDropTrial( owner, picker, weapon );
            continue;
        }
        if ( tokens[0] == "turretwatch" && tokens.size > 1 )
        {
            level thread turretWatch( float( tokens[1] ) );
            continue;
        }
        // "perkpop <client> <verb> ...": zombies perk-pop hooks, reached
        // through a pointer so this shared file names no zombies module.
        if ( tokens[0] == "perkpop" && tokens.size > 2 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) && isDefined( level.codolEquipTestPerkpop ) )
                level thread [[ level.codolEquipTestPerkpop ]]( player, tokens );
            else
                codolPrintLn( "CoDOL equiptest: perkpop needs a zombies match and client " + tokens[1] );
            continue;
        }
        if ( tokens.size < 3 )
            continue;
        player = playerByNumber( int( tokens[0] ) );
        if ( !isDefined( player ) )
        {
            codolPrintLn( "CoDOL equiptest: no client " + tokens[0] );
            continue;
        }
        player thread throwEquipment( int( tokens[1] ), tokens[2] );
    }
}

// "perk <client> <perkName>": the native C1 setPerk on that client now and
// after each later spawn (the loadout clears perks on every spawn), so an
// effect of that perk can be observed across a bot's lives.
keepPerk( perkName )
{
    self endon( "disconnect" );
    self notify( "codol_equiptest_keepperk_" + perkName );
    self endon( "codol_equiptest_keepperk_" + perkName );
    for ( ;; )
    {
        if ( isAlive( self ) )
        {
            self setPerk( perkName );
            codolPrintLn( "CoDOL equiptest: " + self.name + " (" + self getEntityNumber() + ", " + self.team +
                ") perk " + perkName + " hasPerk " + self hasPerk( perkName ) +
                " noSentry " + self hasAbilityModifier( "modNoSentryGuns" ) +
                " bulletPaint " + self hasAbilityModifier( "modBulletPaint" ) +
                " explosivePaint " + self hasAbilityModifier( "modExplosivePaint" ) );
        }
        self waittill( "spawned_player" );
        wait 0.1;
    }
}

gpsDropTrial( owner, picker, weapon )
{
    owner giveWeapon( weapon );
    owner giveMaxAmmo( weapon );
    owner switchToWeapon( weapon );
    wait 1.0;
    owner.lastDroppableWeapon = weapon;
    owner.droppedDeathWeapon = undefined;
    owner [[ level.codolEquipTestDropWeapon ]]( undefined );
    wait 0.5;
    items = getEntArray( "weapon_" + weapon, "classname" );
    codolPrintLn( "CoDOL equiptest: gpsdrop " + owner.name + " dropped '" + weapon + "' items " + items.size );
    if ( !items.size )
        return;
    for ( i = 0; i < 12; i++ )
    {
        if ( !isDefined( items[0] ) || !isAlive( picker ) )
            break;
        picker setOrigin( items[0].origin );
        picker codol_botPressButtons( 40, 0.5 );   // 0x08 activate | 0x20 use-reload
        wait 0.25;
    }
    codolPrintLn( "CoDOL equiptest: gpsdrop " + picker.name + " has it " + picker hasWeapon( weapon ) +
        " ownerTrack " + picker hasAbilityModifier( "modOwnerTrack" ) +
        " radarArrow " + picker hasAbilityModifier( "modRadarArrow" ) );
}

weaponFlagsReport()
{
    foreach ( weapon in self GetWeaponsListAll() )
        weaponFlagsLine( self.name, weapon );
}

weaponFlagsLine( label, weapon )
{
    codolPrintLn( "CoDOL equiptest: weapflags " + label + " '" + weapon + "' thermal " + weaponHasThermalScope( weapon ) +
        " gps " + codol_weaponHasGPS( weapon ) + " idlock " + codol_weaponHasIDLock( weapon ) +
        " hpbt " + codol_weaponHasHPBT( weapon ) + " mods '" + getWeaponScriptMods( weapon ) + "'" );
}

// "turretwatch <seconds>": twice a second, every live turret in level.turrets
// and the target it is tracking, with that target's modNoSentryGuns bit.
turretWatch( seconds )
{
    level endon( "game_ended" );
    for ( elapsed = 0; elapsed < seconds; elapsed += 0.5 )
    {
        if ( isDefined( level.turrets ) )
        {
            foreach ( turret in level.turrets )
            {
                if ( !isDefined( turret ) )
                    continue;
                target = turret getTurretTarget( false );
                if ( !isDefined( target ) )
                    continue;
                report = "CoDOL equiptest: turret " + turret getEntityNumber() + " targets ent " + target getEntityNumber();
                if ( isPlayer( target ) )
                    report += " " + target.name + " (" + target.team + ") noSentry " + target hasAbilityModifier( "modNoSentryGuns" );
                codolPrintLn( report );
            }
        }
        wait 0.5;
    }
}

playerByNumber( clientNum )
{
    foreach ( player in level.players )
    {
        if ( player getEntityNumber() == clientNum )
            return player;
    }
    return undefined;
}

// C1 offhand slot buttons (codol_weapon_state.c CODOL_OFFHAND_BUTTON_*).
slotButton( slot )
{
    if ( slot == 1 )
        return 32768;   // 0x8000 tactical / special grenade
    if ( slot == 2 )
        return 65536;   // 0x10000 equipment
    return 16384;       // 0x4000 lethal / frag
}

throwEquipment( slot, weapon )
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
    {
        codolPrintLn( "CoDOL equiptest: " + self.name + " never came back alive" );
        return;
    }
    if ( getWeaponIndexFromName( weapon ) <= 0 )
    {
        codolPrintLn( "CoDOL equiptest: weapon " + weapon + " is not loaded on this map" );
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = self getOffhandEquipment( i );
    slots[slot] = weapon;
    self giveWeapon( weapon );
    self giveStartAmmo( weapon );
    self setOffhandEquipment( slots[0], slots[1], slots[2] );
    codolPrintLn( "CoDOL equiptest: " + self.name + " throws " + weapon + " from slot " + slot +
        " (ammo " + self getAmmoCount( weapon ) + ")" );
    wait 0.5;
    self pressUntilThrown( slotButton( slot ), weapon );
}

// The engine refuses an offhand while the weapon is busy (firing, reloading,
// switching, sprinting), exactly as it would a player's press; keep pressing
// until it throws, as a player would.
pressUntilThrown( button, weapon )
{
    self endon( "disconnect" );
    self endon( "death" );
    for ( attempt = 1; attempt <= 6; attempt++ )
    {
        self.codolLastThrown = undefined;
        self codol_botPressButtons( button, 0.4 );
        wait 1.5;
        if ( isDefined( self.codolLastThrown ) && self.codolLastThrown == weapon )
            return true;
    }
    codolPrintLn( "CoDOL equiptest: " + self.name + " never threw " + weapon );
    return false;
}

logGrenadeFire()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread logPlayerGrenadeFire();
        player thread logPlayerFlashbang();
    }
}

// The native flash blast (C1 522580) notifies (origin, distance, angle,
// attacker, team); log it so a thrown flash can be checked end to end.
logPlayerFlashbang()
{
    self endon( "disconnect" );
    for ( ;; )
    {
        self waittill( "flashbang", origin, amountDistance, amountAngle, attacker, team );
        who = "undefined";
        if ( isDefined( attacker ) && isPlayer( attacker ) )
            who = attacker.name;
        codolPrintLn( "CoDOL equiptest: flashbang on " + self.name + " origin " + origin + " distance " +
            amountDistance + " angle " + amountAngle + " attacker " + who );
    }
}

logPlayerGrenadeFire()
{
    self endon( "disconnect" );
    for ( ;; )
    {
        self waittill( "grenade_fire", grenade, weapon );
        self.codolLastThrown = weapon;
        where = "(no entity)";
        if ( isDefined( grenade ) )
            where = "" + grenade.origin;
        if ( isDefined( grenade ) )
            level thread trackLastGrenade( grenade );
        codolPrintLn( "CoDOL equiptest: grenade_fire " + weapon + " by " + self.name + " at " + where );
    }
}

// set scr_codol_equiptest "table <name>": every row (first 24 columns) of a mounted string table.
dumpTable( table )
{
    for ( row = 0; row < 256; row++ )
    {
        first = tableLookupByRow( table, row, 0 );
        if ( !isDefined( first ) || first == "" )
            break;
        line = "";
        for ( col = 0; col < 24; col++ )
            line = line + tableLookupByRow( table, row, col ) + "|";
        codolPrintLn( "CoDOL equiptest: " + table + " row " + row + ": " + line );
    }
}

addTestBots( count )
{
    level endon( "game_ended" );
    for ( i = 0; i < count; i++ )
    {
        bot = addTestClient( "Test " + ( i + 1 ) );
        if ( !isDefined( bot ) )
        {
            codolPrintLn( "CoDOL equiptest: addTestClient failed" );
            return;
        }
        bot.codolEquipTestBot = true;
        // Honoured by the zombies damage callback only; see _zombiemode.gsc.
        bot.codolTestInvulnerable = true;
        codolPrintLn( "CoDOL equiptest: added test client " + bot getEntityNumber() );
        wait 0.5;
    }
}

// The last thrown grenade's position, kept after the engine removes it.
trackLastGrenade( grenade )
{
    level notify( "codol_equiptest_new_grenade" );
    level endon( "codol_equiptest_new_grenade" );
    while ( isDefined( grenade ) )
    {
        level.codolEquipTestGrenadePos = grenade.origin;
        wait 0.05;
    }
}

positionVerb( tokens )
{
    if ( tokens.size < 2 )
        return;
    player = playerByNumber( int( tokens[1] ) );
    if ( !isDefined( player ) || !isAlive( player ) )
    {
        codolPrintLn( "CoDOL equiptest: no live client " + tokens[1] );
        return;
    }
    if ( tokens[0] == "down" )
    {
        if ( !isDefined( level.codolEquipTestDown ) )
        {
            codolPrintLn( "CoDOL equiptest: this gametype has no down hook" );
            return;
        }
        codolPrintLn( "CoDOL equiptest: " + player.name + " goes down at " + player.origin );
        player thread [[ level.codolEquipTestDown ]]();
        return;
    }
    target = undefined;
    if ( tokens[0] == "warp" && tokens.size > 4 )
        target = ( float( tokens[2] ), float( tokens[3] ), float( tokens[4] ) );
    else if ( tokens[0] == "togrenade" )
        target = level.codolEquipTestGrenadePos;
    else if ( tokens[0] == "tozombie" && tokens.size > 2 )
    {
        zombie = nearestZombie( player.origin );
        if ( isDefined( zombie ) )
        {
            target = zombie.origin - ( float( tokens[2] ), 0, 0 );
            player setPlayerAngles( ( 20, 0, 0 ) );
            codolPrintLn( "CoDOL equiptest: nearest zombie ent " + zombie getEntityNumber() + " at " + zombie.origin +
                " health " + zombie.health );
        }
    }
    else if ( tokens[0] == "place" && tokens.size > 3 )
    {
        anchor = playerByNumber( int( tokens[2] ) );
        if ( isDefined( anchor ) )
        {
            forward = anglesToForward( ( 0, anchor.angles[1], 0 ) );
            spot = anchor.origin + ( forward[0] * float( tokens[3] ), forward[1] * float( tokens[3] ), 0 );
            trace = bulletTrace( spot + ( 0, 0, 48 ), spot - ( 0, 0, 256 ), false, player );
            target = trace["position"];
        }
    }
    if ( !isDefined( target ) )
    {
        codolPrintLn( "CoDOL equiptest: " + tokens[0] + " has no target position" );
        return;
    }
    codolPrintLn( "CoDOL equiptest: " + player.name + " moved from " + player.origin + " to " + target );
    player setOrigin( target );
}

carTrial( dist )
{
    self endon( "disconnect" );
    while ( !isAlive( self ) )
        wait 0.1;
    cars = getEntArray( "destructible_vehicle", "targetname" );
    toys = getEntArray( "destructible_toy", "targetname" );
    for ( i = 0; i < toys.size; i++ )
        cars[cars.size] = toys[i];
    car = undefined;
    for ( i = 0; i < cars.size; i++ )
    {
        if ( isDefined( cars[i].exploded ) || !isDefined( cars[i].destructible_type ) ||
            getSubStr( cars[i].destructible_type, 0, 7 ) != "vehicle" )
            continue;
        if ( !isDefined( car ) || distanceSquared( self.origin, cars[i].origin ) < distanceSquared( self.origin, car.origin ) )
            car = cars[i];
    }
    if ( !isDefined( car ) )
    {
        codolPrintLn( "CoDOL equiptest: car: no intact destructible vehicle" );
        return;
    }
    spot = car.origin + ( dist, 0, 0 );
    trace = bulletTrace( spot + ( 0, 0, 64 ), spot - ( 0, 0, 256 ), false, self );
    self setOrigin( trace["position"] );
    self freezeControls( true );
    codolPrintLn( "CoDOL equiptest: car t=" + getTime() + " ent " + car getEntityNumber() + " " + car.destructible_type +
        " at " + car.origin + "; client " + self getEntityNumber() + " at " + self.origin );
    self thread carTrialDeath();
    car thread carTrialNotes();
    for ( n = 0; n < 200 && isDefined( car ) && !isDefined( car.exploded ); n++ )
    {
        radiusDamage( car.origin + ( 0, 0, 16 ), 72, 30, 30, self );
        wait 0.3;
    }
}

// No endon "death": the engine notifies "death" on a destructible the first
// time its script_model health runs out, long before it explodes.
carTrialNotes()
{
    self thread carTrialDamage();
    self waittill( "exploded" );
    codolPrintLn( "CoDOL equiptest: car t=" + getTime() + " exploded notify" );
    self waittill( "destroyed" );
    codolPrintLn( "CoDOL equiptest: car t=" + getTime() + " destroyed notify (radius damage done)" );
}

carTrialDamage()
{
    self endon( "destroyed" );
    for ( ;; )
    {
        self waittill( "damage", amount, attacker, dir, point, type );
        codolPrintLn( "CoDOL equiptest: car t=" + getTime() + " damage " + amount + " " + type );
    }
}

pipeTrial()
{
    self endon( "disconnect" );
    while ( !isAlive( self ) )
        wait 0.1;
    pipes = getEntArray( "pipe_shootable", "targetname" );
    pipe = undefined;
    for ( i = 0; i < pipes.size; i++ )
    {
        if ( !isDefined( pipes[i].A ) || !isDefined( pipes[i].B ) )
            continue;
        if ( !isDefined( pipe ) || distanceSquared( self.origin, pipes[i].A ) < distanceSquared( self.origin, pipe.A ) )
            pipe = pipes[i];
    }
    if ( !isDefined( pipe ) )
    {
        codolPrintLn( "CoDOL equiptest: pipe: none of " + pipes.size + " shootable pipes is set up" );
        return;
    }
    // A point a few units off the pipe's axis, as a bullet hits its surface:
    // on the axis _pipes.gsc gets a zero spray direction and playFx throws.
    dir = vectorNormalize( pipe.B - pipe.A );
    off = ( 3, 3, 3 );
    off = off - dir * vectorDot( off, dir );
    point = ( pipe.A + pipe.B ) * 0.5 + off;
    trace = bulletTrace( point + ( 64, 0, 0 ), point + ( 64, 0, -512 ), false, self );
    self setOrigin( trace["position"] );
    self freezeControls( true );
    codolPrintLn( "CoDOL equiptest: pipe t=" + getTime() + " " + pipe.script_noteworthy + " at " + point + "; client " +
        self getEntityNumber() + " at " + self.origin );
    pipe notify( "damage", 10, self, ( -1, 0, 0 ), point, "MOD_PISTOL_BULLET" );
}

carTrialDeath()
{
    self endon( "disconnect" );
    self waittill( "death" );
    codolPrintLn( "CoDOL equiptest: car t=" + getTime() + " client " + self getEntityNumber() + " died" );
}

nearestZombie( origin )
{
    if ( !isDefined( level.zombies ) )
        return undefined;
    best = undefined;
    bestDsq = 0;
    for ( i = 0; i < level.zombies.size; i++ )
    {
        z = level.zombies[i];
        if ( !isDefined( z ) || !isAlive( z ) )
            continue;
        dsq = distanceSquared( origin, z.origin );
        if ( !isDefined( best ) || dsq < bestDsq )
        {
            best = z;
            bestDsq = dsq;
        }
    }
    return best;
}

// "akimbosprint" trial: run after an "akimbo" trial has armed the weapon.
// Fires the left gun (hand 1, C1 trigger 0x1) one round per press until its
// magazine is empty, waits <delay> seconds into the reload that starts, then
// holds sprint (0x2) with the stick forward for <seconds>. C1's sprint check
// (0x41d3e0 -> 0x41d070) puts every hand into sprint-in, so the left reload is
// dropped: the left magazine must stay empty through the sprint and refill
// only after it, by a fresh reload.
akimboSprintTrial( delay, seconds )
{
    self endon( "disconnect" );
    if ( !isAlive( self ) || !self isDualWielding() )
    {
        codolPrintLn( "CoDOL akimbosprint: " + self.name + " is not dual wielding" );
        return;
    }
    weapon = self getCurrentWeapon();
    self giveMaxAmmo( weapon );
    for ( shots = 0; self getCurrentWeaponClipAmmo( "left" ) > 0 && shots < 60; shots++ )
    {
        self codol_botPressButtons( 1, 0.1 );
        wait 0.25;
    }
    codolPrintLn( "CoDOL akimbosprint: left emptied after " + shots + " presses, right " +
        self getCurrentWeaponClipAmmo( "right" ) + " left " + self getCurrentWeaponClipAmmo( "left" ) );
    // Keep the bot's commands running (stance-held bit only) through the delay.
    self codol_botPressButtons( 4096, delay );
    wait delay;
    codolPrintLn( "CoDOL akimbosprint: sprint for " + seconds + " left " + self getCurrentWeaponClipAmmo( "left" ) );
    self codol_botPressButtons( 2, seconds, 1 );
    start = getTime();
    lastLeft = -1;
    lastRight = -1;
    while ( getTime() - start < seconds * 1000 + 4000 )
    {
        if ( getTime() - start >= seconds * 1000 && getTime() - start < seconds * 1000 + 100 )
            self codol_botPressButtons( 4096, 4 );
        right = self getCurrentWeaponClipAmmo( "right" );
        left = self getCurrentWeaponClipAmmo( "left" );
        if ( right != lastRight || left != lastLeft )
            codolPrintLn( "CoDOL akimbosprint: t " + ( getTime() - start ) + " right " + right + " left " + left );
        lastRight = right;
        lastLeft = left;
        wait 0.05;
    }
    codolPrintLn( "CoDOL akimbosprint: done right " + self getCurrentWeaponClipAmmo( "right" ) + " left " +
        self getCurrentWeaponClipAmmo( "left" ) );
}

// "akimbo" trial: the held weapon's per-hand magazines ("right" is hand 0, the
// retained machine; "left" is hand 1) sampled every frame while a test client
// holds <mask>. C1 fires an akimbo hand 0 on 0x100000 and hand 1 on 0x1
// (0x41b1f0), and never aims down the sights while dual wielding (0x418140),
// so a trial shows which hand each trigger fires, whether a semi-automatic hand
// pins after one round, and whether ADS was entered.
akimboTrial( weapon, mask, seconds )
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
        return;
    if ( weapon == "held" )
    {
        // Whichever owned weapon dual wields (a loadout akimbo secondary).
        foreach ( candidate in self getWeaponsListPrimaries() )
        {
            if ( self isDualWielding() )
                break;
            codolPrintLn( "CoDOL akimbo: candidate '" + candidate + "'" );
            self switchToWeapon( candidate );
            for ( tries = 0; self getCurrentWeapon() != candidate && tries < 40; tries++ )
                wait 0.1;
            wait 1.5;
        }
        weapon = self getCurrentWeapon();
        self giveMaxAmmo( weapon );
    }
    // A test client that is not a combat bot only runs commands while a
    // scripted hold is active; hold the stance-held bit (0x1000, no weapon
    // or movement effect) so the switch below is actually executed.
    self codol_botPressButtons( 4096, 4 );
    if ( self getCurrentWeapon() != weapon )
    {
        self giveWeapon( weapon );
        self giveMaxAmmo( weapon );
        // setSpawnWeapon raises it directly: a test client's own command names
        // no weapon until the server has published one for it.
        self setSpawnWeapon( weapon );
        self switchToWeapon( weapon );
        codolPrintLn( "CoDOL akimbo: gave '" + weapon + "' has " + self hasWeapon( weapon ) + " current '" +
            self getCurrentWeapon() + "'" );
        for ( tries = 0; self getCurrentWeapon() != weapon && tries < 40; tries++ )
            wait 0.1;
        wait 1.5;
    }
    codolPrintLn( "CoDOL akimbo: " + self.name + " weapon '" + self getCurrentWeapon() + "' dual " +
        self isDualWielding() + " right " + self getCurrentWeaponClipAmmo( "right" ) + " left " +
        self getCurrentWeaponClipAmmo( "left" ) + " mask " + mask + " for " + seconds );
    self codol_botPressButtons( mask, seconds );
    start = getTime();
    lastRight = -1;
    lastLeft = -1;
    lastAds = -1;
    while ( getTime() - start < seconds * 1000 + 500 )
    {
        right = self getCurrentWeaponClipAmmo( "right" );
        left = self getCurrentWeaponClipAmmo( "left" );
        ads = self playerADS();
        if ( right != lastRight || left != lastLeft || ads != lastAds )
            codolPrintLn( "CoDOL akimbo: t " + ( getTime() - start ) + " right " + right + " left " + left +
                " ads " + ads );
        lastRight = right;
        lastLeft = left;
        lastAds = ads;
        wait 0.05;
    }
    codolPrintLn( "CoDOL akimbo: done right " + self getCurrentWeaponClipAmmo( "right" ) + " left " +
        self getCurrentWeaponClipAmmo( "left" ) );
}

// "knife", "knifeat" and "knifepick" (see the header): a real thrown
// throwingknife_mp through the bot's lethal button and native aim, then its
// entity followed until it is gone.
knifeTrial( tokens )
{
    level endon( "game_ended" );
    player = playerByNumber( int( tokens[1] ) );
    if ( !isDefined( player ) || !isAlive( player ) )
    {
        codolPrintLn( "CoDOL knife test: no live client " + tokens[1] );
        return;
    }
    if ( tokens[0] == "knifepick" )
    {
        if ( !isDefined( level.codolKnifeTestKnife ) )
        {
            codolPrintLn( "CoDOL knife test: no knife to pick up" );
            return;
        }
        before = player getAmmoCount( "throwingknife_mp" );
        spot = level.codolKnifeTestKnife.origin;
        trace = bulletTrace( spot + ( 0, 0, 8 ), spot - ( 0, 0, 256 ), false, level.codolKnifeTestKnife );
        player setOrigin( trace["position"] );
        wait 0.3;
        codolPrintLn( "CoDOL knife test: " + player.name + " stands on the knife, ammo " + player getAmmoCount( "throwingknife_mp" ) +
            ", knife still there " + isDefined( level.codolKnifeTestKnife ) );
        player codol_botPressButtons( 40, 0.6 );   // 0x08 activate | 0x20 use-reload
        wait 1.0;
        codolPrintLn( "CoDOL knife test: " + player.name + " at " + player.origin + " knife ammo " + before + " -> " +
            player getAmmoCount( "throwingknife_mp" ) + ", knife still there " + isDefined( level.codolKnifeTestKnife ) );
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = player getOffhandEquipment( i );
    slots[0] = "throwingknife_mp";
    player giveWeapon( "throwingknife_mp" );
    player giveStartAmmo( "throwingknife_mp" );
    player setOffhandEquipment( slots[0], slots[1], slots[2] );
    victim = undefined;
    if ( tokens[0] == "knife" )
    {
        victim = playerByNumber( int( tokens[2] ) );
        if ( !isDefined( victim ) || !isAlive( victim ) )
        {
            codolPrintLn( "CoDOL knife test: no live victim " + tokens[2] );
            return;
        }
        forward = anglesToForward( ( 0, player.angles[1], 0 ) );
        spot = player.origin + ( forward[0] * 300, forward[1] * 300, 0 );
        trace = bulletTrace( spot + ( 0, 0, 48 ), spot - ( 0, 0, 256 ), false, victim );
        victim setOrigin( trace["position"] );
        // A still target: the bot AI would otherwise walk out of the throw.
        victim freezeControls( true );
        victim thread knifeVictimThaw();
        // "knife <thrower> <victim> tough": the victim survives the knife and
        // dies 2 s later, carrying it.
        if ( tokens.size > 3 && tokens[3] == "tough" )
        {
            victim.maxhealth = 400;
            victim.health = 400;
            victim thread knifeVictimLaterDeath();
        }
        wait 0.3;
        point = victim.origin + ( 0, 0, 44 );
    }
    else if ( tokens[2] == "wall" || tokens[2] == "ground" )
    {
        // "knifeat <thrower> wall|ground 0 0": the first wall straight ahead
        // at chest height, or the floor 200 units ahead.
        forward = anglesToForward( ( 0, player.angles[1], 0 ) );
        eye = player.origin + ( 0, 0, 48 );
        point = player.origin + ( forward[0] * 200, forward[1] * 200, 0 );
        if ( tokens[2] == "wall" )
        {
            // The first wall 250..700 units away, searched round the thrower.
            for ( yaw = 0; yaw < 360; yaw += 30 )
            {
                dir = anglesToForward( ( 0, player.angles[1] + yaw, 0 ) );
                hit = bulletTrace( eye, eye + ( dir[0] * 700, dir[1] * 700, 0 ), false, player );
                if ( hit["fraction"] < 1 && distance( eye, hit["position"] ) > 250 )
                {
                    point = hit["position"];
                    break;
                }
            }
        }
    }
    else
        point = ( float( tokens[2] ), float( tokens[3] ), float( tokens[4] ) );
    level.codolKnifeTestKnife = undefined;
    player thread knifeCatch();
    codolPrintLn( "CoDOL knife test: " + player.name + " at " + player.origin + " throws at " + point + " (ammo " +
        player getAmmoCount( "throwingknife_mp" ) + ")" );
    thrown = false;
    for ( attempt = 0; attempt < 6 && !thrown; attempt++ )
    {
        thrown = player codol_botThrowOffhand( 0, point );
        if ( !thrown )
            wait 0.5;
    }
    if ( !thrown )
    {
        codolPrintLn( "CoDOL knife test: " + player.name + " refused the throw" );
        return;
    }
    for ( t = 0; t < 40 && !isDefined( level.codolKnifeTestKnife ); t++ )
        wait 0.05;
    knife = level.codolKnifeTestKnife;
    if ( !isDefined( knife ) )
    {
        codolPrintLn( "CoDOL knife test: no knife left the hand" );
        return;
    }
    for ( t = 0; t < 24 && isDefined( knife ); t++ )
    {
        line = "CoDOL knife test: t=" + ( t * 0.5 ) + " knife " + knife getEntityNumber() + " at " + knife.origin;
        if ( isDefined( victim ) )
            line += " victim alive " + isAlive( victim ) + " at " + victim.origin;
        codolPrintLn( line );
        wait 0.5;
    }
    if ( !isDefined( knife ) )
        codolPrintLn( "CoDOL knife test: knife entity is gone" );
}

knifeCatch()
{
    self endon( "disconnect" );
    self notify( "codol_knife_catch" );
    self endon( "codol_knife_catch" );
    for ( ;; )
    {
        self waittill( "grenade_fire", grenade, weapon );
        if ( weapon == "throwingknife_mp" && isDefined( grenade ) )
        {
            level.codolKnifeTestKnife = grenade;
            return;
        }
    }
}

knifeVictimThaw()
{
    self endon( "disconnect" );
    self thread knifeVictimThawOnDeath();
    wait 6;
    self freezeControls( false );
}

knifeVictimLaterDeath()
{
    self endon( "disconnect" );
    self endon( "death" );
    wait 2.5;
    codolPrintLn( "CoDOL knife test: " + self.name + " health " + self.health + ", now killed" );
    self.maxhealth = 100;
    self suicide();
}

knifeVictimThawOnDeath()
{
    self endon( "disconnect" );
    self waittill( "death" );
    self freezeControls( false );
}
