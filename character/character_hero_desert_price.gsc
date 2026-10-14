// character\character_hero_desert_price -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_desert_tf141_assault_a" );
	if ( codescripts\character::modelMounted( "head_hero_price_desert" ) )
	{
		self attach( "head_hero_price_desert", "", true );
		self.headModel = "head_hero_price_desert";
	}
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_desert_tf141_assault_a" );
	codescripts\character::precacheMounted( "head_hero_price_desert" );
}
