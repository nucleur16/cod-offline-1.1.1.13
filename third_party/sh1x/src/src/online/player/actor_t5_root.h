#ifndef ACTOR_T5_ROOT_H
#define ACTOR_T5_ROOT_H
/* T5 root-motion profiles of the zombie and hellhound move clips, measured in
 * real T5 (tools/t5_root_table.py, actor_t5_root.c). */
#ifdef __cplusplus
extern "C" {
#endif

#define T5_ROOT_STEPS 40

typedef struct {
    const char *clip;
    float length;                                 /* seconds at rate 1 */
    float cumulative[T5_ROOT_STEPS + 1];    /* 2D travel at i/40 of the clip */
} t5_root_profile_t;

const t5_root_profile_t *T5RootProfile(const char *clip);
/* 2D travel from the clip's start to normalised time t (0..1). */
float T5RootTravel(const t5_root_profile_t *profile, float t);

#ifdef __cplusplus
}
#endif
#endif
