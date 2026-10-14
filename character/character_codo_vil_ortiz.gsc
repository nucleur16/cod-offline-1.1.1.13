// character\character_codo_vil_ortiz -- CoDOL, in IW4's character layout.
// Ortiz (coop_take_mp_fav_tropical): the spawner's "model" key, and the
// militia head the mission zone ships.

main()
{
	self setModel( "body_airport_assault_a" );
	if ( codescripts\character::modelMounted( "head_militia_ba_blk" ) )
	{
		self attach( "head_militia_ba_blk", "", true );
		self.headModel = "head_militia_ba_blk";
	}
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_airport_assault_a" );
	codescripts\character::precacheMounted( "head_militia_ba_blk" );
}
