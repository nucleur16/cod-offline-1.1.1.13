/* Actual client fiber + actual native three-installer/renderer/observer path.
 * Kernel process/pipe/fiber and engine cvars/map execution are explicit doubles. */
static unsigned options_calls,options_observations;
static int options_result=1,observer_result=1,cancel_options,wrong_rules,late_frame_rule;
static int fixture_options(const void *p,unsigned n)
{
    assert(fixture_fiber&&fixture_thread==11&&script_calls==1&&principal_calls==1&&!init_calls);
    ++options_calls;
    int result=options_result==1?Sh1xHost_UseVerifiedMatchOptions(p,n):
        options_result==0?Sh1xHost_UseVerifiedMatchOptions(p,1):options_result;
    if(cancel_options)++g_p2pStartupActive;
    return result;
}
static int fixture_observe_options(const void *p,unsigned n)
{
    assert(fixture_fiber&&fixture_thread==11&&frame_calls>0&&host_in_call==0);
    ++options_observations;
    int result=Sh1xHost_ObserveMatchOptions(p,n);
    return result==1?observer_result:result;
}
int main(int argc,char **argv)
{
    assert(argc==4);size_t qn,pn;unsigned char *q=load(argv[2],&qn),*p=load(argv[3],&pn);
    protocol=2;g_piStartup.protocol=2;g_piStartup.pipe=(HANDLE)77;g_piStartup.started=clock_ms;g_piStartup.attempt=1;
    g_piStartup.packet=p;g_piStartup.packet_size=(DWORD)pn;g_piStartup.phase=P2PS_FETCHED;g_piStartup.fetch_claimed=1;
    assert(P2PStartupParseEnvelope(q,qn,p,pn,sha256,&g_piStartup.view));free(q);
    assert(g_piStartup.view.protocol==2&&g_piStartup.view.options_length==352);
    memcpy(g_piStartup.match_options,g_piStartup.view.options,352);
    assert(P2PACaptureBootstrap(&g_piStartup.view,&g_piStartup.active_bootstrap));
    realtime.tv_sec=1000;realtime.tv_nsec=0;monotime.tv_sec=10;monotime.tv_nsec=0;
    g_piStartup.expires_ms=1025000;g_piStartup.scope_started=clock_ms;g_piStartup.scope_lifetime=25000;
    g_piStartupOptions=fixture_options;g_piObserveOptions=fixture_observe_options;
    snprintf(g_piArgs,sizeof(g_piArgs),"+set dedicated 2 +set codol_admissionRequired 1 +set codol_zoneType %s +set codol_sessionMap %s +set g_gametype %s +map %s",
        g_piStartup.match_options+336,g_piStartup.match_options+240,g_piStartup.match_options+304,g_piStartup.match_options+176);
    if(!strcmp(argv[1],"script-fail"))script_result=0;
    else if(!strcmp(argv[1],"principal-fail"))principal_result=0;
    else if(!strcmp(argv[1],"options-fail"))options_result=0;
    else if(!strcmp(argv[1],"options-two"))options_result=2;
    else if(!strcmp(argv[1],"options-scope"))++g_piStartup.match_options[112];
    else if(!strcmp(argv[1],"cancel-options"))cancel_options=1;
    else if(!strcmp(argv[1],"init-fail"))init_result=-5;
    else if(!strcmp(argv[1],"wrong-rule"))wrong_rules=1;
    else if(!strcmp(argv[1],"late-frame-rule"))late_frame_rule=1;
    else if(!strcmp(argv[1],"observe-two"))observer_result=2;
    g_piState=PI_INIT_PENDING;resume_fiber();
    int installed=!strcmp(argv[1],"success")||wrong_rules||observer_result==2||late_frame_rule;
    if(installed) {
        assert(script_calls==1&&principal_calls==1&&options_calls==1&&init_calls==1&&native_engine_calls==1);
        assert(frame_calls==1&&options_observations==1&&g_piState==PI_RUNNING);
        if(wrong_rules||observer_result==2) {
            assert(!g_piMatchFresh&&!g_piPrincipalFresh&&g_piQuit&&!P2PInprocPrincipalFresh());
            assert(principal_faulted());
        } else {
            assert(g_piMatchFresh&&g_piPrincipalFresh&&P2PInprocPrincipalFresh());
            unsigned char retained[352];memcpy(retained,g_piStartup.match_options,352);
            unsigned char binding[32];memcpy(binding,g_piStartup.active_bootstrap.binding,32);
            fixture_thread=22;assert(P2PSAcknowledge(&g_piStartup));fixture_thread=11;
            assert(ack_sent==1&&!memcmp(ack_bytes,"CPSAv2\0\0",8)&&ack_bytes[72]==1);
            assert(!g_piStartup.packet&&freed==1&&!g_piStartup.aborted);
            assert(!memcmp(g_piStartup.match_options,retained,352)&&!memcmp(g_piStartup.active_bootstrap.binding,binding,32));
            assert(Sh1xHost_ObserveMatchOptions(retained,352)==1);
            /* A changed actual engine rule cannot leave cached Ready live. */
            if(!late_frame_rule)fixture_set("sv_maxclients","17");
            resume_fiber();
            assert(!P2PInprocPrincipalFresh()&&!g_piMatchFresh&&principal_faulted());
        }
    } else {
        assert(!g_piMatchFresh&&!g_piPrincipalFresh&&!P2PInprocPrincipalFresh()&&g_piState==PI_ENDED);
        assert(principal_faulted());
        if(init_result)assert(init_calls==1&&native_engine_calls==1);
        else assert(!init_calls&&!native_engine_calls);
        if(!strcmp(argv[1],"script-fail"))assert(!principal_calls&&!options_calls);
        if(!strcmp(argv[1],"principal-fail"))assert(!options_calls);
    }
    puts("PASS actual client v2 fiber/native options/provider/parser; engine/OS/DTLS boundaries explicit doubles");return 0;
}
