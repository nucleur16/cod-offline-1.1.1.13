#include "match_report.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 assert(!MatchReportZombiesCheckpoint(1,1,0));
 assert(!MatchReportZombiesCheckpoint(3,0,0));
 assert(!MatchReportZombiesCheckpoint(3,1,13));
 assert(MatchReportZombiesCheckpoint(3,1,0));
 assert(MatchReportZombiesCheckpoint(3,1001,0));
 assert(!MatchReportZombiesCheckpoint(3,1000,0));
 assert(MatchReportZombiesCheckpoint(3,1001,0));
 puts("PASS production endless checkpoint mode/goal/range and monotonic state");
}
