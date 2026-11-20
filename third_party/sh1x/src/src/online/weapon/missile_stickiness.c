#include "missile_stickiness.h"
#include <math.h>
#include <stdint.h>

int ReadMissileStickiness(const mounted_weapon_t *source,
    missile_stickiness_t *out)
{
    missile_stickiness_t result;
    const uint8_t *data;
    uint32_t mode;
    if (!source || !out || source->asset.type != 28 ||
        !source->asset.data || source->asset.size < 0xbfc)
        return 0;
    data = source->asset.data;
    mode = (uint32_t)data[0x7c8] | ((uint32_t)data[0x7c9] << 8) |
        ((uint32_t)data[0x7ca] << 16) | ((uint32_t)data[0x7cb] << 24);
    if (mode > STICK_FLESH || data[0xbde] > 1)
        return 0;
    result.mode = (missile_stickiness_mode_t)mode;
    result.stick_to_player_or_actor = data[0xbde];
    *out = result;
    return 1;
}

int MissileSupportsAttachment(const missile_stickiness_t *definition)
{
    return definition->mode == STICK_ALL ||
        definition->mode == STICK_ALL_NO_SENTIENTS ||
        definition->mode == STICK_FLESH;
}

int NativeMissileSupportsAttachment(const struct WeaponDef *definition)
{
    missile_stickiness_t metadata;
    return GetNativeWeaponMissileStickiness(definition, &metadata) &&
        MissileSupportsAttachment(&metadata);
}

int PredictMissileStop(const missile_stickiness_t *definition,
    int target_is_player_or_actor, const float normal[3],
    const float reflected_velocity[3], float stop_speed)
{
    float speed;
    if (target_is_player_or_actor)
        return definition->stick_to_player_or_actor == 1;
    if (definition->mode == STICK_ALL ||
        definition->mode == STICK_ALL_NO_SENTIENTS)
        return 1;

    if (definition->mode == STICK_FLESH) {
        float inverse_speed, direction[3], dot;
        /* Match 6190b0's rounded length and reciprocal, including zero. */
        speed = sqrtf(reflected_velocity[0] * reflected_velocity[0] +
            reflected_velocity[1] * reflected_velocity[1] +
            reflected_velocity[2] * reflected_velocity[2]);
        if (normal[2] > 0.7f && speed < stop_speed)
            return 1;
        inverse_speed = 1.0f / (speed > 0.0f ? speed : 1.0f);
        direction[0] = reflected_velocity[0] * inverse_speed;
        direction[1] = reflected_velocity[1] * inverse_speed;
        direction[2] = reflected_velocity[2] * inverse_speed;
        dot = normal[0] * direction[0] + normal[1] * direction[1] +
            normal[2] * direction[2];
        return dot > 0.7f;
    }
    if (!(normal[2] > 0.7f))
        return 0;
    if (definition->mode != STICK_NONE)
        return 1;
    speed = sqrtf(reflected_velocity[0] * reflected_velocity[0] +
        reflected_velocity[1] * reflected_velocity[1] +
        reflected_velocity[2] * reflected_velocity[2]);
    return speed < stop_speed;
}
