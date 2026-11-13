#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include "../src/kernel/kernel_call_guard.c"
__declspec(dllexport) int Call(uint32_t arg,uint32_t function,unsigned convention)
{return KRC_CallGuarded(arg,function,convention);}
__declspec(dllexport) int Dispatch(EXCEPTION_RECORD *r,void *f,CONTEXT *c)
{return KrcCallException(r,f,c,NULL);}
__declspec(dllexport) const uint32_t context_eip=offsetof(CONTEXT,Eip);
__declspec(dllexport) const uint32_t context_esp=offsetof(CONTEXT,Esp);
__declspec(dllexport) const uint32_t context_ebp=offsetof(CONTEXT,Ebp);
__declspec(dllexport) const uint32_t context_size=sizeof(CONTEXT);
__declspec(dllexport) const uint32_t frame_size=sizeof(KrcExceptionFrame);
__declspec(dllexport) const uint32_t failed_address=(uint32_t)(uintptr_t)KRC_CallFailed;
