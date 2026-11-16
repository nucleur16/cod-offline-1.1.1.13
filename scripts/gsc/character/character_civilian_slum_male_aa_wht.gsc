// character\character_civilian_slum_male_aa_wht -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_slum_civ_male_aa_wht" );
	codescripts\character::attachHead( "alias_civilian_slum_heads_wht", xmodelalias\alias_civilian_slum_heads_wht::main() );
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_slum_civ_male_aa_wht" );
	codescripts\character::precacheModelArray( xmodelalias\alias_civilian_slum_heads_wht::main() );
}
