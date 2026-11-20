#include "reliable_menu.h"
#include "configstrings.h"
#include <stdlib.h>
#include <errno.h>
int ReliableMenuCommandOrdered(const char *command)
{
    char *end;
    unsigned long index;
    if(!command || !*command || (command[1] && command[1]!=' '))return 0;
    if(command[0]=='q' || command[0]=='u' || command[0]=='v' ||
       command[0]=='r' || command[0]=='H')return 1;
    if(command[0]!='d' || command[1]!=' ')return 0;
    errno=0;index=strtoul(command+2,&end,10);
    if(errno==ERANGE || end==command+2 || (*end && *end!=' '))return 0;
    return index>=OL_CS_MENUS && index<OL_CS_MENUS+MENU_SLOTS;
}
