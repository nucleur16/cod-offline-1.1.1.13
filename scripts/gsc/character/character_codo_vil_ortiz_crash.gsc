// character\character_codo_vil_ortiz_crash -- CoDOL, in IW4's character layout.
// Ortiz in Crash Site (coop_defense_mp_crash_tropical).

main()
{
	self setModel( "body_airport_assault_a_clean" );
	if ( codescripts\character::modelMounted( "head_militia_ba_blk" ) )
	{
		self attach( "head_militia_ba_blk", "", true );
		self.headModel = "head_militia_ba_blk";
	}
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_airport_assault_a_clean" );
	codescripts\character::precacheMounted( "head_militia_ba_blk" );
}
