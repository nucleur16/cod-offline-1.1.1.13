// character\character_shadow_co_lmg -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_shadow_co_lmg" );
	codescripts\character::attachHead( "alias_shad_co_heads", xmodelalias\alias_shad_co_heads::main() );
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_shadow_co_lmg" );
	codescripts\character::precacheModelArray( xmodelalias\alias_shad_co_heads::main() );
}
