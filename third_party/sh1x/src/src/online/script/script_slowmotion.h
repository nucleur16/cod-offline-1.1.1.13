#ifndef SCRIPT_SLOWMOTION_H
#define SCRIPT_SLOWMOTION_H
/* C1 54a5d2 publishes to6132;466eb0 parses timestamp,duration,start,end.
 * Native common-frame scaling and full server teardown are integrated.
 * Fast restart retains bank and factor; full teardown resets both. */
void Scr_SetSlowMotion(void);
/* Unscaled microseconds; accumulated to C1's millisecond update granularity.
 * Used by the native common-frame scaler below. */
float SlowMotionAdvance(unsigned rawUsec);
unsigned SlowMotionScaleTime(unsigned rawUsec, float operatorScale,
                                 unsigned fixedUsec);
void SlowMotionReset(void);
#endif
