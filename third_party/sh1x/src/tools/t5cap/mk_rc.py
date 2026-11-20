"""Regenerate the CoDOL copy of the RC-XD capture (tools/t5cap/zz_rccap.gsc):
the body is shared, only the platform block differs.

    python tools/t5cap/mk_rc.py bin/home/codol_rawfiles/maps/mp/killstreaks/_rcbomb_t5cap.gsc
"""
import os
import sys
src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'zz_rccap.gsc')
dst = sys.argv[1]
s = open(src).read()
a = s.index('// ---- platform')
b = s.index('// ---- end platform ----')
plat = r'''// ---- platform: CoDOL ----
// Developer-only RC-XD telemetry (docs/RCXD_T5_PARITY.md), generated
// from tools/t5cap/zz_rccap.gsc by tools/t5cap/mk_rc.py. Inactive unless
// the dvar rccap_scn is set; _rcbomb::init starts it. The driver is a test
// client: throttle through sh1x_rcTestThrottle, buttons through
// sh1x_rcTestButtons, steering through its view yaw (setPlayerAngles), the
// same view-yaw steering a T5 driver's camera gives.
#include maps\mp\_utility;
#include common_scripts\utility;

cap_platform()
{
	return "codol";
}

cap_wait_player()
{
	while ( !isDefined( level.autoassign ) || !isDefined( level.class ) )
		wait 0.05;
	wait 2;
	bot = cap_add_bot();
	for ( i = 0; i < 400 && !isAlive( bot ); i++ )
		wait 0.05;
	wait 1;
	return bot;
}

cap_platform_init()
{
	setDvar( "scr_rcbomb_notimeout", "0" );
	setDvar( "sh1x_rcTestThrottle", "0" );
	setDvar( "sh1x_rcTestButtons", "0" );
}

cap_quit()
{
	setDvar( "sh1x_rcTestThrottle", "0" );
	setDvar( "sh1x_rcTestButtons", "0" );
	codolPrintLn( "rccap: done" );
}

cap_deploy()
{
	self maps\mp\killstreaks\_rcbomb::useKillstreakRCBomb( 0 );
}

cap_throttle( v )
{
	setDvar( "sh1x_rcTestThrottle", v );
}

cap_button( name, on )
{
	mask = 0;
	if ( name == "boost" )
		mask = 1024;
	else if ( name == "jump" || name == "ads" )
		mask = 2048;
	if ( !on )
		mask = 0;
	setDvar( "sh1x_rcTestButtons", mask );
}

cap_add_bot()
{
	bot = addTestClient( "rccap" + gettime() );
	if ( !isDefined( bot ) )
		return undefined;
	// Loadout from the bot class table (a test client has no profile), but
	// enableCombatBot is never called: the bot stays idle, the script drives.
	bot.codolCombatBot = true;
	bot.codolBotClass = 0;
	bot.pers["codolCombatBot"] = true;
	bot.pers["codolBotClass"] = 0;
	bot thread cap_bot_join();
	return bot;
}

cap_bot_join()
{
	self endon( "disconnect" );
	wait 1;
	self [[ level.autoassign ]]();
	wait 0.25;
	self.selectedClass = true;
	self [[ level.class ]]( "class0" );
}

cap_vehicle_speed( car )
{
	return car vehicle_getspeed() * 17.6;
}

cap_hit( car, att, amount )
{
	car doDamage( amount, car.origin + ( 0, 0, 8 ), att, att, 0, "MOD_RIFLE_BULLET", 0, att getCurrentWeapon() );
}
'''
open(dst, 'w', newline='\n').write(s[:a] + plat + s[b:])
print('wrote', dst)
