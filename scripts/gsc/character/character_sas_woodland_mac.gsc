// character\character_sas_woodland_mac -- CoDOL, in IW4's character layout.
// Captain MacMillan's look in the training recording: the woodland SAS boonie
// and his own head.

main()
{
	self setModel( "body_sp_sas_woodland_assault_a" );
	if ( codescripts\character::modelMounted( "head_sp_sas_woodland_mac" ) )
	{
		self attach( "head_sp_sas_woodland_mac", "", true );
		self.headModel = "head_sp_sas_woodland_mac";
	}
	self.voice = "british";
}

precache()
{
	codescripts\character::precacheMounted( "body_sp_sas_woodland_assault_a" );
	codescripts\character::precacheMounted( "head_sp_sas_woodland_mac" );
}
