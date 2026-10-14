// character\character_sas_woodland_support_a -- CoDOL, in IW4's character layout.

main()
{
	self setModel( "body_sp_sas_woodland_support_a" );
	codescripts\character::attachHead( "alias_sas_woodland_heads", xmodelalias\alias_sas_woodland_heads::main() );
	self.voice = "british";
}

precache()
{
	codescripts\character::precacheMounted( "body_sp_sas_woodland_support_a" );
	codescripts\character::precacheModelArray( xmodelalias\alias_sas_woodland_heads::main() );
}
