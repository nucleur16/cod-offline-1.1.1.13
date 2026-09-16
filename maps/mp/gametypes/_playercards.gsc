#include common_scripts\utility;
#include maps\mp\_utility;

init()
{
	level thread onPlayerConnect();
}

onPlayerConnect()
{
	for (;;)
	{
		level waittill( "connected", player );
		if ( player.isbot )
			continue;

		icon = player selectedCardRow( "cardIcon", "pci", "mp/callsignIcon.csv" );
		if ( isDefined( icon ) )
			player SetCardIcon( icon );
		title = player selectedCardRow( "cardTitle", "pct", "mp/callsignTitle.csv" );
		if ( isDefined( title ) )
			player SetCardTitle( title );
		background = player selectedCardRow( "cardBackground", "pcb", "mp/callsignBackground.csv" );
		if ( isDefined( background ) )
			player SetCardBackground( background );

		// This field has no selected marketplace family in the shipped table.
		// Preserve the bounded raw nameplate value, in its actual fourth field.
		nameplate = player maps\mp\gametypes\_persistence::statGet( "cardNameplate" );
		if ( isDefined( nameplate ) && nameplate >= 0 && nameplate < 1024 )
			player SetCardNameplate( nameplate );
	}
}

selectedCardRow( member, family, callsignTable )
{
	item = self maps\mp\gametypes\_persistence::statGet( member );
	if ( !isDefined( item ) || item <= 0 )
		return undefined;
	itemName = tableLookup( "mp/coMarketplaceItems.csv", 0, item, 1 );
	itemFamily = tableLookup( "mp/coMarketplaceItems.csv", 0, item, 2 );
	if ( itemName == "" || itemFamily != family )
	{
		println( "CoDOL invalid player card item: ", member, " ", item );
		return undefined;
	}
	row = tableLookupRowNum( callsignTable, 0, itemName );
	if ( row < 0 || row >= 1024 )
	{
		println( "CoDOL missing player card row: ", callsignTable, " ", itemName );
		return undefined;
	}
	return row;
}
