// character\character_us_army_dmg_a_blk -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_us_army_dmg_a_blk" );
	codescripts\character::attachHead( "alias_us_army_heads_dmg_blk", xmodelalias\alias_us_army_heads_dmg_blk::main() );
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_us_army_dmg_a_blk" );
	codescripts\character::precacheModelArray( xmodelalias\alias_us_army_heads_dmg_blk::main() );
}
