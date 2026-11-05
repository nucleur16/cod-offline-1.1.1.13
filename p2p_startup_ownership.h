/* Shared atomic phase protocol; no caller Boolean can stand in for ownership. */
#ifndef CODOL_P2P_STARTUP_OWNERSHIP_H
#define CODOL_P2P_STARTUP_OWNERSHIP_H
#include <stdint.h>
#ifdef _WIN32
typedef long p2ps_atomic_value;
#else
typedef int32_t p2ps_atomic_value;
#endif
typedef p2ps_atomic_value (*p2ps_cas_fn)(volatile p2ps_atomic_value *,p2ps_atomic_value,p2ps_atomic_value);
enum { P2PS_FETCHED=1,P2PS_FIBER_OWNED=2,P2PS_FIBER_DONE=3,P2PS_CANCELLED=4 };
static int P2PSReserveContext(volatile p2ps_atomic_value *claim,p2ps_cas_fn cas)
{ return cas(claim,1,0)==0; }
static int P2PSClaimForFiber(volatile p2ps_atomic_value *phase,p2ps_cas_fn cas)
{ return cas(phase,P2PS_FIBER_OWNED,P2PS_FETCHED)==P2PS_FETCHED; }
static p2ps_atomic_value P2PSCancelOrObserve(volatile p2ps_atomic_value *phase,p2ps_cas_fn cas)
{ return cas(phase,P2PS_CANCELLED,P2PS_FETCHED); }
static int P2PSWorkerMayReclaim(p2ps_atomic_value observed)
{
    return observed==0||observed==P2PS_FETCHED||observed==P2PS_FIBER_DONE||observed==P2PS_CANCELLED;
}
#endif
