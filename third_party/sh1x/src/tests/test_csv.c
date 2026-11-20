#include "assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void cell(stringtable_t *t,unsigned r,unsigned c,const char *value)
{assert(!strcmp(StringTableCell(t,r,c)->text,value));}
int main(void)
{
    const char text[]="key,value,third\r\na,\"b,c\",\"d\"\"e\"\r\nb,\"line\nnext\"\nlast,";
    stringtable_t t={0};
    assert(ReadCSVStringTable(text,sizeof(text)-1,&t));
    assert(t.rows==4 && t.columns==3);cell(&t,0,0,"key");cell(&t,1,1,"b,c");
    cell(&t,1,2,"d\"e");cell(&t,2,1,"line\nnext");cell(&t,2,2,"");cell(&t,3,1,"");
    FreeStringTable(&t);
    const char *bad[]={"\"unterminated","\"closed\"extra","a\"b"};
    for(unsigned i=0;i<3;++i){t.rows=91;assert(!ReadCSVStringTable(bad[i],strlen(bad[i]),&t));assert(t.rows==91);}
    assert(!ReadCSVStringTable("x\0y",3,&t));
    assert(ReadCSVStringTable(",",1,&t));assert(t.rows==1 && t.columns==2);
    cell(&t,0,0,"");cell(&t,0,1,"");FreeStringTable(&t);
    puts("PASS: owned CSV cells, quotes/newlines, ragged rows, trailing empty fields and malformed input");
}
