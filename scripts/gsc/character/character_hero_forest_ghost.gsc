// character\character_hero_forest_ghost -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_forest_tf141_ghost" );
	if ( codescripts\character::modelMounted( "head_hero_ghost_forest" ) )
	{
		self attach( "head_hero_ghost_forest", "", true );
		self.headModel = "head_hero_ghost_forest";
	}
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_forest_tf141_ghost" );
	codescripts\character::precacheMounted( "head_hero_ghost_forest" );
}
