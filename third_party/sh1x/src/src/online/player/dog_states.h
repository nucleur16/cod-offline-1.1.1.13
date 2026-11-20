#ifndef DOG_STATES_H
#define DOG_STATES_H
struct DObj_s;
#ifdef __cplusplus
extern "C" {
#endif
int DogStateIndex(const char *name);
/* Clip binding against the loaded C1 dog tree, cached per tree. `missing`
 * names the first clip the tree does not declare when binding fails. */
int DogClipBindings(unsigned tree,const unsigned **refs,const char **missing);
void DogClipBindingsReset(void);
/* Table introspection, for the offline mapping test and diagnostics. */
const char *DogClipName(unsigned index);
unsigned DogClipCount(void);
const char *DogStateName(unsigned index);
unsigned DogStateCount(void);
int OL_DogStateApply(struct DObj_s *,unsigned entity,unsigned generation,unsigned tree,unsigned time,
    int state,int previous,int entering,float rate,float lean,float aimUp,float aimLeft);
/* The zm hellhound: the same states, bound against animtrees/zombie_dog.atr. */
int ZombieDogClipBindings(unsigned tree,const unsigned **refs,const char **missing);
const char *ZombieDogClipName(unsigned index);
int ZombieDogStateApply(struct DObj_s *,unsigned entity,unsigned generation,unsigned tree,unsigned time,
    int state,int previous,int entering,float rate,float lean,float aimUp,float aimLeft);
#ifdef __cplusplus
}
#endif
#endif
