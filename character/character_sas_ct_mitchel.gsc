// character\character_sas_ct_mitchel -- CoDOL, in IW4's character layout.
// A complete SAS counter-terrorist body (gas mask, no separate head).

main()
{
	self setModel( "body_complete_sp_sas_ct_mitchel" );
	self.voice = "british";
}

precache()
{
	codescripts\character::precacheMounted( "body_complete_sp_sas_ct_mitchel" );
}
