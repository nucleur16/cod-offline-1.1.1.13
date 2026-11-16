// character\character_us_army_smg_b -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.
// IW4 sets mp_body_us_army_smg_b and viewhands_us_army; C1 ships the body as
// body_us_army_smg_b (coop_defense_mp_crash_tropical) and an actor has no
// viewmodel.

main()
{
	self setModel( "body_us_army_smg_b" );
	codescripts\character::attachHead( "alias_us_army_heads", xmodelalias\alias_us_army_heads::main() );
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_us_army_smg_b" );
	codescripts\character::precacheModelArray( xmodelalias\alias_us_army_heads::main() );
}
