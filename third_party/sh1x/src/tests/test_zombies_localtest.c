#include "localtest.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
#ifdef LOCAL_TEST_BUILD
 assert(LocalTestPolicy("1","1","zm","zm","2"));
#else
 assert(!LocalTestPolicy("1","1","zm","zm","2"));
#endif
 assert(!LocalTestPolicy(NULL,"1","zm","zm","2"));
 assert(!LocalTestPolicy("0","1","zm","zm","2"));
 assert(!LocalTestPolicy("1","0","zm","zm","2"));
 assert(!LocalTestPolicy("1","1","mp","zm","2"));
 assert(!LocalTestPolicy("1","1","zm","war","2"));
 assert(!LocalTestPolicy("1","1","zm","zm","1"));
 puts("PASS production localtest compile/environment/dvar/category/gametype/dedicated capability policy");
}
