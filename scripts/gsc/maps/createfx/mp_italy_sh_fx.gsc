//_createfx generated. Do not touch!!
#include common_scripts\utility;
#include common_scripts\_createfx;

main()
{
	// CreateFX entities placed: 67

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 1936, 1086, 1095 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emi_water_drip_lp";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( -197, 1531, 1285 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_water_lap_gurg_lp";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( -196, 1530, 1284 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_water_lap_gurg_lt_lp";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 1833, 453, 1006 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emi_fountain_gurgle_spray_04 ";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 1835, 455, 1009 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emi_water_fountain_thin_narrow_spray";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( -668, 1695, 1302 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emi_fountain_gurgle_spray_04 ";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( -667, 1696, 1303 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emi_water_fountain_thin_narrow_spray";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 2209, 180, 1023 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_water_stream_lap";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 2272, -495, 1002 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_water_stream_lap";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 1913, -1769, 953 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_ocean_waves_open";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 1046, -1838, 912 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_ocean_waves_open";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 78, -1670, 889 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_ocean_waves_open";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( -1262, -1760, 843 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_ocean_waves_open";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( 2556, -1475, 988 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_ocean_waves_open";   

	ent = createLoopSound();
    ent.v[ "origin" ] = ( -1802, -1016, 903 );
    ent.v[ "angles" ] = ( 270, 0, 0 );    
    ent.v[ "soundalias" ] = "emt_ocean_waves_open";   

    ent = createIntervalSound();
    ent.v[ "origin" ] = ( 1366, -1251, 1068 );
    ent.v[ "angles" ] = ( 270, 0, 0 );
    ent.v[ "delay_min" ] = 13;
    ent.v[ "delay_max" ] = 17;
    ent.v[ "delay_initial" ] = 0;
    ent.v[ "soundalias" ] = "emt_tree_rustle_mid";

    ent = createIntervalSound();
    ent.v[ "origin" ] = ( 911, -583, 1061 );
    ent.v[ "angles" ] = ( 270, 0, 0 );
    ent.v[ "delay_min" ] = 13;
    ent.v[ "delay_max" ] = 17;
    ent.v[ "delay_initial" ] = 0;
    ent.v[ "soundalias" ] = "emt_tree_rustle_mid";

    ent = createIntervalSound();
    ent.v[ "origin" ] = ( 2137, -412, 1226 );
    ent.v[ "angles" ] = ( 270, 0, 0 );
    ent.v[ "delay_min" ] = 13;
    ent.v[ "delay_max" ] = 17;
    ent.v[ "delay_initial" ] = 0;
    ent.v[ "soundalias" ] = "emt_tree_rustle_mid";

    ent = createIntervalSound();
    ent.v[ "origin" ] = ( 1583, 304, 1316 );
    ent.v[ "angles" ] = ( 270, 0, 0 );
    ent.v[ "delay_min" ] = 13;
    ent.v[ "delay_max" ] = 17;
    ent.v[ "delay_initial" ] = 0;
    ent.v[ "soundalias" ] = "emt_tree_rustle_mid";

    ent = createIntervalSound();
    ent.v[ "origin" ] = ( -1022, -1152, 963 );
    ent.v[ "angles" ] = ( 270, 0, 0 );
    ent.v[ "delay_min" ] = 13;
    ent.v[ "delay_max" ] = 17;
    ent.v[ "delay_initial" ] = 0;
    ent.v[ "soundalias" ] = "emt_tree_rustle_mid";

	ent = createOneshotEffect( "water_fountain_splash_impact" );
	ent.v[ "origin" ] = ( -680.248, 1708.45, 1305 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_impact";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_impact" );
	ent.v[ "origin" ] = ( -664.43, 1699.62, 1302.8 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_impact";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_impact" );
	ent.v[ "origin" ] = ( -653.568, 1705.33, 1302.96 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_impact";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_sm" );
	ent.v[ "origin" ] = ( -670.483, 1676.67, 1362.68 );
	ent.v[ "angles" ] = ( 297.984, 261.471, 93.534 );
	ent.v[ "fxid" ] = "water_fountain_splash_sm";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_ring" );
	ent.v[ "origin" ] = ( -189.82, 1536.44, 1279.68 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_ring";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_ring_sm" );
	ent.v[ "origin" ] = ( -189.82, 1536.44, 1317 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_ring_sm";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_impact" );
	ent.v[ "origin" ] = ( 1833.61, 431.25, 999.815 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_impact";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_impact" );
	ent.v[ "origin" ] = ( 1848.41, 431.32, 1000.82 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_impact";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_impact" );
	ent.v[ "origin" ] = ( 1818.19, 442.35, 999.815 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_impact";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water_fountain_splash_sm" );
	ent.v[ "origin" ] = ( 1840.24, 461.846, 1086.44 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "water_fountain_splash_sm";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "mp_italy_fountain" );
	ent.v[ "origin" ] = ( -190.766, 1536.19, 1388.34 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "mp_italy_fountain";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "mp_italy_fountain_splash_top" );
	ent.v[ "origin" ] = ( -191.781, 1536.14, 1383.23 );
	ent.v[ "angles" ] = ( 270, 244.421, 115.579 );
	ent.v[ "fxid" ] = "mp_italy_fountain_splash_top";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "dust_wind_fast_paper" );
	ent.v[ "origin" ] = ( -1444.2, 649.426, 1016 );
	ent.v[ "angles" ] = ( 272, 268.009, -6.00956 );
	ent.v[ "fxid" ] = "dust_wind_fast_paper";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_fall_gentlewind_codo" );
	ent.v[ "origin" ] = ( -1548.98, -633.034, 1029.66 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_fall_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_fall_gentlewind_codo" );
	ent.v[ "origin" ] = ( -1358.29, -963.026, 1036.05 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_fall_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( -848.101, -1283.31, 732.165 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_fall_gentlewind_codo" );
	ent.v[ "origin" ] = ( 955.971, -525.649, 892.109 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_fall_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_fall_gentlewind_codo" );
	ent.v[ "origin" ] = ( 535.939, -1217.53, 1018.41 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_fall_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( 833.284, -903.371, 704.125 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_spiral_runner" );
	ent.v[ "origin" ] = ( 1886.86, -207.521, 985.857 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_spiral_runner";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "dust_wind_fast_paper" );
	ent.v[ "origin" ] = ( 781.389, 351.109, 1024 );
	ent.v[ "angles" ] = ( 272, 182, -90 );
	ent.v[ "fxid" ] = "dust_wind_fast_paper";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_fall_gentlewind_codo" );
	ent.v[ "origin" ] = ( 1453.02, 468.678, 1285.98 );
	ent.v[ "angles" ] = ( 270, 0, 166 );
	ent.v[ "fxid" ] = "leaves_fall_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_fall_gentlewind_codo" );
	ent.v[ "origin" ] = ( -37.2386, 1881.42, 1484.89 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_fall_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_spiral_runner" );
	ent.v[ "origin" ] = ( -632.964, 1878.49, 1280 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_spiral_runner";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( 112.328, 1853.51, 1267.15 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_spiral_runner" );
	ent.v[ "origin" ] = ( 633.539, 1581.53, 1204.72 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_spiral_runner";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( 1234.47, -1141.95, 704 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( 1405.07, -614.326, 736 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_spiral_runner" );
	ent.v[ "origin" ] = ( -157.245, -663.325, 859.73 );
	ent.v[ "angles" ] = ( 270, 244.421, 115.579 );
	ent.v[ "fxid" ] = "leaves_spiral_runner";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( 736.626, 1878.57, 1214.82 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( 1090.93, 1876.21, 1216 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( 1196.92, 1162.44, 1127.28 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( -289.897, 598.934, 1152.12 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( 1682.13, 1375.97, 1216 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( 1696.77, 1832.18, 1216 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( 1850.82, 983.284, 1085 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "insects_moths" );
	ent.v[ "origin" ] = ( 1996.18, 893.344, 1210.07 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "insects_moths";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( -423.565, 869.53, 903.672 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "hawk" );
	ent.v[ "origin" ] = ( -870.726, -1357.16, 737.776 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "hawk";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "hawk" );
	ent.v[ "origin" ] = ( 761.917, 1239.58, 1153.67 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "hawk";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "leaves_ground_gentlewind_codo" );
	ent.v[ "origin" ] = ( -1005.11, -543.534, 766.021 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "leaves_ground_gentlewind_codo";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "room_dust_200_blend" );
	ent.v[ "origin" ] = ( -175.841, -40.2336, 864.125 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "room_dust_200_blend";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2256.72, 354.34, 1038.07 );
	ent.v[ "angles" ] = ( 278.624, 195.081, -9.79624 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2180.77, 494.063, 1045.1 );
	ent.v[ "angles" ] = ( 277.957, 359.574, 179.586 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2240.72, 143.143, 1006.15 );
	ent.v[ "angles" ] = ( 275.216, 179.873, -19.6065 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2173.1, 169.083, 1004.83 );
	ent.v[ "angles" ] = ( 274.764, 322.888, -168.839 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2131.25, -15.3397, 1003.83 );
	ent.v[ "angles" ] = ( 270, 0, -168 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2109.49, 45.3311, 1004.64 );
	ent.v[ "angles" ] = ( 280.184, 342.44, -168.966 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2251.87, 750.396, 1061.44 );
	ent.v[ "angles" ] = ( 275.6, 315.497, -136.749 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "water/creek_splash" );
	ent.v[ "origin" ] = ( 2284.06, 753.732, 1059.46 );
	ent.v[ "angles" ] = ( 278, 186, 0 );
	ent.v[ "fxid" ] = "water/creek_splash";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "drip_single" );
	ent.v[ "origin" ] = ( -1470.71, 1203.81, 1045.02 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "drip_single";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "drip_single" );
	ent.v[ "origin" ] = ( 1939.05, 1086.01, 1113.78 );
	ent.v[ "angles" ] = ( 270, 70.2427, -70.2428 );
	ent.v[ "fxid" ] = "drip_single";
	ent.v[ "delay" ] = -15;

	ent = createOneshotEffect( "drip_single" );
	ent.v[ "origin" ] = ( 441.466, -62.3711, 1036.71 );
	ent.v[ "angles" ] = ( 270, 0, 0 );
	ent.v[ "fxid" ] = "drip_single";
	ent.v[ "delay" ] = -15;

}
 
