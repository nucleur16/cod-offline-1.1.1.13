// character\character_op_militia_sniper -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_op_miltia_sniper" );
	if ( codescripts\character::modelMounted( "head_op_militia_sniper" ) )
	{
		self attach( "head_op_militia_sniper", "", true );
		self.headModel = "head_op_militia_sniper";
	}
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_op_miltia_sniper" );
	codescripts\character::precacheMounted( "head_op_militia_sniper" );
}
