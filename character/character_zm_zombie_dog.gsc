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
	precacheItem( bite_weapon() );
	precacheShellshock( "dog_bite" );
}

// The bite WeaponDef the native melee() needs (codol_actor_lifecycle.cpp
// ActorMelee refuses an actor without a real aiweapon definition).
bite_weapon()
{
	return "dog_bite_zm";
}
