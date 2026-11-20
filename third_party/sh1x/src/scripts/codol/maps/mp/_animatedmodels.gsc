// sh1x server native adaptation of IW4 _animatedmodels.gsc for Call of Duty Online.
//
// An entity tagged "animated_model" chooses its animation from
// level.anim_prop_models[ self.model ], a table the level designer populates
// from the map's own animation registration scripts (maps/animated_models/*).
// Those scripts ship inside the map's zone, not with the gametype library, so
// on a CoDOL map whose registrations we do not have the lookup is undefined and
// IW4's unguarded GetArrayKeys call raises a fatal script error before the
// match ever starts. mp_bog_sh does exactly that.
//
// A server cannot invent an animation for a model it has no data for, so the
// only correct behaviour is to leave that prop unanimated and carry on. Each
// unregistered model is reported once in developer mode, so the missing
// registration stays visible rather than being silently absorbed.
#include common_scripts\utility;

#using_animtree( "animated_props" );

main()
{
	if ( !isdefined( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model_keys = GetArrayKeys( level.anim_prop_models );
	foreach ( model_key in model_keys )
	{
		anim_keys = GetArrayKeys( level.anim_prop_models[ model_key ] );
		foreach ( anim_key in anim_keys )
			PrecacheMpAnim( level.anim_prop_models[ model_key ][ anim_key ] );
	}

	// wait until the end of the frame so that maps can init their trees
	// in their _anim instead of only above _load
	waittillframeend;

	level.init_animatedmodels = [];
	level.codol_unregistered_anim_models = [];

	animated_models = getentarray( "animated_model", "targetname" );
	process = ::animateModel;
	foreach ( animated_model in animated_models )
	{
		animated_model thread [[ process ]]();
		// Large maps contain enough animated props to exhaust one VM slice.
		// C1 can finish this asynchronously; one launch per frame preserves all
		// entities without turning the VM's runaway-loop warning into restarts.
		waittillframeend;
	}

	level.init_animatedmodels = undefined;
}

animateModel()
{
	if ( isdefined( self.animation ) )
	{
		animation = self.animation;
	}
	else
	{
		if ( !isdefined( self.model ) || !isdefined( level.anim_prop_models[ self.model ] ) )
		{
			codol_reportUnregisteredAnimModel();
			return;
		}

		keys = GetArrayKeys( level.anim_prop_models[ self.model ] );
		if ( !isdefined( keys ) || keys.size <= 0 )
		{
			codol_reportUnregisteredAnimModel();
			return;
		}

		animation = level.anim_prop_models[ self.model ][ keys[ RandomInt( keys.size ) ] ];
	}

	if ( !isdefined( animation ) )
	{
		codol_reportUnregisteredAnimModel();
		return;
	}

	self ScriptModelPlayAnim( animation );
	self willNeverChange();
}

// One line per distinct model, so a map missing its registrations is obvious
// without one line per prop.
codol_reportUnregisteredAnimModel()
{
	if ( !isdefined( self.model ) )
		return;

	if ( isdefined( level.codol_unregistered_anim_models[ self.model ] ) )
		return;

	level.codol_unregistered_anim_models[ self.model ] = true;
	println( "CoDOL animated model: no animation registered for \"" + self.model + "\"; prop left static" );
}
