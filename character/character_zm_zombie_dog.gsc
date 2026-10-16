// zm hellhound body.
//
// T5 character\character_sp_zombie_dog.gsc sets "zombie_wolf"; CoDOL ships the
// same model name in all three zm MAP zones (zm_asylum_sh, zm_prototype_sh,
// zm_zombie_training -- docs/zone_assets/ALL_xmodel.txt) and in no common
// zone, exactly like the humanoid c_zom_* bodies (character_zm_zombie.gsc).

main()
{
	self setModel( "zombie_wolf" );
	self.voice = "american";
	self.skeleton = "base";
}

precache()
{
	precacheModel( "zombie_wolf" );
	precacheItem( "dog_bite_zm" );
	precacheItem( "zombie_melee" );
	precacheShellshock( "dog_bite" );
}

// The bite WeaponDef the native melee() needs (codol_actor_lifecycle.cpp
// ActorMelee refuses an actor without a real aiweapon definition).
//
// dog_bite_zm ships in the zm_asylum_sh, zm_prototype_sh and
// zm_zombie_training map zones only.  MEASURED 2026-10-15 on a zm_sumpf_sh
// probe: its zone has none, and the first bite was a fatal "actor melee
// requires its real aiweapon definition".  zombie_melee (common_gametype_zm,
// mounted for every zm map) is the fallback, as in
// _zombiemode_spawner::resolve_melee_weapon; the bite's damage is the
// hound's meleeDamage (40, T5 dog_init) either way, applied by
// _zombiemode::Callback_PlayerDamage.
bite_weapon()
{
	if ( isDefined( level.zm_dog_bite_weapon ) )
		return level.zm_dog_bite_weapon;

	candidates = [];
	candidates[ 0 ] = "dog_bite_zm";
	candidates[ 1 ] = "zombie_melee";
	for ( i = 0; i < candidates.size; i++ )
	{
		if ( getWeaponIndexFromName( candidates[ i ] ) > 0 )
		{
			level.zm_dog_bite_weapon = candidates[ i ];
			return candidates[ i ];
		}
	}
	return candidates[ 0 ];
}
