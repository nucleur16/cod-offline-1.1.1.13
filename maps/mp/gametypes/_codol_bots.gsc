// CoDOL combat-training integration. Native AI follows OpenBLOPS T5
// server_mp/sv_bot_mp.cpp semantics; player lifecycle uses the mounted IW4
// gametype callbacks. No scripted damage, origin assignment, or weapon proxy.
init()
{
    if ( isDefined( level.codolCombatBotsStarted ) )
        return;
    level.codolCombatBotsStarted = true;
    if ( getDvar( "scr_codol_bots" ) == "" )
        setDvar( "scr_codol_bots", 0 );
    // Independent of bots: it also drives a human client by number.
    level thread watchKillstreakTestDvar();
    maps\mp\gametypes\_codol_equiptest::init();
    maps\mp\gametypes\_codol_perktest::init();
    maps\mp\gametypes\_codol_scavtest::init();
    maps\mp\gametypes\_codol_breathtest::init();
    maps\mp\gametypes\_codol_ridetest::init();
    maps\mp\gametypes\_codol_chtest::init();
    graph = "waypoints/" + getDvar( "mapname" ) + "_wp.csv";
    if ( loadBotWaypoints( graph ) <= 0 )
        return;
    // The serial names bots and picks their class. It lives in game[] so a
    // round restart does not name a new bot after one that is still on.
    if ( !isDefined( game["codolBotSerial"] ) )
        game["codolBotSerial"] = 0;
    level thread maintainBots();
}

// Round-based modes (sd, sab, ...) end each round with map_restart: pers[]
// survives, entity fields do not. A returning training bot would otherwise
// connect as a human (no codolCombatBot): giveLoadout reads its class0 as a
// profile class, the profile getters raise (a bot has no profile upload) and
// the undefined killstreak kills the match; watchSpawns would not run either,
// and maintainBots would add a second set of bots. Called from
// Callback_PlayerConnect before anything reads those fields.
restoreCombatBot()
{
    if ( !isDefined( self.pers["codolCombatBot"] ) || !self.pers["codolCombatBot"] )
        return;
    self.codolCombatBot = true;
    self.codolBotClass = self.pers["codolBotClass"];
    self thread watchSpawns();
}

isCombatBot( player )
{
    if ( isDefined( player.codolCombatBot ) && player.codolCombatBot )
        return true;
    return isDefined( player.pers ) && isDefined( player.pers["codolCombatBot"] ) && player.pers["codolCombatBot"];
}

// Test hook for rewards bots never earn on their own:
//   set scr_codol_usekillstreak "<clientnum> <streakRef> [place|throw|locate]"
// runs level.killstreakFuncs[streakRef] on that player, exactly as a use
// would. "place" also sends the carry-placement notify a second later, for
// carried rewards (sentry, sam_turret) that wait on +attack. "throw" throws
// an airdrop marker, "locate" answers a map location selection. Other verbs:
// "capture <clientnum>" picks up the nearest landed care package, "crates"
// prints the care-package weight table, "ents" the live reward entities,
// "switch <clientnum> [weapon]" reports a scripted weapon switch. Empty = off.
watchKillstreakTestDvar()
{
	// Test hooks (they can set health, freeze players and grant rewards) only run
	// on a developer server; the live server runs developer 0.
	if ( getDvarInt( "developer" ) <= 0 )
		return;
    level endon( "game_ended" );
    setDvar( "scr_codol_usekillstreak", "" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_usekillstreak" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_usekillstreak", "" );
        if ( request == "list" )
        {
            reportKillstreakCoverage();
            continue;
        }
        if ( request == "rows" )
        {
            reportKillstreakRows();
            continue;
        }
        hurtTokens = strTok( request, " " );
        if ( hurtTokens[0] == "hurt" && hurtTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( hurtTokens[1] ) && isAlive( player ) )
                {
                    {}
                    player thread reportNextSpawn();
                    player doDamage( 1000, player.origin + ( 0, 0, 40 ), level.players[0] );
                    player codol_deathGates();
                }
            continue;
        }
        // "xp <clientnum> <amount>" exercises the ordinary human rank path
        // without relying on bot kills or the retired _dev.gsc switches.
        if ( hurtTokens[0] == "xp" && hurtTokens.size > 2 )
        {
            xpAmount = int( hurtTokens[2] );
            if ( xpAmount > 0 && xpAmount <= 50000 )
                foreach ( player in level.players )
                    if ( player getEntityNumber() == int( hurtTokens[1] ) &&
                         ( !isDefined( player.isbot ) || !player.isbot ) )
                    {
                        oldXP = player maps\mp\gametypes\_rank::getRankXP();
                        player maps\mp\gametypes\_rank::giveRankXP( "challenge", xpAmount );
                        {}
                    }
            continue;
        }
        if ( request == "gates" )
        {
            foreach ( player in level.players )
                player codol_deathGates();
            continue;
        }
        // "perk <client> <perk>": grant a perk row for this life and report
        // the selected modifiers it drives. "assist <client> <victim>...": run
        // the real assist award per victim. "perkdmg <client> <mod> <damage>": report
        // what cac_modified_damage leaves of that damage.
        perkTokens = strTok( request, " " );
        if ( ( perkTokens[0] == "perk" || perkTokens[0] == "assist" || perkTokens[0] == "perkdmg" ) && perkTokens.size > 2 )
        {
            perkPlayer = undefined;
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( perkTokens[1] ) )
                    perkPlayer = player;
            if ( !isDefined( perkPlayer ) || !isAlive( perkPlayer ) )
            {
                {}
                continue;
            }
            if ( perkTokens[0] == "perk" )
                perkPlayer thread testPerk( perkTokens[2] );
            else if ( perkTokens[0] == "perkdmg" && perkTokens.size > 3 )
            {
                left = maps\mp\perks\_perks::cac_modified_damage( perkPlayer, level.players[0], int( perkTokens[3] ), perkTokens[2], "frag_grenade_mp", perkPlayer.origin, ( 0, 0, 1 ), "none" );
                {}
            }
            else if ( perkTokens[0] == "assist" )
            {
                for ( victimToken = 2; victimToken < perkTokens.size; victimToken++ )
                    foreach ( player in level.players )
                        if ( player getEntityNumber() == int( perkTokens[victimToken] ) )
                            perkPlayer thread maps\mp\gametypes\_gamescore::processAssist( player );
            }
            continue;
        }
        tiTokens = strTok( request, " " );
        // Keep spatial placement tests repeatable after a bot respawns at a
        // wall-facing point. This developer-only hook moves the test subject;
        // the reward still runs its normal placement and ownership checks.
        if ( tiTokens[0] == "relocate" && tiTokens.size > 5 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                {
                    player setOrigin( ( float( tiTokens[2] ), float( tiTokens[3] ), float( tiTokens[4] ) ) );
                    player setPlayerAngles( ( 0, float( tiTokens[5] ), 0 ) );
                    {}
                }
            continue;
        }
        // Exercise the real award path without relying on bot combat timing.
        // "streak <client> <count>" logs thresholds and owned reward IDs.
        if ( tiTokens[0] == "streak" && tiTokens.size > 2 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) && isAlive( player ) )
                {
                    count = int( tiTokens[2] );
                    player.pers["cur_kill_streak"] = count;
                    player maps\mp\killstreaks\_killstreaks::checkKillstreakReward( count );
                    thresholds = "";
                    foreach ( cost, name in player.killStreaks )
                        if ( cost <= count )
                            thresholds = thresholds + cost + ":" + name + " ";
                    owned = "";
                    foreach ( reward in player.pers["killstreaks"] )
                        owned = owned + reward.streakName + "#" + reward.kID + " ";
                    selected = "";
                    for ( slot = 2; slot <= 4; slot++ )
                    {
                        reward = player maps\mp\killstreaks\_killstreaks::codolRewardForSlot( slot );
                        if ( isDefined( reward ) )
                            selected = selected + slot + ":" + reward.streakName + "#" + reward.kID + " ";
                    }
                    {}
                }
            continue;
        }
        // "switch <client> [weapon]": report the inventory, then switch to
        // the named weapon (default: the first other weapon owned).
        if ( tiTokens[0] == "switch" && tiTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                {
                    list = player getWeaponsListAll();
                    names = "";
                    foreach ( w in list )
                        names = names + w + ", ";
                    want = undefined;
                    if ( tiTokens.size > 2 )
                        want = tiTokens[2];
                    else
                        foreach ( w in list )
                            if ( w != player getCurrentWeapon() && !isDefined( want ) )
                                want = w;
                    {}
                    player thread logWeaponChanges( 5 );
                    ok = player switchToWeapon( want );
                    {}
                }
            continue;
        }
        // "crates": the care-package weight table and ten draws from it.
        if ( tiTokens[0] == "crates" )
        {
            thread testCrateTable();
            continue;
        }
        // "ents": what killstreak entities exist right now.
        if ( tiTokens[0] == "ents" )
        {
            thread reportStreakEntities();
            continue;
        }
        // "capture <client>": walk that player to the nearest landed care
        // package and hold use on it, as a player picking it up would.
        if ( tiTokens[0] == "capture" && tiTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                    player thread testCaptureCrate();
            continue;
        }
        // "downed <client>": a lethal hit from an enemy, which a player with
        // a last-stand perk survives into last stand. "revive <client>": the
        // teammate revive's own completion (_damage.gsc reviveTriggerThink).
        if ( ( tiTokens[0] == "downed" || tiTokens[0] == "revive" ) && tiTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) && isAlive( player ) )
                {
                    if ( tiTokens[0] == "downed" )
                    {
                        enemy = undefined;
                        foreach ( other in level.players )
                            if ( other != player && isAlive( other ) && ( !level.teamBased || other.team != player.team ) )
                                enemy = other;
                        {}
                        player.maxhealth = 100;
                        player.health = 100;
                        player doDamage( 1000, player.origin + ( 0, 0, 40 ), enemy, enemy, 0, "MOD_RIFLE_BULLET" );
                    }
                    else if ( isDefined( player.lastStand ) )
                    {
                        {}
                        player.lastStand = undefined;
                        player maps\mp\_utility::clearLowerMessage( "last_stand" );
                        player maps\mp\gametypes\_playerlogic::lastStandRespawnPlayer();
                    }
                    player reportRewardState();
                }
            continue;
        }
        // Sentry checks. "sentryplace <client>": carry and place a sentry.
        // "sentryuse <client> [yawOffset]": stand that bot 48 units from the
        // nearest placed sentry or SAM (yawOffset degrees round from its
        // facing, default 0 = in front) and hold use, as a player picking it
        // up does. "sentryvictim <client> <distance> <yawOffset> <seconds>":
        // stand a player frozen in front of it and report whether it killed
        // him. "sentrystate": each player's team and each turret's mode,
        // owner, carrier, target and gun pitch/yaw (tag_flash).
        // (Own function: this one is at the VM's local variable limit.)
        if ( tiTokens[0] == "sentryuse" || tiTokens[0] == "sentrystate" || tiTokens[0] == "sentryplace" || tiTokens[0] == "sentryvictim" )
        {
            testSentryVerb( tiTokens );
            continue;
        }
        if ( tiTokens[0] == "suicide" && tiTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                    player thread testSuicide();
            continue;
        }
        // "state <client>": the reward dispatcher's per-player state, for
        // death-cleanup checks. "confirm <client>": one map-selector
        // confirmation 600 units ahead, as the C1 selector sends it.
        // "give <client> <streak>": own a reward without using it; switch to
        // its weapon ("switch") to use it through the real dispatcher.
        // "activate <client>": raise the newest reward's weapon and deliver
        // its weapon_change to the player's own use waiter, as a C1 client's
        // weapon selection does (bots never name the weapon in a usercmd).
        if ( ( tiTokens[0] == "state" || tiTokens[0] == "confirm" || tiTokens[0] == "give" || tiTokens[0] == "activate" || tiTokens[0] == "kill" || tiTokens[0] == "freeze" ) && tiTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                {
                    if ( tiTokens[0] == "give" && tiTokens.size > 2 )
                        player maps\mp\killstreaks\_killstreaks::giveKillstreak( tiTokens[2] );
                    if ( tiTokens[0] == "state" || tiTokens[0] == "give" )
                        player reportRewardState();
                    if ( tiTokens[0] == "activate" )
                        player thread activateTopReward( tiTokens[2], tiTokens[3] );
                    if ( tiTokens[0] == "freeze" )
                        player freezeControls( int( tiTokens[2] ) != 0 );
                    if ( tiTokens[0] == "kill" && isAlive( player ) )
                    {
                        // "kill <client>": end an "activate"d subject's
                        // protection and kill it through the damage callback.
                        {}
                        player.maxhealth = 100;
                        player.health = 100;
                        player doDamage( 1000, player.origin + ( 0, 0, 40 ), level.players[0] );
                    }
                    if ( tiTokens[0] == "confirm" )
                    {
                        location = player.origin + anglesToForward( ( 0, player.angles[1], 0 ) ) * 600;
                        {}
                        player notify( "confirm_location", location, player.angles[1] );
                    }
                }
            continue;
        }
        // "rcsafe <0|1>": 1 stops an RC-XD driver's trigger detonating the
        // car (T5 level.disableRCBombTrigger), so a bot's ride lasts until
        // its timer; bots hold attack.
        // "ksslot <client> <verb>": killstreak action-slot switching checks
        // (own function: this one is at the VM's local variable limit).
        if ( tiTokens[0] == "ksslot" && tiTokens.size > 2 )
        {
            testRewardSlotVerb( tiTokens );
            continue;
        }
        if ( tiTokens[0] == "rcsafe" && tiTokens.size > 1 )
        {
            level.disableRCBombTrigger = int( tiTokens[1] ) != 0;
            {}
            continue;
        }
        if ( tiTokens[0] == "ti" && tiTokens.size > 1 )
        {
            tiPlayer = undefined;
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                    tiPlayer = player;
            if ( isDefined( tiPlayer ) )
                tiPlayer thread testTacticalInsertion();
            continue;
        }
        tokens = strTok( request, " " );
        if ( tokens.size < 2 )
            continue;
        clientNum = int( tokens[0] );
        streakRef = tokens[1];
        if ( !isDefined( level.killstreakFuncs[streakRef] ) )
        {
            {}
            continue;
        }
        target = undefined;
        foreach ( player in level.players )
        {
            if ( player getEntityNumber() == clientNum )
                target = player;
        }
        // A bot is often mid-respawn; give it a moment rather than dropping
        // the request.
        for ( tries = 0; isDefined( target ) && !isAlive( target ) && tries < 30; tries++ )
            wait 0.5;
        if ( !isDefined( target ) || !isAlive( target ) )
        {
            {}
            continue;
        }
        // Rides and carried rewards refuse an airborne user; bots jump.
        target freezeControls( true );
        for ( tries = 0; tries < 60 && !target isOnGround(); tries++ )
            wait 0.1;
        target freezeControls( false );
        target.maxhealth = 100000;
        target.health = 100000;
        {}
        // Own it first, as a real use does: several rewards (the helicopter
        // queue, for one) reshuffle the owner's pers["killstreaks"] list and
        // raise a runtime error for a streak that is not in it.
        // Then use it through the real button path, which resolves the owned
        // reward record and passes its lifeId and kID (airdrops validate the
        // kID) instead of calling the handler bare.
        target maps\mp\killstreaks\_killstreaks::giveKillstreak( streakRef );
        // "throw": use a thrown-marker reward (the airdrops) the way a player
        // does - switch to the activation weapon, which the real use waiter
        // turns into the use, then pull the trigger until it is thrown.
        if ( tokens.size > 2 && tokens[2] == "throw" )
        {
            target thread throwKillstreakMarker( streakRef );
            continue;
        }
        target thread maps\mp\killstreaks\_killstreaks::killstreakUsePressed();
        // "locate": answer a location selection (airstrikes, mortar) the way
        // the C1 map selector does, 600 units ahead of the player.
        if ( tokens.size > 2 && tokens[2] == "locate" )
            target thread confirmTestLocation();
        if ( tokens.size > 2 && tokens[2] == "place" )
            target thread placeCarriedKillstreak();
    }
}

activateTopReward( streakName, hold )
{
    self endon( "disconnect" );
    reward = self.pers["killstreaks"][0];
    foreach ( candidate in self.pers["killstreaks"] )
        if ( isDefined( streakName ) && candidate.streakName == streakName )
            reward = candidate;
    if ( !isDefined( reward ) )
        return;
    weapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( reward.streakName );
    // Bots fight; keep the subject alive until "kill".
    self.maxhealth = 100000;
    self.health = 100000;
    // Rides and placements refuse an airborne user; bots jump.
    self freezeControls( true );
    for ( tries = 0; tries < 60 && !self isOnGround(); tries++ )
        wait 0.1;
    // setSpawnWeapon raises the native weapon_change the use waiter consumes.
    self setSpawnWeapon( weapon );
    wait 0.1;
    // "hold": stay frozen (no fire, no movement) until "freeze <client> 0";
    // a bot otherwise fires the tablet it was just handed.
    if ( !isDefined( hold ) )
        self freezeControls( false );
    {}
}

reportRewardState()
{
    owned = "";
    foreach ( reward in self.pers["killstreaks"] )
        owned = owned + reward.streakName + "#" + reward.kID + " ";
    claim = "none";
    if ( isDefined( self.codolRCClaim ) )
        claim = "released " + self.codolRCClaim.released;
    rule = "";
    if ( isDefined( level.codolRCRule ) )
        rule = " rcRule " + level.codolRCRule.cur + "/" + level.codolRCRule.max;
    {}
    // Offhand hold counter (_disableOffhandWeapons) and each offhand's ammo:
    // every remote must return the counter to 0 when it ends.
    hold = "undefined";
    if ( isDefined( self.disabledOffhandWeapons ) )
        hold = "" + self.disabledOffhandWeapons;
    offhands = "";
    foreach ( offhand in self getWeaponsListOffhands() )
        offhands = offhands + offhand + ":" + self getWeaponAmmoClip( offhand ) + " ";
    switchHold = "undefined";
    if ( isDefined( self.disabledWeaponSwitch ) )
        switchHold = "" + self.disabledWeaponSwitch;
    {}
    // weapFlags 0x800 weapon switch held, 0x1000 offhands held (native).
    self codol_deathGates();
}

confirmTestLocation()
{
    self endon( "disconnect" );
    self.maxhealth = 100000;
    self.health = 100000;
    for ( i = 0; i < 10 && !isDefined( self.selectingLocation ); i++ )
        wait 0.3;
    wait 1.0;
    location = self.origin + anglesToForward( ( 0, self.angles[1], 0 ) ) * 600;
    {}
    self notify( "confirm_location", location, self.angles[1] );
}

reportStreakEntities()
{
    classes = [];
    classes[classes.size] = "script_vehicle";
    classes[classes.size] = "misc_turret";
    classes[classes.size] = "script_model";
    classes[classes.size] = "grenade";
    classes[classes.size] = "rocket";
    line = "";
    foreach ( c in classes )
        line = line + c + "=" + getEntArray( c, "classname" ).size + " ";
    line = line + "helis=" + isDefined( level.chopper ) + " ac130player=" + isDefined( level.ac130player ) + " empTeam allies/axis " + level.teamEMPed["allies"] + "/" + level.teamEMPed["axis"] + " nukeIncoming " + isDefined( level.nukeIncoming ) + " sr71 allies/axis " + isDefined( level.sr71TeamUntil["allies"] ) + "/" + isDefined( level.sr71TeamUntil["axis"] ) + " uav allies/axis " + level.activeUAVs["allies"] + "/" + level.activeUAVs["axis"] + " counter " + level.activeCounterUAVs["allies"] + "/" + level.activeCounterUAVs["axis"];
    {}
}

testCrateTable()
{
    foreach ( dropType, crateTypes in level.crateTypes )
    {
        line = "";
        foreach ( crateType, crateWeight in crateTypes )
            line = line + crateType + "=" + crateWeight + " ";
        {}
    }
    draws = "";
    for ( i = 0; i < 10; i++ )
        draws = draws + randomInt( 125 ) + ":" + maps\mp\killstreaks\_airdrop::getRandomCrateType( "airdrop" ) + " ";
    {}
}

testCaptureCrate()
{
    self endon( "disconnect" );
    self endon( "death" );
    best = undefined;
    foreach ( crate in getEntArray( "care_package", "targetname" ) )
    {
        if ( !isDefined( crate.crateType ) )
            continue;
        if ( !isDefined( best ) || distance( crate.origin, self.origin ) < distance( best.origin, self.origin ) )
            best = crate;
    }
    if ( !isDefined( best ) )
    {
        {}
        return;
    }
    self.maxhealth = 100000;
    self.health = 100000;
    crateType = best.crateType;
    self setOrigin( best.origin + ( 48, 0, 0 ) );
    self setPlayerAngles( vectorToAngles( best.origin - ( self.origin + ( 0, 0, 60 ) ) ) );
    {}
    for ( i = 0; i < 12 && isDefined( best ); i++ )
    {
        self codol_botPressButtons( 40, 1.0 );
        wait 1.0;
    }
    {}
}

throwKillstreakMarker( streakRef )
{
    self endon( "disconnect" );
    self endon( "death" );
    weapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( streakRef );
    // Keep the test subject alive through the throw; bots fight each other.
    self.maxhealth = 100000;
    self.health = 100000;
    wait 0.5;
    {}
    self thread logWeaponChanges( 8 );
    self switchToWeapon( weapon );
    for ( i = 0; i < 20 && self getCurrentWeapon() != weapon; i++ )
        wait 0.05;
    // A test client's scripted switch does not always complete (no client
    // names the weapon in its usercmd); raise it directly and run the same
    // activation the use waiter runs on a weapon_change to it.
    if ( self getCurrentWeapon() != weapon )
    {
        self setSpawnWeapon( weapon );
        wait 0.1;
        if ( isDefined( self.pers["killstreaks"][0] ) )
            self thread maps\mp\killstreaks\_killstreaks::codolActivateReward( self.pers["killstreaks"][0].kID );
        wait 0.3;
    }
    {}
    self thread logMarkerThrow( weapon );
    for ( attempt = 1; attempt <= 6; attempt++ )
    {
        if ( isDefined( self.codolMarkerThrown ) )
            return;
        self codol_botPressButtons( 1, 0.4 );
        wait 1.5;
    }
    if ( !isDefined( self.codolMarkerThrown ) )
        {}
}

logWeaponChanges( seconds )
{
    self endon( "disconnect" );
    self endon( "death" );
    self notify( "logWeaponChanges" );
    self endon( "logWeaponChanges" );
    endTime = getTime() + seconds * 1000;
    while ( getTime() < endTime )
    {
        self waittill( "weapon_change", newWeapon );
        {}
    }
}

trackMarker( weapname )
{
    self endon( "death" );
    self endon( "explode" );
    start = getTime();
    for ( i = 0; i < 40; i++ )
    {
        {}
        self common_scripts\utility::waittill_any_timeout( 2.5, "stationary", "missile_stuck" );
    }
}

logMarkerThrow( weapon )
{
    self endon( "disconnect" );
    self.codolMarkerThrown = undefined;
    for ( ;; )
    {
        self waittill( "grenade_fire", grenade, weapname );
        if ( weapname != weapon )
            continue;
        self.codolMarkerThrown = true;
        {}
        grenade thread trackMarker( weapname );
        grenade waittill( "explode", position );
        {}
        return;
    }
}

// set scr_codol_usekillstreak list: every catalogue row, whether a use
// handler is registered for it, and its activation weapon.
reportKillstreakCoverage()
{
    table = "mp/coKillstreakStatsTable.csv";
    missing = 0;
    for ( row = 0; true; row++ )
    {
        ref = tableLookupByRow( table, row, 1 );
        if ( !isDefined( ref ) || ref == "" )
            break;
        if ( ref == "none" )
            continue;
        handled = isDefined( level.killstreakFuncs[ref] );
        if ( !handled )
            missing++;
        {}
    }
    {}
}

// set scr_codol_usekillstreak rows: every column of every catalogue row.
reportKillstreakRows()
{
    table = "mp/coKillstreakStatsTable.csv";
    for ( row = 0; true; row++ )
    {
        ref = tableLookupByRow( table, row, 1 );
        if ( !isDefined( ref ) || ref == "" )
            break;
        line = "";
        for ( col = 0; col < 24; col++ )
            line = line + tableLookupByRow( table, row, col ) + "|";
        {}
    }
}

// set scr_codol_usekillstreak "ti <clientnum>": give the tactical insertion
// perk, throw its flare_mp through the real tactical button, then kill the
// player and report whether the respawn used the beacon.
testTacticalInsertion()
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
        return;
    // As _class::codol_giveEquipment does for a class carrying it: the perk,
    // then flare_mp with its start ammo. flare_mp is offhand class 5, so it is
    // equipment (slot 2, button 0x10000), like the betty.
    // NOTE: C1 ships flare_mp with shared ammo cap WEAPON_FRAGGRENADE, so a
    // player already holding a frag gets 0 flares from giveStartAmmo (the cap
    // is full) and the flare leaves the inventory; codol_dumpOwnedAmmo() shows
    // the pools. Open question for C1's own cap routine, 2026-09-24.
    self maps\mp\perks\_perks::givePerk( "specialty_tacticalinsertion" );
    self giveWeapon( "flare_mp" );
    self giveStartAmmo( "flare_mp" );
    if ( !self hasWeapon( "flare_mp" ) )
    {
        {}
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = self getOffhandEquipment( i );
    slots[2] = "flare_mp";
    self setOffhandEquipment( slots[0], slots[1], slots[2] );
    {}
    wait 1.0;
    self maps\mp\gametypes\_codol_equiptest::pressUntilThrown( 65536, "flare_mp" );
    wait 1.0;
    if ( !isDefined( self.setSpawnPoint ) )
    {
        {}
        return;
    }
    beacon = self.setSpawnPoint.origin;
    {}
    self suicide();
    self waittill( "spawned_player" );
    {}
}

testPerk( perk )
{
    self endon( "disconnect" );
    maps\mp\perks\_perks::givePerk( perk );
    // Perk set callbacks run as threads.
    wait 0.1;
    names = strTok( "modFinalStandTime modPistolDeath modKillstreakAssists modDeathstreakShift modObjectiveUseTimeScale", " " );
    report = "CoDOL perk test: " + self.name + " +" + perk;
    foreach ( name in names )
    {
        value = self getSelectedPerkModifierValue( name );
        if ( isDefined( value ) )
            report += " " + name + "=" + value;
    }
    report += " rowDeathstreakShift=" + maps\mp\perks\_perks::perkTableModifierDelta( perk, "modDeathstreakShift" );
    report += " modBlastShield=" + self hasAbilityModifier( "modBlastShield" );
    report += " modNoRadar=" + self hasAbilityModifier( "modNoRadar" );
    report += " _specialty_blastshield=" + maps\mp\_utility::_hasPerk( "_specialty_blastshield" );
    unlocked = self codol_tryIsItemUnlocked( perk );
    if ( isDefined( unlocked ) )
        report += " unlocked=" + unlocked;
    else
        report += " unlocked=unresolved";
    {}
}

testSuicide()
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
    {
        {}
        return;
    }
    {}
    self codol_deathGates();
    self thread reportNextSpawn();
    self suicide();
    self codol_deathGates();
}

reportNextSpawn()
{
    self endon( "disconnect" );
    self thread reportAliveAfterSuicide();
    self waittill( "spawned_player" );
    {}
}

reportAliveAfterSuicide()
{
    self endon( "disconnect" );
    for ( i = 0; i < 8; i++ )
    {
        wait 1.0;
        {}
    }
}

testSentryVerb( tokens )
{
    if ( tokens[0] == "sentrystate" )
    {
        reportSentries();
        return;
    }
    if ( tokens.size < 2 )
        return;
    foreach ( player in level.players )
        if ( player getEntityNumber() == int( tokens[1] ) && isAlive( player ) )
        {
            if ( tokens[0] == "sentryuse" )
                player thread testSentryUse( tokens );
            else if ( tokens[0] == "sentryplace" )
                player thread testSentryPlace();
            else if ( tokens[0] == "sentryvictim" )
                player thread testSentryVictim( tokens );
        }
}

// "sentryplace <client>": own and carry a sentry the real way, then turn
// the frozen carrier until the spot in front of him is placeable and place
// it there (bots rarely face open ground on their own).
testSentryPlace()
{
    self endon( "disconnect" );
    self endon( "death" );
    self.maxhealth = 100000;
    self.health = 100000;
    self freezeControls( true );
    for ( tries = 0; tries < 60 && !self isOnGround(); tries++ )
        wait 0.1;
    self maps\mp\killstreaks\_killstreaks::giveKillstreak( "sentry" );
    self thread maps\mp\killstreaks\_killstreaks::killstreakUsePressed();
    wait 1.0;
    carried = undefined;
    foreach ( turret in getEntArray( "misc_turret", "classname" ) )
        if ( isDefined( turret.carriedBy ) && turret.carriedBy == self )
            carried = turret;
    if ( !isDefined( carried ) )
    {
        {}
        self freezeControls( false );
        return;
    }
    for ( yaw = 0; yaw < 360 && isDefined( carried.carriedBy ); yaw += 30 )
    {
        self setPlayerAngles( ( 0, yaw, 0 ) );
        wait 0.3;
        if ( carried.canBePlaced )
            self notify( "place_sentry" );
        wait 0.1;
    }
    // Nowhere placeable (a bot's spot is often under cover): stand it at
    // the carrier's feet, so the placed-sentry paths can still be tested.
    if ( isDefined( carried.carriedBy ) )
    {
        carried.origin = self.origin;
        carried.angles = ( 0, self.angles[1], 0 );
        carried.canBePlaced = true;
        self notify( "place_sentry" );
        wait 0.1;
        self setOrigin( self.origin + anglesToForward( ( 0, self.angles[1], 0 ) ) * -48 );
    }
    self freezeControls( false );
    {}
}

// "sentryvictim <client> <distance> <yawOffset> <seconds>": stand that
// player frozen in front of the nearest placed sentry and report whether
// he survived.
testSentryVictim( tokens )
{
    self endon( "disconnect" );
    self endon( "death" );
    best = undefined;
    foreach ( turret in getEntArray( "misc_turret", "classname" ) )
        if ( isDefined( turret.sentryType ) && !isDefined( turret.carriedBy ) )
            best = turret;
    if ( !isDefined( best ) || tokens.size < 5 )
        return;
    self setOrigin( best.origin + anglesToForward( ( 0, best.angles[1] + float( tokens[3] ), 0 ) ) * float( tokens[2] ) + ( 0, 0, 8 ) );
    self freezeControls( true );
    self.maxhealth = 100;
    self.health = 100;
    {}
    self thread testSentryVictimDeath( best );
    wait float( tokens[4] );
    if ( isAlive( self ) )
    {
        self freezeControls( false );
        {}
    }
}

testSentryVictimDeath( turret )
{
    self endon( "disconnect" );
    self waittill( "death", attacker, meansOfDeath, weapon );
    attackerName = "none";
    if ( isDefined( attacker ) && isPlayer( attacker ) )
        attackerName = attacker.name;
    else if ( isDefined( attacker ) )
        attackerName = attacker.classname;
    if ( !isDefined( weapon ) )
        weapon = "undefined";
    if ( !isDefined( meansOfDeath ) )
        meansOfDeath = "undefined";
    {}
}

testSentryUse( tokens )
{
    self endon( "disconnect" );
    self endon( "death" );
    yawOffset = 0;
    if ( tokens.size > 2 )
        yawOffset = float( tokens[2] );
    best = undefined;
    foreach ( turret in getEntArray( "misc_turret", "classname" ) )
    {
        if ( !isDefined( turret.sentryType ) || isDefined( turret.carriedBy ) )
            continue;
        if ( !isDefined( best ) || distance( turret.origin, self.origin ) < distance( best.origin, self.origin ) )
            best = turret;
    }
    if ( !isDefined( best ) )
    {
        {}
        return;
    }
    self.maxhealth = 100000;
    self.health = 100000;
    // Land first: a frozen player does not re-find the ground after a
    // setOrigin, and the use gates need a ground entity.
    self freezeControls( false );
    home = best.origin;
    spot = best.origin + anglesToForward( ( 0, best.angles[1] + yawOffset, 0 ) ) * 48;
    self setOrigin( spot + ( 0, 0, 8 ) );
    for ( tries = 0; tries < 30 && !self isOnGround(); tries++ )
        wait 0.05;
    self setPlayerAngles( vectorToAngles( ( best.origin + ( 0, 0, 30 ) ) - self getEye() ) );
    wait 0.2;
    ownerName = "none";
    if ( isDefined( best.owner ) )
        ownerName = best.owner.name;
    {}
    for ( i = 0; i < 4 && isDefined( best ) && !isDefined( best.carriedBy ); i++ )
    {
        // Not frozen: a frozen player gets no use hints at all (the
        // retained Player_UpdateCursorHints returns early), so hold use
        // through the bot controller, which also stops its movement.
        self pressUseButton( 1.0 );
        wait 1.5;
    }
    carrier = "nobody";
    if ( isDefined( best ) && isDefined( best.carriedBy ) )
        carrier = best.carriedBy.name;
    {}
    self freezeControls( false );
    if ( isDefined( best ) && isDefined( best.carriedBy ) && best.carriedBy == self )
    {
        wait 1.0;
        // Put it down where it was picked up (a bot's view is rarely on a
        // placeable spot).
        best.origin = home;
        best.canBePlaced = true;
        self notify( "place_sentry" );
        wait 0.5;
        {}
    }
}

reportSentries()
{
    foreach ( player in level.players )
        {}
    foreach ( turret in getEntArray( "misc_turret", "classname" ) )
    {
        if ( !isDefined( turret.sentryType ) )
            continue;
        ownerName = "none";
        if ( isDefined( turret.owner ) )
            ownerName = turret.owner.name;
        carrier = "none";
        if ( isDefined( turret.carriedBy ) )
            carrier = turret.carriedBy.name;
        teamName = "none";
        if ( isDefined( turret.team ) )
            teamName = turret.team;
        target = turret getTurretTarget( false );
        targetName = "none";
        if ( isDefined( target ) && isPlayer( target ) )
            targetName = target.name + "(" + target.team + ")";
        else if ( isDefined( target ) )
            targetName = target.classname;
        {}
    }
}

placeCarriedKillstreak()
{
    self endon( "death" );
    self endon( "disconnect" );
    // Placement is refused while the carrier is airborne or the spot is bad
    // (updateSentryPlacement), so keep asking for a while as a player would.
    for ( i = 0; i < 20; i++ )
    {
        wait 0.5;
        self notify( "place_sentry" );
    }
    turrets = getEntArray( "misc_turret", "classname" );
    {}
    foreach ( turret in turrets )
    {
        if ( !isDefined( turret.sentryType ) )
            continue;
        {}
    }
}

// Two modes:
//   scr_codol_botfill 0 (default): keep scr_codol_bots bots (training); lowering
//       the count stops replenishment and never kicks.
//   scr_codol_botfill N > 0: fill around the real players. With no real player
//       there are no bots; otherwise N - humans bots, never more than
//       scr_codol_botmax (default 11), adding and kicking as players come and go.
maintainBots()
{
    level endon( "game_ended" );
    if ( getDvar( "scr_codol_botfill" ) == "" )
        setDvar( "scr_codol_botfill", 0 );
    if ( getDvar( "scr_codol_botmax" ) == "" )
        setDvar( "scr_codol_botmax", 11 );
    // After a round restart, let the previous round's clients reconnect
    // before counting them.
    if ( game["codolBotSerial"] > 0 )
        wait 2;
	for ( ;; )
	{
		matchLimit = getDvarInt( "sv_maxclients" );
		if ( level.gameType == "gun" || level.gameType == "oic" || level.gameType == "shrp" )
		{
			if ( matchLimit > 4 )
				matchLimit = 4;
		}
		bots = [];
        humans = 0;
        players = getEntArray( "player", "classname" );
        foreach ( player in players )
        {
            if ( isCombatBot( player ) )
                bots[bots.size] = player;
            else if ( !player.isbot )
                humans++;
        }
		// Test hook, idle unless set: pretend this many more real players are on.
		humans += getDvarInt( "scr_codol_botfill_testhumans" );
		availableBots = matchLimit - humans;
		if ( humans == 0 )
			availableBots--;
		if ( availableBots < 0 )
			availableBots = 0;
		fill = getDvarInt( "scr_codol_botfill" );
        if ( fill > 0 )
        {
            desired = 0;
            if ( humans > 0 )
                desired = fill - humans;
            botMax = getDvarInt( "scr_codol_botmax" );
            if ( desired > botMax )
                desired = botMax;
			if ( desired < 0 )
				desired = 0;
			if ( desired > availableBots )
				desired = availableBots;
            if ( desired > bots.size )
                spawnBots( desired - bots.size );
            else if ( desired < bots.size )
                removeBots( bots, bots.size - desired );
        }
        else
        {
            desired = getDvarInt( "scr_codol_bots" );
			if ( desired > 16 )
				desired = 16;
			if ( desired > availableBots )
				desired = availableBots;
            if ( desired > bots.size )
                spawnBots( desired - bots.size );
        }
        wait 1;
    }
}

// Kick `count` bots, dead ones first (nobody is watching them), newest first.
removeBots( bots, count )
{
    for ( pass = 0; pass < 2 && count > 0; pass++ )
    {
        for ( i = bots.size - 1; i >= 0 && count > 0; i-- )
        {
            bot = bots[i];
            if ( !isDefined( bot ) || ( pass == 0 && isAlive( bot ) ) )
                continue;
            kick( bot getEntityNumber() );
            bots[i] = undefined;
            count--;
        }
    }
}

spawnBots( count )
{
    level endon( "game_ended" );
    while ( !isDefined( level.autoassign ) || !isDefined( level.class ) )
        wait 0.05;
    for ( i = 0; i < count; i++ )
    {
        bot = addTestClient( "Training " + ( game["codolBotSerial"] + 1 ) );
        if ( !isDefined( bot ) )
            return;
        bot.codolCombatBot = true;
        bot.codolBotClass = classForDifficulty( game["codolBotSerial"] );
        {}
        bot.pers["codolCombatBot"] = true;
        bot.pers["codolBotClass"] = bot.codolBotClass;
        game["codolBotSerial"]++;
        bot thread enterGame();
        wait 0.25;
    }
}

classForDifficulty( serial )
{
    eligible = [];
    tier = getDvar( "sv_botDifficulty" );
    for ( classIndex = 0; classIndex < 9; classIndex++ )
        if ( codol_botclassallowsdifficulty( classIndex, tier ) )
            eligible[eligible.size] = classIndex;
    // A missing or malformed table row must not prevent the match from
    // spawning; the native loadout checker will report the actual class data.
    if ( eligible.size == 0 )
        return serial % 9;
    return eligible[serial % eligible.size];
}

enterGame()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    // Let normal connected/menu threads establish pers and class state.
    wait 1;
    self thread watchSpawns();
    self [[level.autoassign]]();
    wait 0.25;
    self.selectedClass = true;
    self [[level.class]]( "class0" );
}

watchSpawns()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "spawned_player" );
        self enableCombatBot( true );
        self thread watchWedged();
        self thread maps\mp\gametypes\_codol_bot_equipment::onBotSpawned();
        self thread maps\mp\gametypes\_codol_bot_killstreaks::onBotSpawned();
        self thread maps\mp\gametypes\_codol_bot_objective::objectiveThink();
    }
}

// Native movement reports a player that nudge, repath and regoal could not
// move one unit (spawned in solid, or on unwalkable collision). A bot standing
// inside a wall forever is worse than one death: respawn it elsewhere.
watchWedged()
{
    self endon( "disconnect" );
    self endon( "death" );
    level endon( "game_ended" );
    // A bot driving a remote (RC-XD, predator) stands still on purpose.
    for ( ;; )
    {
        self waittill( "bot_wedged" );
        if ( !isDefined( self.usingRemote ) )
            break;
    }
    if ( isAlive( self ) )
        self suicide();
}

// Killstreak action-slot switching checks, driven the way C1 drives them: the
// "+actionslot N" command notify first, then the weapon switch C1 predicts.
//   ksslot <client> give <streak> <slot>  own a reward in that action slot
//   ksslot <client> press <slot> [carry]  press that slot (notify + switch);
//                                         "carry" also raises the carried
//                                         turret's "cancel_sentry", which the
//                                         native command dispatch raises for
//                                         every event registered on the slot
//   ksslot <client> fire                  hold attack (throws a held marker)
//   ksslot <client> primary               raise the first primary weapon
//   ksslot <client> ground                freeze the subject once it lands
//   ksslot <client> seq <a> <b> <seconds> press slot a, then b that much later
//   ksslot <client> state                 weapon, owned rewards, busy, UAVs
testRewardSlotVerb( tokens )
{
    subject = undefined;
    foreach ( player in level.players )
        if ( player getEntityNumber() == int( tokens[1] ) && isAlive( player ) )
            subject = player;
    if ( !isDefined( subject ) )
    {
        {}
        return;
    }
    subject.maxhealth = 100000;
    subject.health = 100000;
    if ( tokens[2] == "give" && tokens.size > 4 )
    {
        subject maps\mp\killstreaks\_killstreaks::giveKillstreak( tokens[3] );
        subject.pers["killstreaks"][0].actionSlot = int( tokens[4] );
        subject maps\mp\killstreaks\_killstreaks::giveOwnedKillstreakItem( true );
    }
    else if ( tokens[2] == "press" && tokens.size > 3 )
        subject thread pressRewardSlot( int( tokens[3] ), tokens.size > 4 && tokens[4] == "carry" );
    else if ( tokens[2] == "fire" )
        subject codol_botPressButtons( 1, 0.5 );
    else if ( tokens[2] == "seq" && tokens.size > 5 )
        subject thread pressRewardSlots( int( tokens[3] ), int( tokens[4] ), float( tokens[5] ) );
    else if ( tokens[2] == "ground" )
        subject thread freezeOnGround();
    else if ( tokens[2] == "primary" )
        subject setSpawnWeapon( subject maps\mp\killstreaks\_killstreaks::getFirstPrimaryWeapon() );
    subject reportRewardSlots( tokens[2] );
}

pressRewardSlots( first, second, delay )
{
    self endon( "disconnect" );
    self endon( "death" );
    self thread pressRewardSlot( first, false );
    wait delay;
    self thread pressRewardSlot( second, false );
}

// Rides and carried rewards refuse an airborne user, and bots jump: hold the
// subject still once it lands ("freeze <client> 0" releases it).
freezeOnGround()
{
    self endon( "disconnect" );
    self endon( "death" );
    for ( tries = 0; tries < 100 && !self isOnGround(); tries++ )
        wait 0.05;
    self freezeControls( true );
}

pressRewardSlot( slot, carry )
{
    self endon( "disconnect" );
    self endon( "death" );
    reward = self maps\mp\killstreaks\_killstreaks::codolRewardForSlot( slot );
    weapon = "none";
    if ( isDefined( reward ) )
        weapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( reward.streakName );
    // Rides and carried rewards refuse an airborne user; bots jump.
    for ( tries = 0; tries < 40 && !self isOnGround(); tries++ )
        wait 0.05;
    {}
    self notify( "codol_reward_slot_" + slot );
    if ( carry )
        self notify( "cancel_sentry" );
    // A test client's scripted switch does not reliably complete (see
    // throwKillstreakMarker), so raise the weapon directly: the native
    // "weapon_change" it raises stands for C1's completed predicted switch.
    // C1 cannot switch while weapons are disabled (a carried turret).
    // switchToWeapon makes a test client's usercmd name the weapon, as C1's
    // does after its predicted switch; without it the bot keeps asking for
    // the weapon it held and the engine switches back.
    if ( weapon != "none" && weapon != self getCurrentWeapon() && self common_scripts\utility::isWeaponEnabled() )
    {
        self switchToWeapon( weapon );
        self setSpawnWeapon( weapon );
    }
    wait 2.5;
    self reportRewardSlots( "after slot " + slot );
}

reportRewardSlots( label )
{
    owned = "";
    foreach ( reward in self.pers["killstreaks"] )
        owned = owned + reward.streakName + "#" + reward.kID + "@" + reward.actionSlot + " ";
    held = "";
    foreach ( item in self getWeaponsListAll() )
        if ( isDefined( level.killstreakWeapons[item] ) )
            held = held + item + ":" + self getAmmoCount( item ) + " ";
    {}
}
