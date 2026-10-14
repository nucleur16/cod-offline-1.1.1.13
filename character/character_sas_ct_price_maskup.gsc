// character\character_sas_ct_price_maskup -- CoDOL, in IW4's character layout.
// Captain Price in the black kit, mask up (coop_spec1_mp_killhouse_sh).

main()
{
	self setModel( "body_complete_sp_sas_ct_price_maskup" );
	self.voice = "british";
}

precache()
{
	codescripts\character::precacheMounted( "body_complete_sp_sas_ct_price_maskup" );
}
