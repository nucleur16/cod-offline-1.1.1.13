#ifndef VEHICLE_GRAPH_H
#define VEHICLE_GRAPH_H
#include "vehicle_drive.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* C1 GRAPH_FLOAT_FILE data. Output unchanged unless valid and complete. */
int ParseVehicleAccelerationGraph(const char *,size_t,vehicle_acceleration_t *);
/* Mounted type36 rawfile vehicle/<accelerationGraph>. 1=loaded,0=missing,-1=invalid.
 * This is the actual C1 graph dependency, not vehicles/<vehicle definition>. */
int LoadVehicleAccelerationGraph(const vehicle_raw_t *,vehicle_acceleration_t *);
/* Borrowed from the validated installed definition; reset/replacement invalidates it. */
const vehicle_acceleration_t *NativeVehicleAcceleration(int info_index);
#ifdef __cplusplus
}
#endif
#endif
