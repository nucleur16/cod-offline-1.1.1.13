// character\character_codo_hero_harker -- CoDOL, in IW4's character layout.
// Colonel Harker.  His spawners' "model" key is body_vil_shepherd, which the
// training zone ships; IW4's head_vil_shepherd does not ship, and the one
// head the zone carries that no other actor uses is head_hero_russian_president,
// so that is his.

main()
{
	self setModel( "body_vil_shepherd" );
	if ( codescripts\character::modelMounted( "head_hero_russian_president" ) )
	{
		self attach( "head_hero_russian_president", "", true );
		self.headModel = "head_hero_russian_president";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_vil_shepherd" );
	codescripts\character::precacheMounted( "head_hero_russian_president" );
}
