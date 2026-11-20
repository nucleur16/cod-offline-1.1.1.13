/* i386 assembly VM boundary regression. Use the production declaration and
 * wrapper: the stand-in has the retained entry's one-argument/EAX ABI.
 * g++ -m32 -fno-pie -no-pie -I src tests/test_entityref_abi.cpp -o <test>
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "scr_vm.h"
#include <assert.h>
#include <stdio.h>

extern "C" {
volatile unsigned entityref_index;
volatile unsigned entityref_packed;
}
asm(".text\n"
    ".global Scr_GetEntityRef\n"
    "Scr_GetEntityRef:\n"
    "mov 4(%esp), %eax\n"
    "mov %eax, entityref_index\n"
    "mov entityref_packed, %eax\n"
    "ret\n");

int main()
{
    const unsigned refs[] = {0, 1, 0x20000, 0x20017, 0x2ffff, 0xffffabcd};
    for (unsigned i = 0; i < sizeof(refs)/sizeof(refs[0]); ++i) {
        entityref_packed = refs[i];
        entityref_index = 0xdeadbeef;
        scr_entref_t ref = Scr_GetEntityRefValue(i);
        assert(entityref_index == i);
        assert(ref.entnum == (refs[i] & 0xffff));
        assert(ref.classnum == (refs[i] >> 16));
    }
    puts("entityref i386 ABI: index and packed entity/class return passed");
}
