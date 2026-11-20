#include "matchdata.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned size=858,loads;
int LoadMountedStructuredSchema(const char *name,structured_schema_t *s)
{
    ++loads;
    if(!strcmp(name,"missing"))return 0;
    s->count=1;s->definitions=calloc(1,sizeof(*s->definitions));assert(s->definitions);
    s->definitions[0].version=16;s->definitions[0].checksum=0x3fd7bbad;
    s->definitions[0].layout.record_size=size;
    return 1;
}
int main(void)
{
    client_matchdata_t *m;unsigned i;
    char longname[65];memset(longname,'a',64);longname[64]=0;
    ResetClientMatchData();assert(!ClientMatchData());
    assert(!SelectClientMatchData(NULL));assert(!SelectClientMatchData(""));
    assert(!SelectClientMatchData(longname));assert(loads==0);
    assert(!SelectClientMatchData("missing"));assert(!ClientMatchData());
    size=1025;assert(!SelectClientMatchData("oversize"));assert(!ClientMatchData());
    size=7;assert(!SelectClientMatchData("tiny"));assert(!ClientMatchData());
    size=858;assert(SelectClientMatchData("mp/clientmatchdata.def"));
    m=ClientMatchData();assert(m);
    assert(!memcmp(m->record,"\x10\0\0\0\xad\xbb\xd7\x3f",8));
    for(i=8;i<sizeof(m->record);++i)assert(m->record[i]==0);
    m->record[654]=17;i=loads;
    RequestClientMatchData(0);RequestClientMatchData(1);
    assert(!ClientMatchDataDue(0,999) && ClientMatchDataDue(0,1000));
    ClientMatchDataSent(0,1000);
    assert(!ClientMatchDataDue(0,1999) && ClientMatchDataDue(0,2000));
    AcknowledgeClientMatchData(0);
    assert(!ClientMatchDataDue(0,3000) && ClientMatchDataDue(1,3000));
    RequestClientMatchData(0);assert(ClientMatchDataDue(0,3000));
    assert(SelectClientMatchData("MP/CLIENTMATCHDATA.DEF"));
    assert(loads==i && m->record[654]==17);
    assert(!SelectClientMatchData("other"));assert(loads==i && m->record[654]==17);
    ResetClientMatchData();assert(!ClientMatchData());
    assert(!ClientMatchDataDue(0,5000) && !ClientMatchDataDue(1,5000));
    size=1024;assert(SelectClientMatchData("other"));
    assert(ClientMatchData()->record[654]==0);
    ResetClientMatchData();ResetClientMatchData();
    puts("PASS: match-data schema ownership, selection, headers and reset");
}
