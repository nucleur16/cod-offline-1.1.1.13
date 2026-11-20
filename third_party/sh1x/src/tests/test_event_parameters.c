#include "ol_events.h"
#include "bg.h"
#include "weapon_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    uint32_t value=99,events[4]={EV_NONE,EV_STANCE_FORCE_STAND,EV_STANCE_FORCE_CROUCH,EV_STANCE_FORCE_PRONE};
    uint32_t parameters[4]={UINT32_MAX,0,2,3},out[4]={9,8,7,6},saved[4];
    unsigned event,reason;
    {
        static const unsigned native[]={EV_RELOAD,EV_RELOAD_FROM_EMPTY,EV_RELOAD_START,EV_RECHAMBER_WEAPON,
            EV_FIRE_WEAPON,EV_FIRE_WEAPON_LASTSHOT};
        static const unsigned wire[]={18,19,20,34,30,31};
        uint32_t zero[4]={0},queue[4];
        for(event=0;event<6;++event) {
            assert(EventToWire(native[event])==(int)wire[event]);
            value=99;
            assert(EventParameterToWire(native[event],0,&value)==1 && !value);
            value=99;
            assert(EventParameterToWire(native[event],1,&value)==-1 && value==99);
            assert(EventParameterToWire(native[event],UINT32_MAX,&value)==-1 && value==99);
            if(event<4) queue[event]=native[event];
        }
        assert(ResolveEventParameters(queue,zero,out));
        assert(!out[0] && !out[1] && !out[2] && !out[3]);
        memcpy(saved,out,sizeof(out)); zero[3]=1;
        assert(!ResolveEventParameters(queue,zero,out) && !memcmp(out,saved,sizeof(out)));
        queue[0]=EV_FIRE_WEAPON; queue[1]=EV_FIRE_WEAPON_LASTSHOT; zero[3]=0;
        assert(ResolveEventParameters(queue,zero,out));
        assert(!out[0] && !out[1]);
    }
    for(event=EV_STANCE_FORCE_STAND;event<=EV_STANCE_FORCE_PRONE;++event) {
        for(reason=0;reason<=3;++reason) {
            assert(EventParameterToWire(event,reason,&value)==1 && value==reason);
            assert(EventToWire(event)==(int)event);
        }
        value=99; assert(EventParameterToWire(event,4,&value)==-1 && value==99);
        assert(EventParameterToWire(event,UINT32_MAX,&value)==-1 && value==99);
    }
    assert(EventParameterToWire(EV_NONE,UINT32_MAX,&value)==1 && !value);
    /* Sound alias carries the registered bank index: 1..255 is the whole
     * space and zero means no sound. */
    value=99; assert(EventParameterToWire(EV_SOUND_ALIAS,3,&value)==1 && value==3);
    value=99; assert(EventParameterToWire(EV_SOUND_ALIAS,0,&value)==-1 && value==99);
    value=99; assert(EventParameterToWire(EV_SOUND_ALIAS,256,&value)==-1 && value==99);
    assert(EventParameterToWire(UINT32_MAX,0,&value)==0 && value==99);
    assert(EventParameterToWire(EV_NONE,0,NULL)==-1);
    assert(ResolveEventParameters(events,parameters,out));
    assert(!out[0] && !out[1] && out[2]==2 && out[3]==3);
    /* An event with no wire translation at all fails the whole queue. */
    memcpy(saved,out,sizeof(out)); events[3]=UINT32_MAX;
    assert(!ResolveEventParameters(events,parameters,out) && !memcmp(out,saved,sizeof(out)));
    events[3]=EV_STANCE_FORCE_PRONE; parameters[3]=4;
    assert(!ResolveEventParameters(events,parameters,out) && !memcmp(out,saved,sizeof(out)));
    assert(!ResolveEventParameters(NULL,parameters,out));
    assert(!ResolveEventParameters(events,NULL,out));
    assert(!ResolveEventParameters(events,parameters,NULL));
    parameters[3]=1;
    assert(ResolveEventParameters(events,parameters,parameters));
    assert(!parameters[0] && parameters[3]==1);
    puts("PASS: stance event reason translation, unsupported event rejection and atomic queue resolution");
    /* Predicted events that used to go out as an empty slot (C1 queues them,
     * so an empty slot shifted its four-slot comparison). */
    {
        unsigned state;
        /* EV_STOP_WEAPON_SOUND: C1 0x419744..0x419773 sends the hand's C1
         * reload state 8..12; the retained producer passes its own 7..11. */
        assert(EventToWire(EV_STOP_WEAPON_SOUND)==2);
        for(state=7;state<=11;++state) {
            value=99;
            assert(EventParameterToWire(EV_STOP_WEAPON_SOUND,state,&value)==1 && value==state+1);
        }
        value=99;
        assert(EventParameterToWire(EV_STOP_WEAPON_SOUND,6,&value)==-1 && value==99);
        assert(EventParameterToWire(EV_STOP_WEAPON_SOUND,12,&value)==-1 && value==99);
        /* EV_DETONATE: zero at both C1 producers (0x41cf56, 0x41d8d1) whatever
         * weapon the retained producer names. */
        assert(EventToWire(EV_DETONATE)==102);
        value=99; assert(EventParameterToWire(EV_DETONATE,15,&value)==1 && value==0);
        value=99; assert(EventParameterToWire(EV_DETONATE,0,&value)==1 && value==0);
        /* Night vision and the lock-on hints: zero parameters. */
        assert(EventToWire(EV_NIGHTVISION_WEAR)==103 && EventToWire(EV_NIGHTVISION_REMOVE)==104);
        assert(EventToWire(EV_TARGET_TOO_CLOSE_HINT)==110);
        assert(EventToWire(EV_TARGET_NOT_ENOUGH_CLEARANCE)==111);
        assert(EventToWire(EV_LOCKON_REQUIRED_HINT)==112);
        value=99; assert(EventParameterToWire(EV_NIGHTVISION_WEAR,0,&value)==1 && value==0);
        value=99; assert(EventParameterToWire(EV_TARGET_NOT_ENOUGH_CLEARANCE,0,&value)==1 && value==0);
        assert(EventParameterToWire(EV_LOCKON_REQUIRED_HINT,1,&value)==-1);
        /* Foliage (C1 0x413cae, zero parameter) keeps its existing slot. */
        assert(EventToWire(EV_FOLIAGE_SOUND)==1);
        /* The left-hand entity fire events C1's CG_FireWeapon reads as hand 1. */
        assert(EventToWire(EV_FIRE_WEAPON_LEFT)==36);
        assert(EventToWire(EV_FIRE_WEAPON_LASTSHOT_LEFT)==37);
        value=99; assert(EventParameterToWire(EV_FIRE_WEAPON_LEFT,0,&value)==1 && value==0);
        puts("PASS: stop-weapon-sound, detonate, night vision, lock hints and left-hand fire reach the wire");
    }
}
