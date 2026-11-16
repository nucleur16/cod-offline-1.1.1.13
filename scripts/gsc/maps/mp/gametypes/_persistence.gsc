#include maps\mp\_utility;

init()
{
	level.persistentDataInfo = [];

	maps\mp\gametypes\_class::init();
	maps\mp\gametypes\_rank::init();
	maps\mp\gametypes\_missions::init();
	maps\mp\gametypes\_playercards::init();

	level thread updateBufferedStats();
}


initBufferedStats()
{
	if ( self.isbot )
	{
		// Bots own match-local counters, not a lobby account/profile.
		self.bufferedStats = [];
		foreach ( name in strTok( "totalShots accuracy misses hits timePlayedAllies timePlayedOpfor timePlayedOther timePlayedTotal", " " ) )
			self.bufferedStats[name] = 0;
		self.bufferedChildStats = [];
		self.bufferedChildStats["round"] = [];
		self.bufferedChildStats["round"]["timePlayed"] = 0;
		if ( !isDefined( self.codolBotStats ) )
			self.codolBotStats = [];
		if ( !isDefined( self.codolBotChildStats ) )
			self.codolBotChildStats = [];
		return;
	}
	self.bufferedStats = [];
	self.bufferedStats[ "totalShots" ] = self getPlayerData( "totalShots" );	
	self.bufferedStats[ "accuracy" ] = self getPlayerData( "accuracy" );	
	self.bufferedStats[ "misses" ] = self getPlayerData( "misses" );	
	self.bufferedStats[ "hits" ] = self getPlayerData( "hits" );	
	self.bufferedStats[ "timePlayedAllies" ] = self getPlayerData( "timePlayedAllies" );	
	self.bufferedStats[ "timePlayedOpfor" ] = self getPlayerData( "timePlayedOpfor" );	
	self.bufferedStats[ "timePlayedOther" ] = self getPlayerData( "timePlayedOther" );	
	self.bufferedStats[ "timePlayedTotal" ] = self getPlayerData( "timePlayedTotal" );	
	
	self.bufferedChildStats = [];
	self.bufferedChildStats[ "round" ] = [];
	self.bufferedChildStats[ "round" ][ "timePlayed" ] = self getPlayerData( "round", "timePlayed" );
}



// ==========================================
// Script persistent data functions
// These are made for convenience, so persistent data can be tracked by strings.
// They make use of code functions which are prototyped below.

/*
=============
statGet

Returns the value of the named stat
=============
*/
statGet( dataName )
{
	if ( self.isbot )
	{
		if ( !isDefined( self.codolBotStats ) )
			self.codolBotStats = [];
		if ( !isDefined( self.codolBotStats[dataName] ) )
			self.codolBotStats[dataName] = 0;
		return self.codolBotStats[dataName];
	}
	assert( !isDefined( self.bufferedStats[ dataName ] ) ); // should use statGetBuffered consistently with statSetBuffered
	return self GetPlayerData( dataName );
}

/*
=============
statSet

Sets the value of the named stat
=============
*/
statSet( dataName, value )
{
	if ( self.isbot )
	{
		if ( !isDefined( self.codolBotStats ) )
			self.codolBotStats = [];
		self.codolBotStats[dataName] = value;
		return;
	}
	assert( !isDefined( self.bufferedStats[ dataName ] ) ); // should use statGetBuffered consistently with statSetBuffered
	
	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	self SetPlayerData( dataName, value );
}

/*
=============
statAdd

Adds the passed value to the value of the named stat
=============
*/
statAdd( dataName, value )
{
	if ( self.isbot )
	{
		current = self statGet( dataName );
		self statSet( dataName, current + value );
		return;
	}
	assert( !isDefined( self.bufferedStats[ dataName ] ) ); // should use statGetBuffered consistently with statSetBuffered
	
	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	curValue = self GetPlayerData( dataName );
	self SetPlayerData( dataName, value + curValue );
}


statGetChild( parent, child )
{
	if ( self.isbot )
	{
		if ( !isDefined( self.codolBotChildStats ) )
			self.codolBotChildStats = [];
		if ( !isDefined( self.codolBotChildStats[parent] ) )
			self.codolBotChildStats[parent] = [];
		if ( !isDefined( self.codolBotChildStats[parent][child] ) )
			self.codolBotChildStats[parent][child] = 0;
		return self.codolBotChildStats[parent][child];
	}
	return self GetPlayerData( parent, child );
}


statSetChild( parent, child, value )
{
	if ( self.isbot )
	{
		if ( !isDefined( self.codolBotChildStats ) )
			self.codolBotChildStats = [];
		if ( !isDefined( self.codolBotChildStats[parent] ) )
			self.codolBotChildStats[parent] = [];
		self.codolBotChildStats[parent][child] = value;
		return;
	}
	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	self SetPlayerData( parent, child, value );
}


statAddChild( parent, child, value )
{
	if ( self.isbot )
	{
		current = self statGetChild( parent, child );
		self statSetChild( parent, child, current + value );
		return;
	}
	assert( isDefined( self.bufferedChildStats[ parent ][ child ] ) );

	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	curValue = self GetPlayerData( parent, child );
	self SetPlayerData( parent, child, curValue + value );
}


statGetChildBuffered( parent, child )
{
	assert( isDefined( self.bufferedChildStats[ parent ][ child ] ) );
	
	return self.bufferedChildStats[ parent ][ child ];
}


statSetChildBuffered( parent, child, value )
{
	assert( isDefined( self.bufferedChildStats[ parent ][ child ] ) );
	
	if ( !self.isbot && !self rankingEnabled() )
		return;

	self.bufferedChildStats[ parent ][ child ] = value;
}


statAddChildBuffered( parent, child, value )
{
	assert( isDefined( self.bufferedChildStats[ parent ][ child ] ) );

	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	curValue = statGetChildBuffered( parent, child );
	statSetChildBuffered( parent, child, curValue + value );
}


/*
=============
statGetBuffered

Returns the value of the named stat
=============
*/
statGetBuffered( dataName )
{
	assert( isDefined( self.bufferedStats[ dataName ] ) );
	
	return self.bufferedStats[ dataName ];
}

/*
=============
statSet

Sets the value of the named stat
=============
*/
statSetBuffered( dataName, value )
{
	assert( isDefined( self.bufferedStats[ dataName ] ) );

	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	self.bufferedStats[ dataName ] = value;
}

/*
=============
statAdd

Adds the passed value to the value of the named stat
=============
*/
statAddBuffered( dataName, value )
{	
	assert( isDefined( self.bufferedStats[ dataName ] ) );

	if ( !self.isbot && !self rankingEnabled() )
		return;
	
	curValue = statGetBuffered( dataName );
	statSetBuffered( dataName, curValue + value );
}


updateBufferedStats()
{
	// give the first player time to connect
	wait ( 0.15 );
	
	nextToUpdate = 0;
	while ( !level.gameEnded )
	{
		nextToUpdate++;
		if ( nextToUpdate >= level.players.size )
			nextToUpdate = 0;

		if ( isDefined( level.players[nextToUpdate] ) )
			level.players[nextToUpdate] writeBufferedStats();

		wait ( 2.0 );
	}
	
	foreach ( player in level.players )
		player writeBufferedStats();	
}


writeBufferedStats()
{
	// Match-local bot counters have no account upload destination.
	if ( self.isbot )
		return;
	foreach ( statName, statVal in self.bufferedStats )
	{
		self setPlayerData( statName, statVal );
	}

	foreach ( statName, statVal in self.bufferedChildStats )
	{
		foreach ( childStatName, childStatVal in statVal )
			self setPlayerData( statName, childStatName, childStatVal );
	}
}
