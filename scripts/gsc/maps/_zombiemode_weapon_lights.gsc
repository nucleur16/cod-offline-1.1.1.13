// Authored T5 CSC weapon-light behavior, transported by C1's own viewmodel
// FX events. References/provenance are retained in the feature evidence bundle.
// This module never substitutes projectile or damage FX for a weapon light.
init()
{
    level.zm_light_fx = [];
    level.zm_light_tesla = codol_isWeaponMounted( "tesla_gun_zm" );
    level.zm_light_thunder = codol_isWeaponMounted( "thundergun_zm" );
    if ( level.zm_light_tesla )
    {
        level.zm_light_fx[ "tesla" ] = loadFx( "maps/zombie/fx_zombie_tesla_tube_view" );
        level.zm_light_fx[ "tesla2" ] = loadFx( "maps/zombie/fx_zombie_tesla_tube_view2" );
        level.zm_light_fx[ "tesla3" ] = loadFx( "maps/zombie/fx_zombie_tesla_tube_view3" );
        level.zm_light_fx[ "tesla_ug" ] = loadFx( "maps/zombie/fx_zombie_tesla_tube_view_ug" );
        level.zm_light_fx[ "tesla2_ug" ] = loadFx( "maps/zombie/fx_zombie_tesla_tube_view2_ug" );
        level.zm_light_fx[ "tesla3_ug" ] = loadFx( "maps/zombie/fx_zombie_tesla_tube_view3_ug" );
        level.zm_light_fx[ "rail" ] = loadFx( "maps/zombie/fx_zombie_tesla_rail_view" );
        level.zm_light_fx[ "rail_ug" ] = loadFx( "maps/zombie/fx_zombie_tesla_rail_view_ug" );
    }
    if ( level.zm_light_thunder )
    {
        for ( i = 1; i <= 3; i++ )
            level.zm_light_fx[ "cell" + i ] = loadFx( "weapon/thunder_gun/fx_thundergun_power_cell_view" + i );
        level.zm_light_fx[ "steam" ] = loadFx( "weapon/thunder_gun/fx_thundergun_steam_view" );
    }
    level thread watch_players();
}

watch_players()
{
    for ( ;; )
    {
        players = getPlayers();
        for ( i = 0; i < players.size; i++ )
        {
            if ( !isDefined( players[i].zm_weapon_light_thread ) )
            {
                players[i].zm_weapon_light_thread = true;
                players[i] thread watch_weapon();
            }
        }
        wait( 0.1 );
    }
}

// Exactly the T5 clip-dependent tube selection; upgraded clip starts at6.
tesla_key( ammo, upgraded )
{
    key = "tesla";
    if ( upgraded )
    {
        if ( ammo == 3 || ammo == 4 ) key = "tesla2";
        else if ( ammo == 1 || ammo == 2 ) key = "tesla3";
        return key + "_ug";
    }
    if ( ammo == 1 ) return "tesla3";
    if ( ammo == 2 ) return "tesla2";
    return key;
}

watch_weapon()
{
    self endon( "disconnect" );
    old_weapon = "";
    tube = "";
    count = 0;
    old_ammo = -1;
    refresh = 0;
    rail = getTime() + randomIntRange( 8000, 12000 );
    for ( ;; )
    {
        wait( 0.1 );
        weapon = self getCurrentWeapon();
        ready = isAlive( self ) && self codol_viewmodelFXReady();
        tesla = level.zm_light_tesla && ( weapon == "tesla_gun_zm" || weapon == "tesla_gun_upgraded_zm" );
        thunder = level.zm_light_thunder && ( weapon == "thundergun_zm" || weapon == "thundergun_upgraded_zm" );
        if ( !ready || ( !tesla && !thunder ) || old_weapon != weapon )
        {
            if ( tube != "" ) self stopViewModelFXOnTag( level.zm_light_fx[tube], "tag_brass" );
            if ( count ) self stop_cells( count );
            tube = "";
            count = 0;
            old_ammo = -1;
            refresh = 0;
        }
        old_weapon = weapon;
        if ( !ready || ( !tesla && !thunder ) ) continue;
        ammo = self getWeaponAmmoClip( weapon );
        if ( tesla )
        {
            if ( ammo <= 0 )
            {
                if ( tube != "" ) self stopViewModelFXOnTag( level.zm_light_fx[tube], "tag_brass" );
                tube = "";
                continue;
            }
            key = tesla_key( ammo, weapon == "tesla_gun_upgraded_zm" );
            if ( tube != "" && tube != key ) self stopViewModelFXOnTag( level.zm_light_fx[tube], "tag_brass" );
            self playViewModelFXOnTag( level.zm_light_fx[key], "tag_brass" );
            tube = key;
            if ( getTime() >= rail )
            {
                rail = getTime() + randomIntRange( 8000, 12000 );
                if ( self playerADS() <= 0 )
                {
                    key = "rail";
                    if ( weapon == "tesla_gun_upgraded_zm" ) key = "rail_ug";
                    self playViewModelFXOnTag( level.zm_light_fx[key], "tag_flash" );
                }
            }
        }
        else
        {
            current = ammo;
            if ( current > 3 ) current = 3;
            if ( current < 0 ) current = 0;
            if ( old_ammo > 0 && ammo < old_ammo )
                for ( i = 1; i <= 3; i++ ) self playViewModelFXOnTag( level.zm_light_fx["steam"], "tag_steam" + i );
            if ( count != current )
            {
                if ( count ) self stop_cells( count );
                count = current;
                refresh = 0;
            }
            if ( count && getTime() >= refresh )
            {
                for ( i = 1; i <= count; i++ ) self playViewModelFXOnTag( level.zm_light_fx["cell" + i], "tag_bulb" + i );
                refresh = getTime() + 3000;
            }
        }
        old_ammo = ammo;
    }
}
stop_cells( count )
{
    for ( i = 1; i <= count; i++ ) self stopViewModelFXOnTag( level.zm_light_fx["cell" + i], "tag_bulb" + i );
}
