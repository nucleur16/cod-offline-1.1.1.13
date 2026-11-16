// character\character_tf_141_forest_shotgun -- ported from IW4 SP (Infinity Ward, Modern Warfare 2),
// adapted to the models C1 ships.

main()
{
	self setModel( "body_forest_tf141_shotgun" );
	codescripts\character::attachHead( "alias_tf141_heads_forest", xmodelalias\alias_tf141_heads_forest::main() );
	self.voice = "taskforce";
}

precache()
{
	codescripts\character::precacheMounted( "body_forest_tf141_shotgun" );
	codescripts\character::precacheModelArray( xmodelalias\alias_tf141_heads_forest::main() );
}
