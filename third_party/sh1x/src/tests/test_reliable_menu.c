#include "reliable_menu.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const char *ordered[]={"d 4274 field_order_get","v 31","d 4274 full_menu_31",
        "d 4274 field_order_win","v 31","d 4274 full_menu_31","d 4243 ","q 0","u 0","r","H"};
    unsigned i;
    for(i=0;i<sizeof(ordered)/sizeof(*ordered);++i)assert(ReliableMenuCommandOrdered(ordered[i]));
    assert(!ReliableMenuCommandOrdered("d 4242 other_bank"));
    assert(!ReliableMenuCommandOrdered("d 4275 material"));
    assert(!ReliableMenuCommandOrdered("d nonsense"));
    assert(!ReliableMenuCommandOrdered("d 4274bad"));
    assert(!ReliableMenuCommandOrdered("d 9999999999999999999999999 x"));
    assert(!ReliableMenuCommandOrdered("print"));
    puts("PASS: menu transitions and borrow-close-restore configstrings cannot coalesce; adjacent banks unchanged");
    return 0;
}
