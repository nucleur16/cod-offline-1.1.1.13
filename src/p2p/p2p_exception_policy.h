#ifndef CODOL_P2P_EXCEPTION_POLICY_H
#define CODOL_P2P_EXCEPTION_POLICY_H
#include <stdint.h>
/* A VEH runs before thread SEH. Never rescue a migrated native call whose
 * recognized fault must terminate, potentially with an engine lock retained.
 * Reading the chain is bounded and delegated to ReadProcessMemory; malformed
 * metadata also declines rescue instead of recursively faulting in the VEH. */
typedef int (*PiSehReadFn)(uint32_t address,uint32_t words[2],void *context);
static int PiSehRequiresThreadDispatch(uint32_t head,uint32_t fatal_handler,
                                      PiSehReadFn read,void *context)
{
    unsigned i;
    if(!fatal_handler || !read)return 1;
    for(i=0;i<64;++i) {
        uint32_t words[2];
        if(head==UINT32_MAX)return 0;
        if(!head || (head&3u) || !read(head,words,context))return 1;
        if(words[1]==fatal_handler)return 1;
        if(words[0]!=UINT32_MAX && words[0]<=head)return 1;
        head=words[0];
    }
    return 1;
}
#endif
