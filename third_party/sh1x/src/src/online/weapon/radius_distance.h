/* Distance from an explosion to an entity's absolute collision box.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef RADIUS_DISTANCE_H
#define RADIUS_DISTANCE_H

float RadiusBoxDistance(const float point[3], const float absmin[3],
                              const float absmax[3]);

#endif
