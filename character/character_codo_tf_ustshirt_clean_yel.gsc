// character\character_codo_tf_ustshirt_clean_yel -- CoDOL, in IW4's character layout.

main()
{
	self setModel( "body_ustshirt_tf_m_clean_yel" );
	codescripts\character::attachHead( "alias_codo_tf_heads_clean", xmodelalias\alias_codo_tf_heads_clean::main() );
	self.voice = "american";
}

precache()
{
	codescripts\character::precacheMounted( "body_ustshirt_tf_m_clean_yel" );
	codescripts\character::precacheModelArray( xmodelalias\alias_codo_tf_heads_clean::main() );
}
