// character\character_so_dog -- the Spec Ops dog's look (IW5
// character\character_sp_german_sheperd_dog).  german_sheperd_dog ships in
// common_gametype_coop; the CoDOL 3.5.2.6 wolf (mp_fullbody_wolf_b, its
// actor_enemy_wolf_so spawners) when a survival zone mounts it and the
// survival layer asks for it (level.survival_dog_wolf).

main()
{
	model = "german_sheperd_dog";
	if ( isDefined( level.survival_dog_wolf ) && level.survival_dog_wolf && codescripts\character::modelMounted( "mp_fullbody_wolf_b" ) )
		model = "mp_fullbody_wolf_b";
	self setModel( model );
	self.voice = "american";
	self.skeleton = "base";
}

precache()
{
	precacheModel( "german_sheperd_dog" );
	codescripts\character::precacheMounted( "mp_fullbody_wolf_b" );
	precacheItem( bite_weapon() );
	precacheShellshock( "dog_bite" );
}

bite_weapon()
{
	return "dog_bite";
}
