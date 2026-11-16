// character\character_opf_militia_assault_ac_blk -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_militia_assault_ac_blk" );
	codescripts\character::attachHead( "alias_opforce_militia_heads_blk", xmodelalias\alias_opforce_militia_heads_blk::main() );
	if ( isdefined( self.headModel ) && issubstr( self.headModel, "hat" ) && codescripts\character::modelMounted( "hat_militia_bb_blk" ) )
	{
		self.hatModel = "hat_militia_bb_blk";
		self attach( self.hatModel );
	}
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_militia_assault_ac_blk" );
	codescripts\character::precacheModelArray( xmodelalias\alias_opforce_militia_heads_blk::main() );
	codescripts\character::precacheMounted( "hat_militia_bb_blk" );
}
