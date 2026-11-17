// character\character_codo_tf_uniform -- CoDOL, in IW4's character layout.

main()
{
	self setModel( "body_uniform_tf_m" );
	if ( codescripts\character::modelMounted( "hea_veiga_tf_uniform" ) )
	{
		self attach( "hea_veiga_tf_uniform", "", true );
		self.headModel = "hea_veiga_tf_uniform";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_uniform_tf_m" );
	codescripts\character::precacheMounted( "hea_veiga_tf_uniform" );
}
