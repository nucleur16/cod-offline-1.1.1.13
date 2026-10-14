// character\character_codo_tf_codotshirt -- CoDOL, in IW4's character layout.

main()
{
	self setModel( "body_codotshirt_tf_m" );
	if ( codescripts\character::modelMounted( "hea_hjelmaa_tf_codotshirt" ) )
	{
		self attach( "hea_hjelmaa_tf_codotshirt", "", true );
		self.headModel = "hea_hjelmaa_tf_codotshirt";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_codotshirt_tf_m" );
	codescripts\character::precacheMounted( "hea_hjelmaa_tf_codotshirt" );
}
