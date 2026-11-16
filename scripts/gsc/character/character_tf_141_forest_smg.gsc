// character\character_tf_141_forest_smg -- CoDOL (IW4 mp_character_tf_141_forest_smg), in IW4's character layout.

main()
{
	self setModel( "body_forest_tf141_smg" );
	codescripts\character::attachHead( "alias_tf141_heads_forest", xmodelalias\alias_tf141_heads_forest::main() );
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_forest_tf141_smg" );
	codescripts\character::precacheModelArray( xmodelalias\alias_tf141_heads_forest::main() );
}
