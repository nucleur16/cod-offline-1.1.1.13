// RC rule from original T5 _killstreakrules.gsc, pinned 70689b8288e60079d48926657e670c41aab4abdf.
// T5: rcxd global3 / team2, FFA global=min(global,per-team)=2.
init()
{
    level.codolRCRule = spawnStruct();
    level.codolRCRule.cur = 0;
    level.codolRCRule.curTeam = [];
    level.codolRCRule.max = 3;
    if ( !level.teamBased )
        level.codolRCRule.max = 2;
}

isKillstreakAllowed( name, team )
{
    assert( name == "rcbomb_mp" );
    rule = level.codolRCRule;
    if ( rule.cur >= rule.max )
        return false;
    if ( level.teamBased && isDefined( rule.curTeam[team] ) && rule.curTeam[team] >= 2 )
        return false;
    if ( isDefined( self.lastStand ) && self.lastStand )
        return false;
    if ( isDefined( self.codolRCClaim ) && !self.codolRCClaim.released )
        return false;
    return true;
}

killstreakStart( name, team, displayTeamMessage, hacked )
{
    if ( !isKillstreakAllowed( name, team ) )
        return false;
    claim = spawnStruct();
    claim.team = team;
    claim.released = false;
    claim.rule = level.codolRCRule;
    self.codolRCClaim = claim;
    claim.rule.cur++;
    if ( level.teamBased )
    {
        if ( !isDefined( claim.rule.curTeam[team] ) )
            claim.rule.curTeam[team] = 0;
        claim.rule.curTeam[team]++;
    }
    return true;
}

killstreakStop( name, team )
{
    assert( name == "rcbomb_mp" );
    if ( !isDefined( self.codolRCClaim ) )
        return;
    release( self.codolRCClaim );
}

release( claim )
{
    // Original RC has competing disconnect/deletion/exit waiters. Each can
    // release the same claim, but must never decrement a later reward's claim.
    if ( claim.released )
        return;
    claim.released = true;
    claim.rule.cur--;
    if ( level.teamBased )
        claim.rule.curTeam[claim.team]--;
}
