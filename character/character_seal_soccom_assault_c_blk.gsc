// character\character_seal_soccom_assault_c_blk -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_seal_soccom_assault_c_blk" );
	codescripts\character::attachHead( "alias_seal_soccom_heads_blk", xmodelalias\alias_seal_soccom_heads_blk::main() );
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_seal_soccom_assault_c_blk" );
	codescripts\character::precacheModelArray( xmodelalias\alias_seal_soccom_heads_blk::main() );
}
