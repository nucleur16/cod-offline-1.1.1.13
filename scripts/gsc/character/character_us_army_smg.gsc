// character\character_us_army_smg -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_us_army_smg" );
	codescripts\character::attachHead( "alias_us_army_heads", xmodelalias\alias_us_army_heads::main() );
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_us_army_smg" );
	codescripts\character::precacheModelArray( xmodelalias\alias_us_army_heads::main() );
}
