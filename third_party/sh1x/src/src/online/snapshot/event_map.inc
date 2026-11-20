/* Exact name matches: IW3 bg.h -> C1.exe event names at VA 0xCD81D8.
 * Generated/verified by tools/event_map.py. Unmatched names remain unsupported. */
    case EV_NONE: return 0;
    case EV_FOLIAGE_SOUND: return 1;
    case EV_STOP_WEAPON_SOUND: return 2;
    case EV_SOUND_ALIAS: return 3;
    case EV_SOUND_ALIAS_AS_MASTER: return 4;
    /* C1 names its slot 5 EV_STOPSOUND, the singular of the retained name,
     * so the generator's exact-name pass skipped it. Both tables place it
     * between the alias events and the stance forcing events, and C1
     * StopSounds 0x5361ee/0x536222 queues exactly 5 with parameter zero. */
    case EV_STOPSOUNDS: return 5;
    case EV_STANCE_FORCE_STAND: return 6;
    case EV_STANCE_FORCE_CROUCH: return 7;
    case EV_STANCE_FORCE_PRONE: return 8;
    case EV_ITEM_PICKUP: return 10;
    case EV_AMMO_PICKUP: return 11;
    case EV_NOAMMO: return 12;
    case EV_EMPTYCLIP: return 13;
    case EV_RESET_ADS: return 17;
    case EV_RELOAD: return 18;
    case EV_RELOAD_FROM_EMPTY: return 19;
    case EV_RELOAD_START: return 20;
    case EV_RELOAD_END: return 21;
    case EV_RELOAD_START_NOTIFY: return 22;
    case EV_RELOAD_ADDAMMO: return 23;
    case EV_RAISE_WEAPON: return 24;
    case EV_FIRST_RAISE_WEAPON: return 25;
    case EV_PUTAWAY_WEAPON: return 26;
    case EV_WEAPON_ALT: return 27;
    case EV_PULLBACK_WEAPON: return 29;
    case EV_FIRE_WEAPON: return 30;
    case EV_FIRE_WEAPON_LASTSHOT: return 31;
    /* C1 has no EV_FIRE_WEAPON_MG42: IW4 and C1 renamed the mounted-turret
     * fire event EV_FIRE_TURRET, slot 55 in the C1 name table at 0xCD81D8
     * (slot 56 is its sibling EV_FIRE_SENTRY). C1's own turret shot builder
     * 0x57acf0 queues exactly that ID on the turret entity through an inlined
     * G_AddEvent (0x57af5e..0x57b023; the event constant 0x37 is stored at
     * 0x57afce for a client owner and 0x57b005 for an entity ring, with the
     * firing entity's number loaded at 0x57af65 as the parameter). That is
     * the same (turret entity, event, attacker->s.number) shape the retained
     * turret_think_client emits as event 38 in _g_misc_mp.asm. Without this
     * case every mounted-turret shot was dropped by the snapshot's unmapped
     * event adaptation, so no client ever drew the turret's muzzle flash.
     * C1 reserves 56 for the no-client branch at 0x57af4d; the retained tree
     * has no producer for it, so only the player/actor event maps here. */
    case EV_FIRE_WEAPON_MG42: return 55;
    case EV_RECHAMBER_WEAPON: return 34;
    case EV_EJECT_BRASS: return 35;
    case EV_EJECT_BRASS_SECONDARY: return 38;
    case EV_MELEE_SWIPE: return 47;
    case EV_FIRE_MELEE: return 48;
    case EV_PREP_OFFHAND: return 49;
    case EV_USE_OFFHAND: return 50;
    case EV_SWITCH_OFFHAND: return 51;
    case EV_WEAPON_SWITCH_STARTED: return 28;
    case EV_DTP_LAND: return 186;
    case EV_MANTLE_START: return 183;
    case EV_SCOPE_ZOOM: return 184;
    case EV_FIRE_WEAPON_LEFT: return 36;
    case EV_FIRE_WEAPON_LASTSHOT_LEFT: return 37;
    case EV_DTP_LAUNCH: return 185;
    case EV_STANCE_INVALID: return 9;
    case EV_OFFHAND_END_NOTIFY: return 16;
    case EV_MELEE_HIT: return 52;
    case EV_MELEE_MISS: return 53;
    case EV_MELEE_BLOOD: return 54;
    case EV_FIRE_QUADBARREL_1: return 57;
    case EV_FIRE_QUADBARREL_2: return 58;
    case EV_BULLET_HIT: return 60;
    case EV_BULLET_HIT_CLIENT_SMALL: return 63;
    case EV_BULLET_HIT_CLIENT_LARGE: return 64;
    case EV_GRENADE_BOUNCE: return 69;
    case EV_GRENADE_EXPLODE: return 73;
    case EV_ROCKET_EXPLODE: return 76;
    case EV_ROCKET_EXPLODE_NOMARKS: return 77;
    case EV_FLASHBANG_EXPLODE: return 78;
    case EV_CUSTOM_EXPLODE: return 79;
    case EV_CUSTOM_EXPLODE_NOMARKS: return 80;
    case EV_CHANGE_TO_DUD: return 81;
    case EV_DUD_EXPLODE: return 82;
    case EV_DUD_IMPACT: return 83;
    case EV_BULLET: return 86;
    case EV_PLAY_FX: return 87;
    case EV_PLAY_FX_ON_TAG: return 89;
    case EV_PLAY_FX_ON_TAG_FOR_CLIENTS: return 89;
    case EV_STOP_FX_ON_TAG: return 90;
    case EV_PLAY_VIEWMODEL_FX_ON_TAG: return 92;
    case EV_STOP_VIEWMODEL_FX_ON_TAG: return 93;
    case EV_MISSILE_REMOTE_BOOST: return 105;
    case EV_PHYS_EXPLOSION_SPHERE: return 95;
    case EV_PHYS_EXPLOSION_CYLINDER: return 96;
    case EV_PHYS_EXPLOSION_JOLT: return 97;
    case EV_PHYS_JITTER: return 99;
    case EV_EARTHQUAKE: return 100;
    case EV_GRENADE_SUICIDE: return 101;
    case EV_DETONATE: return 102;
    case EV_NIGHTVISION_WEAR: return 103;
    case EV_NIGHTVISION_REMOVE: return 104;
    case EV_OBITUARY: return 107;
    case EV_TARGET_TOO_CLOSE_HINT: return 110;
    /* C1 fire routine 0x41ba51 queues 111 (EV_TARGET_NOT_ENOUGH_CLEARANCE_HINT
     * in the name table 0xCD81D8); weapon_lock_fire.c returns it. */
    case EV_TARGET_NOT_ENOUGH_CLEARANCE: return 111;
    case EV_LOCKON_REQUIRED_HINT: return 112;
    case EV_FOOTSTEP_SPRINT: return 116;
    case EV_FOOTSTEP_RUN: return 117;
    case EV_FOOTSTEP_WALK: return 118;
    case EV_FOOTSTEP_PRONE: return 119;
    case EV_JUMP: return 120;
    case EV_LANDING_DEFAULT: return 121;
    case EV_LANDING_BARK: return 122;
    case EV_LANDING_BRICK: return 123;
    case EV_LANDING_CARPET: return 124;
    case EV_LANDING_CLOTH: return 125;
    case EV_LANDING_CONCRETE: return 126;
    case EV_LANDING_DIRT: return 127;
    case EV_LANDING_FLESH: return 128;
    case EV_LANDING_FOLIAGE: return 129;
    case EV_LANDING_GLASS: return 130;
    case EV_LANDING_GRASS: return 131;
    case EV_LANDING_GRAVEL: return 132;
    case EV_LANDING_ICE: return 133;
    case EV_LANDING_METAL: return 134;
    case EV_LANDING_MUD: return 135;
    case EV_LANDING_PAPER: return 136;
    case EV_LANDING_PLASTER: return 137;
    case EV_LANDING_ROCK: return 138;
    case EV_LANDING_SAND: return 139;
    case EV_LANDING_SNOW: return 140;
    case EV_LANDING_WATER: return 141;
    case EV_LANDING_WOOD: return 142;
    case EV_LANDING_ASPHALT: return 143;
    case EV_LANDING_CERAMIC: return 144;
    case EV_LANDING_PLASTIC: return 145;
    case EV_LANDING_RUBBER: return 146;
    case EV_LANDING_CUSHION: return 147;
    case EV_LANDING_FRUIT: return 148;
    case EV_LANDING_PAINTEDMETAL: return 149;
    case EV_LANDING_PAIN_DEFAULT: return 152;
    case EV_LANDING_PAIN_BARK: return 153;
    case EV_LANDING_PAIN_BRICK: return 154;
    case EV_LANDING_PAIN_CARPET: return 155;
    case EV_LANDING_PAIN_CLOTH: return 156;
    case EV_LANDING_PAIN_CONCRETE: return 157;
    case EV_LANDING_PAIN_DIRT: return 158;
    case EV_LANDING_PAIN_FLESH: return 159;
    case EV_LANDING_PAIN_FOLIAGE: return 160;
    case EV_LANDING_PAIN_GLASS: return 161;
    case EV_LANDING_PAIN_GRASS: return 162;
    case EV_LANDING_PAIN_GRAVEL: return 163;
    case EV_LANDING_PAIN_ICE: return 164;
    case EV_LANDING_PAIN_METAL: return 165;
    case EV_LANDING_PAIN_MUD: return 166;
    case EV_LANDING_PAIN_PAPER: return 167;
    case EV_LANDING_PAIN_PLASTER: return 168;
    case EV_LANDING_PAIN_ROCK: return 169;
    case EV_LANDING_PAIN_SAND: return 170;
    case EV_LANDING_PAIN_SNOW: return 171;
    case EV_LANDING_PAIN_WATER: return 172;
    case EV_LANDING_PAIN_WOOD: return 173;
    case EV_LANDING_PAIN_ASPHALT: return 174;
    case EV_LANDING_PAIN_CERAMIC: return 175;
    case EV_LANDING_PAIN_PLASTIC: return 176;
    case EV_LANDING_PAIN_RUBBER: return 177;
    case EV_LANDING_PAIN_CUSHION: return 178;
    case EV_LANDING_PAIN_FRUIT: return 179;
    case EV_LANDING_PAIN_PAINTEDMETAL: return 180;
