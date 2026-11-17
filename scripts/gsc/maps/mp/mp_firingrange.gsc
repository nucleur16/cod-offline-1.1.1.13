// PROVENANCE (2026-10-14): CoDOL mp_firingrange is a 3.3.3.3 map (Black Ops Firing Range)
// ported to 1.1.1.13 by CODOL-AssetExplorer (port-map). The zone ships
// maps/mp/mp_firingrange_fx.gsc and maps/createfx/mp_firingrange_fx.gsc (extracted verbatim
// here) but not its level script, so this is IW4 maps/mp/mp_firingrange.gsc (COD-GSC-Source
// IW4-GSC, the IW4 port of the T5 map) adapted:
//   * order: fx and art before _load, as every IW4 MP level script has it (the IW4 file ran
//     _load first). maps\createfx\mp_firingrange_fx::main() is not called: the client plays
//     the createfx ents itself (clientSideEffects), and the zone's own fx file never calls it.
//   * the moving range targets, as IW4 has them, plus T5 maps/mp/mp_firingrange.gsc's
//     steel-building light targets (damageTargetLights): the zone places
//     steelBuildingTargetLight1/2_on/_off and loudspeaker1/2 and ships amb_target_buzzer.
//     T5's exploder pulse on those targets is left out (the red light fx exploders are T5
//     client-side). Every entity is fetched through codol_getEnt, so a missing one leaves
//     its target idle instead of raising a script error.
//   * explodable barrels: the map places four, so _explosive_barrels runs before _load (IW4
//     mp_terminal order).
//   * the eight Black Ops "destructible" entities (targetname destructible, destructibledef
//     dest_airconditioner_01 / dest_electronic_outlet01 / dest_rus_console_box_a) need T5's
//     native DestructibleDef support, which this server does not have; they stay as static
//     props. The destructible_toy / destructible_vehicle entities use the IW4 machine
//     (toy_tubetv_tv4 and toy_propane_tanks_02 are CoDOL additions to
//     common_scripts/_destructible_types.gsc).
//   * compass: compass_map_mp_firingrange ships in the zone; ambient: the zone's own
//     ambient_mp_firingrange alias (IW4 used ambient_mp_estate).
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_firingrange_fx::main();
	maps\createart\mp_firingrange_art::main();
	maps\mp\_explosive_barrels::main();

	maps\mp\_load::main();

	ambientPlay( "ambient_mp_firingrange" );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_firingrange" );

	setDvar( "r_diffusecolorscale", "1.5" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	alleyTrigger = codol_getEnt( "alleyTrigger" );
	windowTrigger = codol_getEnt( "triggerwindowTarget" );

	target1 = codol_getEnt( "fieldTarget_BackLeft" );
	target2 = codol_getEnt( "fieldTarget_FrontLeft" );
	target3 = codol_getEnt( "fieldTarget_Middle" );
	target4 = codol_getEnt( "fieldTarget_BackRight" );
	target5 = codol_getEnt( "fieldTarget_FrontRight" );
	target6 = codol_getEnt( "trenchTarget_GroundWall" );
	target7 = codol_getEnt( "trailerTarget_Window" );
	target8 = codol_getEnt( "alleyTarget_Cover" );
	target9 = codol_getEnt( "alleyTarget_Path" );
	target10 = codol_getEnt( "centerTarget_Sandbags" );
	target11 = codol_getEnt( "towerTarget_Front" );
	target12 = codol_getEnt( "towerTarget_Back" );
	target13 = codol_getEnt( "centerTarget_Path" );
	target14 = codol_getEnt( "centerTarget_PathBunkerL" );
	target15 = codol_getEnt( "centerTarget_PathBunkerR" );
	target16 = codol_getEnt( "steelBuildingTarget_Slide1" );
	target17 = codol_getEnt( "steelBuildingTarget_PopUp" );
	target18 = codol_getEnt( "target_alleyWindow1" );
	target19 = codol_getEnt( "target_alleyWindow2" );
	target20 = codol_getEnt( "target_alleyWindow3" );

	targetLight1_off = codol_getEnt( "steelBuildingTargetLight1_off" );
	targetLight1_on = codol_getEnt( "steelBuildingTargetLight1_on" );
	targetLight2_off = codol_getEnt( "steelBuildingTargetLight2_off" );
	targetLight2_on = codol_getEnt( "steelBuildingTargetLight2_on" );

	speaker1 = codol_getEnt( "loudspeaker1" );
	speaker2 = codol_getEnt( "loudspeaker2" );

	if ( isDefined( targetLight1_on ) )
		targetLight1_on hide();
	if ( isDefined( targetLight2_on ) )
		targetLight2_on hide();

	codol_damageTarget( target1, 1 );
	codol_damageTarget( target2, 1 );
	codol_damageTarget( target3, 1 );
	codol_damageTarget( target4, 1 );
	codol_damageTarget( target5, 1 );
	codol_damageTarget( target8, 2 );
	codol_damageTarget( target9, 2 );
	codol_damageTarget( target10, 2 );
	codol_damageTarget( target13, 2 );
	codol_damageTarget( target14, 3 );
	codol_damageTarget( target15, 3 );
	codol_damageTarget( target18, 4 );
	codol_damageTarget( target19, 4 );
	codol_damageTarget( target20, 5 );

	if ( isDefined( target16 ) )
	{
		target16 setCanDamage( true );
		target16 thread damageTargetLights( targetLight1_on, targetLight1_off, speaker1, "amb_target_buzzer" );
	}
	if ( isDefined( target17 ) )
	{
		target17 setCanDamage( true );
		target17 thread damageTargetLights( targetLight2_on, targetLight2_off, speaker2, "amb_target_buzzer" );
	}

	codol_moveTarget( target1, 4, 220, 10.1 );
	codol_moveTarget( target2, 4, 220, 5.2 );
	codol_moveTarget( target3, 4, 220, 10.3 );
	codol_moveTarget( target4, 3, 290, 8.4 );
	codol_moveTarget( target5, 3, 285, 3 );
	codol_moveTarget( target6, 1, 228, 8.1 );
	codol_moveTarget( target7, 7, ( 57, 23, 0 ), 3 );
	codol_moveTarget( target8, 1, 250, 5.5 );
	codol_moveTarget( target9, 1, 146, 8.6 );
	codol_moveTarget( target10, 1, 165, 8.7 );
	codol_moveTarget( target11, 4, 136, 5.05 );
	codol_moveTarget( target12, 3, 136, 7.15 );
	codol_moveTarget( target13, 1, 228, 8.25 );
	codol_moveTarget( target16, 4, 164, 5.35 );
	codol_moveTarget( target17, 5, 48, 5.45 );
	codol_moveTarget( target18, 3, 270, 8.55 );
	codol_moveTarget( target19, 6, 70, 6.65 );
	codol_moveTarget( target20, 1, 130, 5.75 );

	if ( isDefined( target11 ) )
		target11 thread rotateTarget( 2, 90, 0.5, 2 );
	if ( isDefined( target12 ) )
		target12 thread rotateTarget( 1, 90, 0.7, 3 );

	if ( isDefined( target9 ) && isDefined( alleyTrigger ) )
	{
		target9.triggeroff = true;
		alleyTrigger thread triggerCheck( target9 );
	}
	if ( isDefined( target7 ) && isDefined( windowTrigger ) )
	{
		target7.triggeroff = true;
		windowTrigger thread triggerCheck( target7 );
	}
}

// CoDOL: getEnt that reports and returns undefined for a missing entity instead of
// raising, since a script error ends the match on this server.
codol_getEnt( name )
{
	ents = getEntArray( name, "targetname" );
	if ( ents.size != 1 )
	{
		println( "CoDOL mp_firingrange: " + ents.size + " entities named " + name + "; expected 1" );
		if ( ents.size == 0 )
			return undefined;
	}
	return ents[ 0 ];
}

codol_damageTarget( target, dir )
{
	if ( !isDefined( target ) )
		return;
	target setCanDamage( true );
	target thread damageTarget( dir );
}

codol_moveTarget( target, dir, dis, speed )
{
	if ( !isDefined( target ) )
		return;
	target thread moveTarget( dir, dis, speed );
}

triggerCheck( target )
{
	self endon( "game_ended" );
	while ( 1 )
	{
		self waittill( "trigger", player );

		distance = Distance( target.origin, self.origin );
		if ( distance <= 90 )
		{
			if ( isDefined( target.triggeroff ) )
				target.triggeroff = false;

			target notify( "targetStopMoving" );

			while ( isDefined( player ) && player isTouching( self ) && distance <= 90 )
				wait 0.1;

			if ( isDefined( target.triggeroff ) )
				target.triggeroff = true;

			target notify( "targetStopMoving" );
		}
	}
}

damageTarget( dir )
{
	self endon( "game_ended" );
	while ( 1 )
	{
		self waittill( "damage", damage, attacker, direction );

		switch ( dir )
		{
			case 1:
				self rotateroll( self.angles[ 1 ] + 90, .1 );
				wait( .2 );
				self rotateroll( self.angles[ 1 ] - 90, .1 );
				wait( .2 );
				self playSound( "amb_target_flip" );
				break;
			case 2:
				self rotateYaw( self.angles[ 2 ] + 180, .3 );
				self playSound( "amb_target_twirl" );
				self waittill( "rotatedone" );
				break;
			case 3:
				self rotatepitch( self.angles[ 1 ] + 90, .1 );
				wait( .2 );
				self rotatepitch( self.angles[ 1 ] - 90, .1 );
				wait( .2 );
				self playSound( "amb_target_flip" );
				break;
			case 4:
				self rotateroll( self.angles[ 1 ] - 90, .1 );
				wait( .2 );
				self rotateroll( self.angles[ 1 ] + 90, .1 );
				wait( .2 );
				self playSound( "amb_target_flip" );
				break;
			case 5:
				self rotatepitch( self.angles[ 1 ] - 90, .1 );
				wait( .2 );
				self rotatepitch( self.angles[ 1 ] + 90, .1 );
				wait( .2 );
				self playSound( "amb_target_flip" );
				break;
		}
	}
}

// T5 mp_firingrange damageTargetLights, without its exploder pulse.
damageTargetLights( light_on, light_off, speaker, alias )
{
	self endon( "game_ended" );
	while ( 1 )
	{
		self waittill( "damage" );

		if ( isDefined( speaker ) )
			speaker playSound( alias );
		if ( isDefined( light_off ) )
			light_off hide();
		if ( isDefined( light_on ) )
			light_on show();
		wait( 0.5 );
		if ( isDefined( light_off ) )
			light_off show();
		if ( isDefined( light_on ) )
			light_on hide();
	}
}

moveTarget( dir, dis, speed )
{
	self endon( "game_ended" );
	keepMoving = true;

	startPOS = self.origin;
	farPOS = self.origin;

	sound = spawn( "script_origin", self.origin );
	sound linkTo( self );
	sound playLoopSound( "amb_target_chain" );

	switch ( dir )
	{
		case 1:
			farPOS = self.origin + ( 0, dis, 0 );
			break;
		case 2:
			farPOS = self.origin - ( 0, dis, 0 );
			break;
		case 3:
			farPOS = self.origin + ( dis, 0, 0 );
			break;
		case 4:
			farPOS = self.origin - ( dis, 0, 0 );
			break;
		case 5:
			farPOS = self.origin + ( 0, 0, dis );
			break;
		case 6:
			farPOS = self.origin - ( 0, 0, dis );
			break;
		case 7:
			farPOS = self.origin - dis;
			break;
	}

	while ( 1 )
	{
		if ( isDefined( self.triggeroff ) )
			keepMoving = self.triggeroff;

		if ( dir == 7 )
		{
			if ( keepMoving == false )
				self waittill( "targetStopMoving" );

			self moveTo( farPOS, speed );
			self waittill_any( "movedone", "targetStopMoving" );
			self playSound( "amb_target_stop" );
			if ( isDefined( self.triggeroff ) )
				keepMoving = self.triggeroff;

			if ( keepMoving == false )
				self waittill( "targetStopMoving" );
			self moveTo( startPOS, speed );
			self waittill_any( "movedone", "targetStopMoving" );
			self playSound( "amb_target_stop" );
			continue;
		}

		// IW4 cases 1-6 are one sequence: out, then back unless a trigger holds the target.
		self moveTo( farPOS, speed );
		self waittill_any( "movedone", "targetStopMoving" );
		self playSound( "amb_target_stop" );

		if ( isDefined( self.triggeroff ) )
			keepMoving = self.triggeroff;

		if ( keepMoving == false )
		{
			// IW4 breaks out of its switch here and loops straight back into a moveTo the
			// target already reached; wait for the trigger's release instead.
			self waittill( "targetStopMoving" );
			continue;
		}
		self moveTo( startPOS, speed );
		self waittill_any( "movedone", "targetStopMoving" );
		self playSound( "amb_target_stop" );
	}
}

rotateTarget( dir, deg, speed, pauseTime )
{
	self endon( "game_ended" );
	while ( 1 )
	{
		switch ( dir )
		{
			case 1:
				self rotateYaw( self.angles[ 2 ] + deg, speed );
				self playSound( "amb_target_rotate" );
				wait( pauseTime );
				self rotateYaw( self.angles[ 2 ] - deg, speed );
				self playSound( "amb_target_rotate" );
				wait( pauseTime );
				break;
			case 2:
				self rotateYaw( self.angles[ 2 ] - deg, speed );
				self playSound( "amb_target_rotate" );
				wait( pauseTime );
				self rotateYaw( self.angles[ 2 ] + deg, speed );
				self playSound( "amb_target_rotate" );
				wait( pauseTime );
				break;
			default:
				wait( pauseTime );
				break;
		}
	}
}
