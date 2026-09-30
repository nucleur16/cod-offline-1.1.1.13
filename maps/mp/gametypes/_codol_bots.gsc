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
    maps\mp\gametypes\_codol_breathtest::init();
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
                    codolPrintLn( "CoDOL killstreak test: hurt " + player.name );
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
                        codolPrintLn( "CoDOL rank test: " + player.name + " requested " + xpAmount +
                            " XP, stored " + oldXP + " -> " + player maps\mp\gametypes\_rank::getRankXP() );
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
                codolPrintLn( "CoDOL perk test: client " + perkTokens[1] + " is not alive" );
                continue;
            }
            if ( perkTokens[0] == "perk" )
                perkPlayer thread testPerk( perkTokens[2] );
            else if ( perkTokens[0] == "perkdmg" && perkTokens.size > 3 )
            {
                left = maps\mp\perks\_perks::cac_modified_damage( perkPlayer, level.players[0], int( perkTokens[3] ), perkTokens[2], "frag_grenade_mp", perkPlayer.origin, ( 0, 0, 1 ), "none" );
                codolPrintLn( "CoDOL perk test: " + perkPlayer.name + " takes " + left + " of " + perkTokens[3] + " " + perkTokens[2] );
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
                    codolPrintLn( "CoDOL killstreak test: relocated " + player.name + " to " + player.origin );
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
                    codolPrintLn( "CoDOL killstreak cycle test: " + player.name + " count " + count +
                        " thresholds [" + thresholds + "] owned [" + owned + "] selected [" + selected + "]" );
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
                    codolPrintLn( "CoDOL killstreak test: " + player.name + " holds " + player getCurrentWeapon() + " owns " + names );
                    player thread logWeaponChanges( 5 );
                    ok = player switchToWeapon( want );
                    codolPrintLn( "CoDOL killstreak test: switchToWeapon( " + want + " ) returned " + ok );
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
                        codolPrintLn( "CoDOL killstreak test: downed " + player.name + " by " + enemy.name );
                        player.maxhealth = 100;
                        player.health = 100;
                        player doDamage( 1000, player.origin + ( 0, 0, 40 ), enemy, enemy, 0, "MOD_RIFLE_BULLET" );
                    }
                    else if ( isDefined( player.lastStand ) )
                    {
                        codolPrintLn( "CoDOL killstreak test: revive " + player.name );
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
                        codolPrintLn( "CoDOL killstreak test: kill " + player.name );
                        player.maxhealth = 100;
                        player.health = 100;
                        player doDamage( 1000, player.origin + ( 0, 0, 40 ), level.players[0] );
                    }
                    if ( tiTokens[0] == "confirm" )
                    {
                        location = player.origin + anglesToForward( ( 0, player.angles[1], 0 ) ) * 600;
                        codolPrintLn( "CoDOL killstreak test: " + player.name + " confirms " + location + " (selecting " + isDefined( player.selectingLocation ) + ")" );
                        player notify( "confirm_location", location, player.angles[1] );
                    }
                }
            continue;
        }
        // "rcsafe <0|1>": 1 stops an RC-XD driver's trigger detonating the
        // car (T5 level.disableRCBombTrigger), so a bot's ride lasts until
        // its timer; bots hold attack.
        if ( tiTokens[0] == "rcsafe" && tiTokens.size > 1 )
        {
            level.disableRCBombTrigger = int( tiTokens[1] ) != 0;
            codolPrintLn( "CoDOL killstreak test: rcsafe " + level.disableRCBombTrigger );
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
            codolPrintLn( "CoDOL killstreak test: no handler for " + streakRef );
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
            codolPrintLn( "CoDOL killstreak test: client " + clientNum + " is not alive" );
            continue;
        }
        // Rides and carried rewards refuse an airborne user; bots jump.
        target freezeControls( true );
        for ( tries = 0; tries < 60 && !target isOnGround(); tries++ )
            wait 0.1;
        target freezeControls( false );
        target.maxhealth = 100000;
        target.health = 100000;
        codolPrintLn( "CoDOL killstreak test: " + target.name + " (" + target.team + ") uses " + streakRef );
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
    codolPrintLn( "CoDOL killstreak test: " + self.name + " activated " + reward.streakName + " holding " + self getCurrentWeapon() );
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
    codolPrintLn( "CoDOL killstreak test: state " + self.name + " alive " + isAlive( self ) +
        " origin " + self.origin + " angles " + self.angles + " onGround " + self isOnGround() +
        " weapon " + self getCurrentWeapon() + " busy " + ( isDefined( self.codolRewardActivationBusy ) && self.codolRewardActivationBusy ) +
        " usingRemote " + isDefined( self.usingRemote ) + " selecting " + isDefined( self.selectingLocation ) +
        " remoteCtl " + self isRemoteControlling() + " rcClaim " + claim + rule +
        " littleBirds " + level.littleBirds + " owned [" + owned + "]" );
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
    codolPrintLn( "CoDOL killstreak test: offhands " + self.name + " hold " + hold + " switchHold " + switchHold +
        " lastStand " + isDefined( self.lastStand ) + " [" + offhands + "]" );
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
    codolPrintLn( "CoDOL killstreak test: " + self.name + " confirms location " + location + " (selecting " + isDefined( self.selectingLocation ) + ")" );
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
    codolPrintLn( "CoDOL killstreak test: entities " + line );
}

testCrateTable()
{
    foreach ( dropType, crateTypes in level.crateTypes )
    {
        line = "";
        foreach ( crateType, crateWeight in crateTypes )
            line = line + crateType + "=" + crateWeight + " ";
        codolPrintLn( "CoDOL killstreak test: crate table " + dropType + " max " + level.crateMaxVal[dropType] + ": " + line );
    }
    draws = "";
    for ( i = 0; i < 10; i++ )
        draws = draws + randomInt( 125 ) + ":" + maps\mp\killstreaks\_airdrop::getRandomCrateType( "airdrop" ) + " ";
    codolPrintLn( "CoDOL killstreak test: crate draws " + draws );
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
        codolPrintLn( "CoDOL killstreak test: no landed care package to capture" );
        return;
    }
    self.maxhealth = 100000;
    self.health = 100000;
    crateType = best.crateType;
    self setOrigin( best.origin + ( 48, 0, 0 ) );
    self setPlayerAngles( vectorToAngles( best.origin - ( self.origin + ( 0, 0, 60 ) ) ) );
    codolPrintLn( "CoDOL killstreak test: " + self.name + " (" + self.team + ") captures " + crateType + " crate at " + best.origin + " owner " + best.owner.name );
    for ( i = 0; i < 12 && isDefined( best ); i++ )
    {
        self codol_botPressButtons( 40, 1.0 );
        wait 1.0;
    }
    codolPrintLn( "CoDOL killstreak test: capture by " + self.name + " done, crate gone " + !isDefined( best ) );
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
    codolPrintLn( "CoDOL killstreak test: " + self.name + " before switch holds " + self getCurrentWeapon() + ", has " + weapon + " " + self hasWeapon( weapon ) + " ammo " + self getAmmoCount( weapon ) );
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
    codolPrintLn( "CoDOL killstreak test: " + self.name + " holds " + self getCurrentWeapon() + " for " + streakRef + " (wants " + weapon + ")" );
    self thread logMarkerThrow( weapon );
    for ( attempt = 1; attempt <= 6; attempt++ )
    {
        if ( isDefined( self.codolMarkerThrown ) )
            return;
        self codol_botPressButtons( 1, 0.4 );
        wait 1.5;
    }
    if ( !isDefined( self.codolMarkerThrown ) )
        codolPrintLn( "CoDOL killstreak test: " + self.name + " never threw " + weapon + " (holding " + self getCurrentWeapon() + ")" );
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
        codolPrintLn( "CoDOL killstreak test: " + self.name + " weapon_change " + newWeapon + " at " + getTime() );
    }
}

trackMarker( weapname )
{
    self endon( "death" );
    self endon( "explode" );
    start = getTime();
    for ( i = 0; i < 40; i++ )
    {
        codolPrintLn( "CoDOL killstreak test: " + weapname + " t+" + ( getTime() - start ) + " at " + self.origin );
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
        codolPrintLn( "CoDOL killstreak test: " + self.name + " threw " + weapname + " from " + self.origin );
        grenade thread trackMarker( weapname );
        grenade waittill( "explode", position );
        codolPrintLn( "CoDOL killstreak test: " + weapname + " exploded at " + position );
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
        codolPrintLn( "CoDOL killstreak coverage: row " + row + " id " + tableLookupByRow( table, row, 0 ) +
            " " + ref + " weapon " + tableLookupByRow( table, row, 13 ) + " base " + tableLookupByRow( table, row, 22 ) +
            " handler " + handled );
    }
    codolPrintLn( "CoDOL killstreak coverage: " + missing + " catalogue row(s) without a handler" );
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
        codolPrintLn( "CoDOL killstreak row " + row + ": " + line );
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
        codolPrintLn( "CoDOL killstreak test: TI for " + self.name + ": flare_mp was not kept (shared frag ammo cap)" );
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = self getOffhandEquipment( i );
    slots[2] = "flare_mp";
    self setOffhandEquipment( slots[0], slots[1], slots[2] );
    codolPrintLn( "CoDOL killstreak test: TI slots for " + self.name + ": " + self getOffhandEquipment( 0 ) + " / " +
        self getOffhandEquipment( 1 ) + " / " + self getOffhandEquipment( 2 ) + ", flare ammo " + self getAmmoCount( "flare_mp" ) );
    wait 1.0;
    self maps\mp\gametypes\_codol_equiptest::pressUntilThrown( 65536, "flare_mp" );
    wait 1.0;
    if ( !isDefined( self.setSpawnPoint ) )
    {
        codolPrintLn( "CoDOL killstreak test: TI for " + self.name + " placed no beacon (TISpawnPosition " + isDefined( self.TISpawnPosition ) + ")" );
        return;
    }
    beacon = self.setSpawnPoint.origin;
    codolPrintLn( "CoDOL killstreak test: TI beacon for " + self.name + " at " + beacon );
    self suicide();
    self waittill( "spawned_player" );
    codolPrintLn( "CoDOL killstreak test: TI respawn for " + self.name + " at " + self.origin + ", " + distance( self.origin, beacon ) + " from the beacon" );
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
    codolPrintLn( report );
}

testSuicide()
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
    {
        codolPrintLn( "CoDOL killstreak test: " + self.name + " never came back alive" );
        return;
    }
    codolPrintLn( "CoDOL killstreak test: suicide " + self.name );
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
    codolPrintLn( "CoDOL killstreak test: " + self.name + " respawned at " + self.origin );
}

reportAliveAfterSuicide()
{
    self endon( "disconnect" );
    for ( i = 0; i < 8; i++ )
    {
        wait 1.0;
        codolPrintLn( "CoDOL killstreak test: " + self.name + " t+" + ( i + 1 ) + "s alive " + isAlive( self ) + " health " + self.health + " session " + self.sessionstate );
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
        codolPrintLn( "CoDOL sentry test: " + self.name + " is not carrying a sentry" );
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
    codolPrintLn( "CoDOL sentry test: " + self.name + " (" + self.team + ") placed " + !isDefined( carried.carriedBy ) + " at " + carried.origin + " yaw " + carried.angles[1] );
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
    codolPrintLn( "CoDOL sentry test: victim " + self.name + " (" + self.team + ") at " + self.origin + " in front of " + best getEntityNumber() );
    self thread testSentryVictimDeath( best );
    wait float( tokens[4] );
    if ( isAlive( self ) )
    {
        self freezeControls( false );
        codolPrintLn( "CoDOL sentry test: victim " + self.name + " (" + self.team + ") survived with health " + self.health );
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
    codolPrintLn( "CoDOL sentry test: victim " + self.name + " (" + self.team + ") killed by " + attackerName + " with " + weapon + " " + meansOfDeath );
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
        codolPrintLn( "CoDOL sentry test: no placed turret to use" );
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
    codolPrintLn( "CoDOL sentry test: " + self.name + " (" + self.team + ") uses " + best.sentryType + " " + best getEntityNumber() +
        " owned by " + ownerName + " from yaw offset " + yawOffset + ", onGround " + self isOnGround() + " at " + self.origin + " turret at " + best.origin + " tries " + tries );
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
    codolPrintLn( "CoDOL sentry test: after use by " + self.name + " the turret is carried by " + carrier );
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
        codolPrintLn( "CoDOL sentry test: " + self.name + " placed it again, carried " + isDefined( best.carriedBy ) );
    }
}

reportSentries()
{
    foreach ( player in level.players )
        codolPrintLn( "CoDOL sentry test: player " + player getEntityNumber() + " " + player.name + " team " + player.team + " alive " + isAlive( player ) + " health " + player.health + " origin " + player.origin );
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
        codolPrintLn( "CoDOL sentry test: " + turret.sentryType + " " + turret getEntityNumber() + " mode " + turret getMode() +
            " owner " + ownerName + " team " + teamName + " carriedBy " + carrier + " target " + targetName +
            " firing " + turret isFiringTurret() + " angles " + turret.angles + " tag_flash " + turret getTagAngles( "tag_flash" ) );
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
    codolPrintLn( "CoDOL killstreak test: " + turrets.size + " misc_turret(s) after placement for " + self.name +
        ", onGround " + self isOnGround() );
    foreach ( turret in turrets )
    {
        if ( !isDefined( turret.sentryType ) )
            continue;
        codolPrintLn( "CoDOL killstreak test:   " + turret.sentryType + " at " + turret.origin +
            " carried " + isDefined( turret.carriedBy ) + " canBePlaced " + turret.canBePlaced );
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
        codolPrintLn( "CoDOL bot difficulty: " + getDvar( "sv_botDifficulty" ) + " class " + bot.codolBotClass + " for " + bot.name );
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
