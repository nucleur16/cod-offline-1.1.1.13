// character\character_codo_vil_vazquez -- CoDOL, in IW4's character layout.
// Vazquez (coop_take_mp_estate_tropical): the spawner's "model" key and the
// one airport head the zone ships.

main()
{
	self setModel( "body_militia_smg_aa_wht" );
	if ( codescripts\character::modelMounted( "head_airport_a" ) )
	{
		self attach( "head_airport_a", "", true );
		self.headModel = "head_airport_a";
	}
	self.voice = "portuguese";
}

precache()
{
	codescripts\character::precacheMounted( "body_militia_smg_aa_wht" );
	codescripts\character::precacheMounted( "head_airport_a" );
}
