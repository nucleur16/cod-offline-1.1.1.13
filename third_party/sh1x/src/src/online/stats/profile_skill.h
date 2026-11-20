#ifndef PROFILE_SKILL_H
#define PROFILE_SKILL_H
#ifdef __cplusplus
extern "C" {
#endif
/* Apply the IW4 skill reducer to two actual public playerdata records.
 * Returns0 before any rating write if either schema/path/record is invalid.
 * Does not establish script eligibility or imply upload/persistent storage. */
int UpdateProfileSkills(unsigned winner,unsigned loser,const char *mode,float weight);
void Scr_UpdateSkill(void);
#ifdef __cplusplus
}
#endif
#endif
