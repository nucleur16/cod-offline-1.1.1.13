// character\character_codo_complete_tf_tan -- CoDOL, in IW4's character layout.
// The tan Task Force uniform and cap (Sgt. Newcastle in the recording).

main()
{
	self setModel( "body_complete_tf_tan" );
	if ( codescripts\character::modelMounted( "head_complete_tf_tan" ) )
	{
		self attach( "head_complete_tf_tan", "", true );
		self.headModel = "head_complete_tf_tan";
	}
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_complete_tf_tan" );
	codescripts\character::precacheMounted( "head_complete_tf_tan" );
}
