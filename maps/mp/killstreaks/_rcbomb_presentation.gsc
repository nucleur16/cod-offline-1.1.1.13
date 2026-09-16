// T5 _rcbomb.csc presentation (persistent light, countdown/timeout blink,
// stun sparks, driving tread FX), emitted through existing native C1 tagged
// effect events. This is server GSC, not an injected CSC: C1 has no client
// script, so the effects the T5 CSC played locally are queued here.
monitor()
{
    self endon( "death" );
    self endon( "codol_rc_hidden" );
    self.codolRCBlinking = false;
    for ( ;; )
    {
        self waittill( "codol_vehicle_clientflag", flag, set );
        // T5 _rcbomb.csc:560-585 stunnedHandler/play_stunned_fx_handler:
        // sparks every 0.5 s while the stun flag is set.
        if ( flag == level.const_flag_stunned )
        {
            if ( set )
                self thread stunSparks();
            else
                self notify( "codol_rc_stun_fx_stop" );
            continue;
        }
        if ( !set )
            continue;
        if ( flag == level.const_flag_countdown && !self.codolRCBlinking )
        {
            self.codolRCBlinking = true;
            self.codolRCBlinkInterval = 1;
            self thread blink();
        }
        else if ( flag == level.const_flag_timeout )
            self.codolRCBlinkInterval = 0.133;
    }
}

blink()
{
    self endon( "death" );
    self endon( "codol_rc_hidden" );
    // T5 _rcbomb.csc:125-131 blink_light: the persistent light stops when
    // the countdown blink takes over.
    self stopPersistentLight();
    for ( ;; )
    {
        viewers = self splitViewers();
        friendly = viewers["friendly"];
        enemy = viewers["enemy"];
        if ( friendly.size )
            playFXOnTagForClients( level._effect["rcbomb_green"], self, "tag_origin", friendly );
        if ( enemy.size )
            playFXOnTagForClients( level._effect["rcbomb_red"], self, "tag_origin", enemy );
        wait self.codolRCBlinkInterval;
        self.codolRCBlinkInterval = self.codolRCBlinkInterval / 1.17;
        if ( self.codolRCBlinkInterval < 0.1 )
            self.codolRCBlinkInterval = 0.1;
    }
}

// Threaded on the car by _rcbomb::spawnRCBomb right after its team is set,
// so the persistent light and tread FX exist for the antenna-pull interval
// before control is handed over (T5 _rcbomb.csc:52-58 main entity init).
start()
{
    self endon( "death" );
    self endon( "codol_rc_hidden" );
    self.codolRCLightOn = false;
    self thread persistentLight();
    self thread treadFx();
}

// Same friend/foe split the T5 CSC made per local client (friendNotFoe):
// the owner and, in team games, the owner's team are friendly.
splitViewers()
{
    viewers = [];
    viewers["friendly"] = [];
    viewers["enemy"] = [];
    foreach ( player in level.players )
    {
        if ( !isDefined( player ) )
            continue;
        if ( player == self.owner || ( level.teamBased && player.team == self.team ) )
            viewers["friendly"][viewers["friendly"].size] = player;
        else
            viewers["enemy"][viewers["enemy"].size] = player;
    }
    return viewers;
}

// T5 _rcbomb.csc:63-83 spawn_solid_fx/start_light: friendly viewers get the
// solid light, enemies the blinky light, once on tag_origin. Both are
// looping effects; they persist on the client until stopped.
persistentLight()
{
    self endon( "death" );
    self endon( "codol_rc_hidden" );
    viewers = self splitViewers();
    if ( viewers["friendly"].size )
        playFXOnTagForClients( level._effect["rcbomb_solid"], self, "tag_origin", viewers["friendly"] );
    if ( viewers["enemy"].size )
        playFXOnTagForClients( level._effect["rcbomb_blinky"], self, "tag_origin", viewers["enemy"] );
    self.codolRCLightOn = true;
}

stopPersistentLight()
{
    if ( !isDefined( self.codolRCLightOn ) || !self.codolRCLightOn )
        return;
    self.codolRCLightOn = false;
    stopFXOnTag( level._effect["rcbomb_solid"], self, "tag_origin" );
    stopFXOnTag( level._effect["rcbomb_blinky"], self, "tag_origin" );
}

// T5 _rcbomb.csc:572-583 play_stunned_fx_handler.
stunSparks()
{
    self notify( "codol_rc_stun_fx_stop" );
    self endon( "codol_rc_stun_fx_stop" );
    self endon( "death" );
    self endon( "codol_rc_hidden" );
    for ( ;; )
    {
        playFXOnTag( level._effect["rcbomb_stunned"], self, "tag_origin" );
        wait 0.5;
    }
}

// T5 _rcbomb.csc:644-690 default_*_surface_type, applied to the native C1
// surface name under the car (codol_rcPlacementTrace "surfacetype").
treadSurface()
{
    trace = codol_rcPlacementTrace( self.origin + (0,0,6), self.origin - (0,0,14), (-2,-2,0), (2,2,1), self, 3 );
    surface = trace["surfacetype"];
    switch ( surface )
    {
        case "dirt":
        case "mud":
        case "gravel":
        case "grass":
        case "foliage":
        case "sand":
        case "water":
            return "dust";
        case "snow":
        case "ice":
            return "snow";
    }
    return "concrete";
}

// T5 _rcbomb.csc:287-323 (driver) and :376-410 (third person) play_driving_fx.
// The CSC read the vehicle's signed speed, throttle and wheel-slip state; the
// server has the native speed magnitude (vehicle_getspeed, mph), so reverse
// comes from the origin delta against the body forward, peel-out from a
// standing-start acceleration and drift from the yaw rate. The bare T5
// third-person "driving" effect does not ship in C1, so the driver sees the
// first-person variant and other viewers the slow variant at cruising speed.
treadFx()
{
    self endon( "death" );
    self endon( "codol_rc_hidden" );
    topSpeed = 38; // rc_car_medium_mp topSpeed/reverseSpeed, mph
    lastOrigin = self.origin;
    lastYaw = self.angles[1];
    lastMph = 0;
    for ( ;; )
    {
        wait 0.2;
        mph = self vehicle_getspeed();
        delta = self.origin - lastOrigin;
        yawRate = abs( angleClamp180( self.angles[1] - lastYaw ) );
        accel = mph - lastMph;
        reverse = vectorDot( delta, anglesToForward( self.angles ) ) < 0;
        lastOrigin = self.origin;
        lastYaw = self.angles[1];
        lastMph = mph;
        if ( mph < 1 )
            continue;
        surface = self treadSurface();
        fraction = mph / topSpeed;
        trail = false;
        if ( accel > 6 && mph < 15 )
        {
            trail = true;
            playFXOnTag( level._effect["rcbomb_tread_" + surface + "_peel"], self, "tag_origin" );
        }
        if ( yawRate > 25 && mph > 8 )
        {
            trail = true;
            playFXOnTag( level._effect["rcbomb_tread_" + surface + "_drift"], self, "tag_origin" );
        }
        if ( reverse && mph > 5 )
            playFXOnTag( level._effect["rcbomb_tread_" + surface + "_reverse"], self, "tag_origin" );
        else if ( fraction >= 0.25 )
        {
            others = [];
            foreach ( player in level.players )
            {
                if ( isDefined( player ) && player != self.owner )
                    others[others.size] = player;
            }
            if ( isDefined( self.owner ) )
                playFXOnTagForClients( level._effect["rcbomb_tread_" + surface + "_first_person"], self, "tag_origin", self.owner );
            if ( others.size )
                playFXOnTagForClients( level._effect["rcbomb_tread_" + surface + "_slow"], self, "tag_origin", others );
        }
        else if ( mph > 5 )
            playFXOnTag( level._effect["rcbomb_tread_" + surface + "_slow"], self, "tag_origin" );
        if ( fraction >= 0.75 && !reverse )
            trail = true;
        if ( trail )
            playFXOnTag( level._effect["rcbomb_tread_" + surface + "_trail"], self, "tag_origin" );
    }
}
