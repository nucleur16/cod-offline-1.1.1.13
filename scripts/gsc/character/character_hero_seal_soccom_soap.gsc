// character\character_hero_seal_soccom_soap -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_hero_soap_socom" );
	if ( codescripts\character::modelMounted( "head_hero_soap_soccom_a" ) )
	{
		self attach( "head_hero_soap_soccom_a", "", true );
		self.headModel = "head_hero_soap_soccom_a";
	}
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_hero_soap_socom" );
	codescripts\character::precacheMounted( "head_hero_soap_soccom_a" );
}
