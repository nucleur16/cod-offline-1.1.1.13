// character\character_civilian_slum_male_ab -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_slum_civ_male_ab" );
	codescripts\character::attachHead( "alias_civilian_slum_heads", xmodelalias\alias_civilian_slum_heads::main() );
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_slum_civ_male_ab" );
	codescripts\character::precacheModelArray( xmodelalias\alias_civilian_slum_heads::main() );
}
