// CoDOL multiplayer perk-challenge test hook (Quickdraw Pro and the other
// CoDOL Pro unlock challenges in _missions.gsc). Idle unless developer is set
// and the dvar names a request. Kills and damage go through the gametype's own
// damage callback, ADS through the bot's real usercmd buttons, so the
// challenge code sees what a match would give it.
//
//   set scr_codol_chtest "admit <client>"
//       admit a training bot bound to a test profile (codol_bindprofile) to
//       challenge progress, which bots never get, load its tiers and stop
//       its combat AI.
//   set scr_codol_chtest "kill <killer> <victim> <ads 0|1> <perk[,perk]|none> [mod]"
//       killer gets the perks ("-<perk>" removes one), holds ADS (1) or not (0), then kills victim
//       with its current weapon and [mod] (default MOD_RIFLE_BULLET).
//   set scr_codol_chtest "blast <victim> <attacker> <perk[,perk]|none>"
//       victim gets the perks and survives a 10-damage frag splash.
//   set scr_codol_chtest "paint <painter> <victim> <perk[,perk]|none> <mod>"
//       painter gets the perks and hurts victim for 10 with <mod>.
//   set scr_codol_chtest "throwback <client> <perk[,perk]|none>"
//       client gets the perks and throws a real frag whose pullback is then
//       forgotten, so _weapons.gsc takes it for one picked up off the ground.
//   set scr_codol_chtest "join <client> <allies|axis>"
//       a spectating test client picks the team and class0, as _dev.gsc's do.
//   set scr_codol_chtest "arm <client> <weapon>"
//       give the client the weapon and put it in hand.
//   set scr_codol_chtest "show <client>"
//       the client's perk challenge progress and derived tiers.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    if ( isDefined( level.codolChTestStarted ) )
        return;
    level.codolChTestStarted = true;
    setDvar( "scr_codol_chtest", "" );
    level thread watchDvar();
}

watchDvar()
{
    if ( getDvarInt( "developer" ) <= 0 )
        return;
    level endon( "game_ended" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_chtest" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_chtest", "" );
        tokens = strTok( request, " " );
        if ( tokens.size < 2 )
            continue;
        a = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
        if ( !isDefined( a ) )
        {
            report( "no client " + tokens[1] );
            continue;
        }
        b = undefined;
        if ( tokens.size > 2 )
            b = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[2] ) );

        switch ( tokens[0] )
        {
        case "admit":
            a admit();
            break;
        case "show":
            a show();
            break;
        case "arm":
            if ( tokens.size > 2 )
            {
                a giveWeapon( tokens[2] );
                a giveMaxAmmo( tokens[2] );
                a setSpawnWeapon( tokens[2] );
                a switchToWeapon( tokens[2] );
            }
            break;
        case "join":
            if ( tokens.size > 2 )
                a thread join( tokens[2] );
            break;
        case "kill":
            if ( isDefined( b ) && tokens.size > 4 )
            {
                mod = "MOD_RIFLE_BULLET";
                if ( tokens.size > 5 )
                    mod = tokens[5];
                a thread testKill( b, int( tokens[3] ), tokens[4], mod );
            }
            break;
        case "blast":
            if ( isDefined( b ) && tokens.size > 3 )
                a thread testBlast( b, tokens[3] );
            break;
        case "paint":
            if ( isDefined( b ) && tokens.size > 4 )
                a thread testPaint( b, tokens[3], tokens[4] );
            break;
        case "throwback":
            if ( tokens.size > 2 )
                a thread testThrowback( tokens[2] );
            break;
        }
    }
}

report( text )
{
    codolPrintLn( "CoDOL chtest: " + text );
}

perkRefs()
{
    return strTok( "ch_quickdraw_pro ch_stalker_pro ch_marksman_pro ch_flakjacket_pro ch_engineer_pro ch_paint_pro ch_bulletpaint ch_sleightofhand_pro ch_bulletaccuracy_pro", " " );
}

admit()
{
    self.codolChTest = true;
    self enableCombatBot( false );
    if ( !isDefined( self.challengeData ) )
        self.challengeData = [];
    foreach ( ref in perkRefs() )
        self.challengeData[ref] = self maps\mp\gametypes\_missions::ch_getState( ref );
    report( self.name + " admitted" );
    self show();
}

show()
{
    text = self.name + " (" + self getEntityNumber() + ") holds " + self getCurrentWeapon() + " of";
    foreach ( weapon in self getWeaponsListPrimaries() )
        text += " " + weapon;
    text += ";";
    foreach ( ref in perkRefs() )
    {
        status = "-";
        if ( isDefined( self.challengeData ) && isDefined( self.challengeData[ref] ) )
            status = self.challengeData[ref];
        text += " " + ref + "=" + self maps\mp\gametypes\_missions::ch_getProgress( ref ) + "/t" + status;
    }
    report( text );
}

join( team )
{
    self notify( "menuresponse", game["menu_team"], team );
    wait 0.5;
    self notify( "menuresponse", "changeclass", "class0" );
    self waittill_notify_or_timeout( "spawned_player", 10 );
    report( self.name + " team " + self.team + " alive " + isReallyAlive( self ) + " weapon " + self getCurrentWeapon() );
}

givePerks( perks )
{
    if ( perks == "none" )
        return;
    foreach ( name in strTok( perks, "," ) )
    {
        // "-<perk>" takes it away instead.
        if ( getSubStr( name, 0, 1 ) == "-" )
            self _unsetPerk( getSubStr( name, 1 ) );
        else
            self maps\mp\perks\_perks::givePerk( name );
    }
}

testKill( victim, ads, perks, mod )
{
    self givePerks( perks );
    victim enableCombatBot( false );
    if ( ads )
    {
        // C1 usercmd ADS (2048) plus the ADS trigger bit, as codol_bot.c holds them.
        self codol_botPressButtons( 2048 + 1048576, 3 );
        wait 0.8;
    }
    weapon = self getCurrentWeapon();
    report( self.name + " kills " + victim.name + " weapon " + weapon + " mod " + mod + " playerAds " + self playerAds() +
        " quickdraw " + self _hasPerk( "specialty_quickdraw" ) + " stalker " + self _hasPerk( "specialty_stalker" ) + " marksman " + self _hasPerk( "specialty_marksman" ) );
    victim [[level.callbackPlayerDamage]]( self, self, victim.health + 1000, 0, mod, weapon, victim.origin, ( 0, 0, 1 ), "torso_upper", 0 );
    self waittill_notify_or_timeout( "playerKilledChallengesProcessed", 3 );
    wait 0.1;
    if ( ads )
        self codol_botPressButtons( 0, 0 );
    self show();
}

testBlast( attacker, perks )
{
    self givePerks( perks );
    report( self.name + " takes a frag splash from " + attacker.name + ", flakjacket " + self _hasPerk( "specialty_flakjacket" ) + " health " + self.health );
    self [[level.callbackPlayerDamage]]( attacker, attacker, 10, 0, "MOD_GRENADE_SPLASH", "frag_grenade_mp", self.origin, ( 0, 0, 1 ), "torso_upper", 0 );
    wait 0.5;
    report( self.name + " alive " + isReallyAlive( self ) );
    self show();
}

testPaint( victim, perks, mod )
{
    self givePerks( perks );
    weapon = self getCurrentWeapon();
    if ( mod != "MOD_RIFLE_BULLET" && mod != "MOD_PISTOL_BULLET" )
        weapon = "frag_grenade_mp";
    report( self.name + " hurts " + victim.name + " with " + weapon + " " + mod + " modExplosivePaint " + self HasAbilityModifier( "modExplosivePaint" ) +
        " modBulletPaint " + self HasAbilityModifier( "modBulletPaint" ) + " victim painted " + victim maps\mp\perks\_perkfunctions::isPainted() );
    victim [[level.callbackPlayerDamage]]( self, self, 10, 0, mod, weapon, victim.origin, ( 0, 0, 1 ), "torso_upper", 0 );
    wait 0.2;
    report( victim.name + " painted " + victim maps\mp\perks\_perkfunctions::isPainted() );
    self show();
}

testThrowback( perks )
{
    self givePerks( perks );
    report( self.name + " throws a frag the engine treats as thrown back, engineer " + self _hasPerk( "specialty_engineer" ) );
    self thread forgetPullback();
    self thread maps\mp\gametypes\_codol_equiptest::throwEquipment( 0, "frag_grenade_mp" );
    self waittill_notify_or_timeout( "grenade_fire", 12 );
    wait 0.5;
    self show();
}

// _weapons.gsc counts a frag fired with no grenade_pullback first as one
// picked up off the ground; clear the flag the real pullback just set.
forgetPullback()
{
    self endon( "disconnect" );
    self waittill( "grenade_pullback" );
    wait 0.05;
    self.gotPullbackNotify = false;
}
