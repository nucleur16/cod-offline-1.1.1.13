static int active_case,active_calls;
static int observation_case,observation_calls;
static uint64_t active_u64(const unsigned char *p){uint64_t n=0;unsigned i;for(i=0;i<8;i++)n|=(uint64_t)p[i]<<(8*i);return n;}
static void active_put64(unsigned char *p,uint64_t n){unsigned i;for(i=0;i<8;i++)p[i]=(unsigned char)(n>>(8*i));}
static void active_put32(unsigned char *p,uint32_t n){unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static int fixture_scope_observer(const void *p,unsigned n,void *out,unsigned length)
{
    assert(fixture_fiber&&fixture_thread==11&&!host_in_call&&!lock_depth);++observation_calls;
    int result=Sh1xHost_ObserveRelayPrincipals(p,n,out,length);
    if(observation_case==2)return 2;
    if(observation_case==3)PiActiveDeny(0);
    if(observation_case==4)parent_alive=0;
    return result;
}
static void scope_packet(const unsigned char *bundle,unsigned n)
{
    static const unsigned char domain[]="CODOL-P2P-NATIVE-OBSERVE-V1";
    unsigned char input[sizeof(domain)+192+112+656],nonce[32];size_t at=0;
    memset(nonce,'n',32);assert(P2PAMakeHello(&g_p2pa.boot,nonce,g_p2pa.hello));
    memset(g_p2pa.packet,0,sizeof(g_p2pa.packet));memcpy(g_p2pa.packet,"CPAQv1\0\0",8);
    active_put32(g_p2pa.packet+8,144+n);active_put32(g_p2pa.packet+12,n);
    memset(g_p2pa.packet+16,'q',32);memcpy(g_p2pa.packet+48,g_p2pa.hello+16,32);
    memcpy(g_p2pa.packet+144,bundle,n);assert(sha256(bundle,n,g_p2pa.packet+80));
    memcpy(input,domain,sizeof(domain));at+=sizeof(domain);memcpy(input+at,g_p2pa.hello,192);at+=192;
    memcpy(input+at,g_p2pa.packet,112);at+=112;memcpy(input+at,bundle,n);at+=n;
    assert(sha256(input,at,g_p2pa.packet+112));
}
static void query_case(const char *scenario)
{
    unsigned n=g_p2pa.boot.length;unsigned char bundle[656],reply[96];p2pa_policy_t policy;unsigned before=active_calls;
    memcpy(bundle,g_p2pa.boot.principals,n);P2PAInitPolicy(&g_p2pa.policy,&g_p2pa.boot);
    policy=g_p2pa.policy;g_p2pa.pipe=(HANDLE)77;g_p2paConnected=1;g_piObservePrincipals=fixture_scope_observer;
    if(!strcmp(scenario,"active-query-old-issued")){realtime.tv_sec+=20;monotime.tv_sec+=20;clock_ms+=20000;}
    if(!strcmp(scenario,"active-query-wrong-scope"))active_put64(bundle+40,active_u64(bundle+40)+1);
    if(!strcmp(scenario,"active-query-native-two"))observation_case=2;
    if(!strcmp(scenario,"active-query-cancel"))observation_case=3;
    if(!strcmp(scenario,"active-query-parent-loss"))observation_case=4;
    if(!strcmp(scenario,"active-query-missing-export"))g_piObservePrincipals=NULL;
    scope_packet(bundle,n);assert(PiActiveStagePacket(144+n));
    if(!strcmp(scenario,"active-query-owned-memory")) {
        g_p2pa.phase=2;assert(!PiActiveStagePacket(144+n));g_p2pa.phase=1;
    }
    fixture_fiber=1;PiActiveBetweenFrames();fixture_fiber=0;
    assert(g_p2pa.phase==3&&active_calls==before&&principal_calls==1);
    assert(g_p2pa.policy.sequence==policy.sequence&&g_p2pa.policy.native_sequence==policy.native_sequence);
    assert(principal_policy.sequence==1);assert(PiActiveMakeReply(reply));
    if(!strcmp(scenario,"active-query-success")||!strcmp(scenario,"active-query-old-issued")||!strcmp(scenario,"active-query-owned-memory")) {
        assert(g_p2pa.result==1&&g_p2paMode==P2PA_BOOTSTRAP&&!principal_faulted()&&observation_calls==1);
        assert(active_u64(reply+80)==principal_policy.mono_start+principal_policy.lifetime);
        assert(active_u64(reply+88)==(uint64_t)monotime.tv_sec*1000+monotime.tv_nsec/1000000);
        assert(PiActiveTransfer(reply,sizeof(reply),TRUE,0,clock_ms,5000));
    } else {
        unsigned i;assert(!g_p2pa.result&&principal_faulted()&&g_p2paMode==P2PA_DEAD);
        for(i=80;i<96;++i)assert(!reply[i]);
        assert(!PiActiveIoCurrent(0,clock_ms,5000)); /* Runtime failure is EOF, no new deny-I/O allowance. */
    }
    puts("PASS actual query codec→worker mailbox→fiber→native current scope→reply, no install/sequence grant");
}
static int fixture_active_installer(const void *p,unsigned n,uint64_t owner_deadline)
{
    assert(fixture_fiber&&fixture_thread==11&&!host_in_call&&!lock_depth);++active_calls;
    assert(owner_deadline==g_p2pa.policy.offered_deadline);
    int result=active_case==2?2:Sh1xHost_InstallActiveRelayPrincipals(p,n,owner_deadline);
    if(active_case==1)PiActiveDeny(0); /* Cancellation races successful install. */
    if(active_case==3)monotime.tv_sec+=5; /* Wall flat during actual native install. */
    if(active_case==4)monotime.tv_sec+=1; /* Consume entire explicit owner cap. */
    return result;
}
static void active_between_frame_case(const char *scenario)
{
    if(!strncmp(scenario,"active-io-",10)) {
        unsigned char bytes[19]={0};g_p2pa.pipe=(HANDLE)77;g_p2paConnected=1;
        if(!strcmp(scenario,"active-io-timeout"))io_mode=1;
        if(!strcmp(scenario,"active-io-parent"))io_mode=2;
        if(!strcmp(scenario,"active-io-foreign"))actual_server_pid=43;
        int okay=PiActiveTransfer(bytes,sizeof(bytes),FALSE,0,clock_ms,5000);
        assert(!io_pending);
        if(io_mode){assert(!okay&&io_cancels==1&&io_drains==1&&!io_completes);PiActiveDeny(0);assert(principal_faulted());}
        else if(actual_server_pid!=42)assert(!okay&&!io_waits);
        else assert(okay&&io_completes==7&&!io_cancels);
        puts("PASS actual active transfer fragment/cancel/drain/retained-parent boundary");return;
    }
    if(!strncmp(scenario,"active-query-",13)){query_case(scenario);return;}
    unsigned char packet[656];uint64_t wall,mono,owner_deadline;unsigned i;
    cvar_t key={0},id={0},maxclients={0};
    unsigned n=g_p2pa.boot.length;assert(n<=sizeof(packet));memcpy(packet,g_p2pa.boot.principals,n);
    active_put64(packet+40,active_u64(packet+40)+1);active_put64(packet+48,1000000);active_put64(packet+56,1029000);
    memset(&g_p2pa.command,0,sizeof(g_p2pa.command));g_p2pa.command.operation=P2PA_UPDATE;
    g_p2pa.command.sequence=1;g_p2pa.command.native_sequence=active_u64(packet+40);
    g_p2pa.command.issued=1000000;g_p2pa.command.expires=1029000;g_p2pa.command.hard_expires=1100000;
    memset(g_p2pa.command.active_id,'a',16);memset(g_p2pa.command.handoff_id,'h',32);
    g_p2pa.command.principals=packet;g_p2pa.command.length=n;g_p2pa.pipe=(HANDLE)77;
    g_p2paConnected=1;P2PAInitPolicy(&g_p2pa.policy,&g_p2pa.boot);
    /* Authenticate each real native peer through actual Receive/Bind. The
     * existing handshake/DATA cryptography is an explicit deterministic double. */
    key.string="session-key-path";id.string="4600000000000000";
    key_path=&key;session_id=&id;strcpy(configured_path,key.string);strcpy(configured_id,id.string);
    sessions_loaded=1;maxclients.integer=18;sv_maxclients=&maxclients;
    for(i=80;i<n;i+=32) {
        netadr_t endpoint={0};endpoint.type=NA_IP;memcpy(endpoint.ip,packet+i+8,4);
        endpoint.port=BigShort((short)(packet[i+12]|(unsigned)packet[i+13]<<8));
        associate(&endpoint,(byte)(i/32),active_u64(packet+i));
    }
    assert(PiActiveClock(&wall,&mono));owner_deadline=mono+29000;
    if(!strncmp(scenario,"active-owner-",13))owner_deadline=mono+1000;
    g_p2pa.command.owner_deadline_low=(uint32_t)owner_deadline;
    g_piActivePrincipals=fixture_active_installer;
    if(!strcmp(scenario,"active-missing-export"))g_piActivePrincipals=NULL;
    if(!strcmp(scenario,"active-missing-native-peer"))peers[0].authenticated=0;
    if(!strcmp(scenario,"active-abort-owned"))active_case=1;
    if(!strcmp(scenario,"active-native-two"))active_case=2;
    if(!strcmp(scenario,"active-install-budget"))active_case=3;
    if(!strcmp(scenario,"active-owner-native-delay"))active_case=4;
    if(!strcmp(scenario,"active-owner-transit")||!strcmp(scenario,"active-owner-expired-stage")) {
        assert(P2PAPrepare(&g_p2pa.policy,&g_p2pa.command,wall,mono));
        if(!strcmp(scenario,"active-owner-transit"))monotime.tv_nsec+=500000000;
        else monotime.tv_sec+=1;
    }
    if(!strcmp(scenario,"active-commit-bootstrap-boundary"))commit_clock_hook=1;
    if(!strcmp(scenario,"active-expired-bootstrap"))clock_ms+=25000;
    require_clock_lock=1;g_p2pa.phase=1;fixture_fiber=1;PiActiveBetweenFrames();fixture_fiber=0;require_clock_lock=0;
    assert(g_p2pa.phase==3);
    if(active_case==1||active_case==2||active_case==4||commit_clock_hook||
       !strcmp(scenario,"active-expired-bootstrap")||!strcmp(scenario,"active-missing-export")||
       !strcmp(scenario,"active-missing-native-peer")||!strcmp(scenario,"active-owner-expired-stage")) {
        assert(!g_p2pa.result&&g_p2paMode==P2PA_DEAD&&principal_faulted());
        if(!strcmp(scenario,"active-expired-bootstrap"))assert(!active_calls);
        if(!strcmp(scenario,"active-missing-export")||!strcmp(scenario,"active-owner-expired-stage"))assert(!active_calls);
        if(commit_clock_hook)assert(clock_ms>=25000&&active_calls==1);
    } else {
        assert(g_p2pa.result==1&&g_p2paMode==P2PA_ACTIVE&&!principal_faulted()&&active_calls==1);
        if(!strncmp(scenario,"active-owner-",13)) {
            assert(g_p2pa.policy.deadline==owner_deadline);
            assert(principal_policy.mono_start+principal_policy.lifetime==owner_deadline);
            if(!strcmp(scenario,"active-owner-wrap"))assert(owner_deadline>UINT32_MAX);
            monotime.tv_sec=owner_deadline/1000;monotime.tv_nsec=(owner_deadline%1000)*1000000;
            assert(!PiActivePermission()&&principal_faulted()&&g_p2paMode==P2PA_DEAD);
            puts("PASS actual client/native owner cap across staged delay/low32 wrap with UTC flat");return;
        }
        assert(g_p2pa.policy.deadline==39000&&g_p2pa.policy.hard_deadline==110000);
        assert(PiActiveClock(&wall,&mono));
        fixture_fiber=1;assert(Sh1xHost_RelayPrincipalsCurrent()==1);fixture_fiber=0;
        if(!strcmp(scenario,"active-scope-after-update")) {
            unsigned char reply[96];g_piObservePrincipals=fixture_scope_observer;g_p2pa.phase=0;
            scope_packet(packet,n);assert(PiActiveStagePacket(144+n));fixture_fiber=1;PiActiveBetweenFrames();fixture_fiber=0;
            assert(g_p2pa.result==1&&PiActiveMakeReply(reply));
            assert(g_p2pa.policy.sequence==1&&principal_policy.sequence==2&&g_p2paMode==P2PA_ACTIVE);
            assert(active_u64(reply+80)==principal_policy.mono_start+principal_policy.lifetime);
        }
        realtime.tv_sec=1026;monotime.tv_sec=36;clock_ms+=26000;
        assert(!P2PSCurrent(&g_piStartup)); /* Original budget never extends. */
        PiActiveBootstrapExpired();assert(g_p2paMode==P2PA_ACTIVE&&!principal_faulted());
        require_clock_lock=1;assert(PiActivePermission());require_clock_lock=0;
        if(!strcmp(scenario,"active-parent-loss")) {
            parent_alive=0;assert(!PiActivePermission());assert(principal_faulted());
        } else {
            g_p2pa.command.operation=P2PA_REVOKE;g_p2pa.command.sequence=2;
            g_p2pa.request_kind=0;
            g_p2pa.command.owner_deadline_low=0;
            PiActiveDeny(1);g_p2pa.phase=1;fixture_fiber=1;PiActiveBetweenFrames();fixture_fiber=0;
            assert(g_p2pa.result==1&&g_p2paMode==P2PA_DEAD&&g_p2pa.policy.native_sequence==2);
        }
    }
    puts("PASS actual active between-Frame/native parser/permission transition");
}
