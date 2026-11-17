// character\character_opf_militia_smg_aa_wht -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_militia_smg_aa_wht" );
	codescripts\character::attachHead( "alias_opforce_militia_heads_wht", xmodelalias\alias_opforce_militia_heads_wht::main() );
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_militia_smg_aa_wht" );
	codescripts\character::precacheModelArray( xmodelalias\alias_opforce_militia_heads_wht::main() );
}
