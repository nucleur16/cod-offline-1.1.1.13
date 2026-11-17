// character\character_codo_tf_bdurolled -- CoDOL, in IW4's character layout.

main()
{
	self setModel( "body_bdurolled_tf_m" );
	if ( codescripts\character::modelMounted( "hea_hernandez_tf_bdurolled" ) )
	{
		self attach( "hea_hernandez_tf_bdurolled", "", true );
		self.headModel = "hea_hernandez_tf_bdurolled";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_bdurolled_tf_m" );
	codescripts\character::precacheMounted( "hea_hernandez_tf_bdurolled" );
}
