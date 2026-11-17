// character\character_codo_tf_ustshirt -- CoDOL, in IW4's character layout.

main()
{
	self setModel( "body_ustshirt_tf_m" );
	if ( codescripts\character::modelMounted( "hea_hernandez_tf_clean" ) )
	{
		self attach( "hea_hernandez_tf_clean", "", true );
		self.headModel = "hea_hernandez_tf_clean";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_ustshirt_tf_m" );
	codescripts\character::precacheMounted( "hea_hernandez_tf_clean" );
}
