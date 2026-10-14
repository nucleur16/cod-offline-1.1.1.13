// character\character_codo_complete_lw -- CoDOL, in IW4's character layout.
// The set's Radiant model (the lwset spawners' "model" key).

main()
{
	self setModel( "body_complete_sp_lw" );
	if ( codescripts\character::modelMounted( "head_complete_sp_lw" ) )
	{
		self attach( "head_complete_sp_lw", "", true );
		self.headModel = "head_complete_sp_lw";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_complete_sp_lw" );
	codescripts\character::precacheMounted( "head_complete_sp_lw" );
}
