#ifndef KNIFE_ATTACHMENT_H
#define KNIFE_ATTACHMENT_H
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
struct trace_s;
int KnifeWorldImpact(struct gentity_s *,struct trace_s *,const float *);
int KnifeDamageImpact(struct gentity_s *,struct gentity_s *,struct gentity_s *,
 const float *,const float *,int,int,int,unsigned,int,unsigned,unsigned,int,struct trace_s *);
int RunAttachedKnife(struct gentity_s *);
void KnifeCorpseCreated(unsigned,unsigned);
void ResetKnifeAttachment(unsigned);
void ResetKnifeAttachments(void);
void KnifePlayerSpawning(struct gentity_s *);
#ifdef __cplusplus
}
#endif
#endif
