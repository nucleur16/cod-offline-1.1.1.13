// character\character_sp_juggernaut_russian -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_complete_sp_juggernaut" );
	self.voice = "russian";
}

precache()
{
	codescripts\character::precacheMounted( "body_complete_sp_juggernaut" );
}
