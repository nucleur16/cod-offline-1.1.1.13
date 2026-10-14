// character\character_hero_seal_soccom_ghost -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_hero_ghost_socom" );
	if ( codescripts\character::modelMounted( "head_hero_ghost_soccom" ) )
	{
		self attach( "head_hero_ghost_soccom", "", true );
		self.headModel = "head_hero_ghost_soccom";
	}
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_hero_ghost_socom" );
	codescripts\character::precacheMounted( "head_hero_ghost_soccom" );
}
