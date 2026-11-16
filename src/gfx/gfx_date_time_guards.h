#ifndef GFX_DATE_TIME_GUARDS_H
#define GFX_DATE_TIME_GUARDS_H
#include <stdint.h>
typedef struct GfxDateGuard { uint32_t va,length,shipping_hash,retained_hash; const char *name; } GfxDateGuard;
static const GfxDateGuard g_gdtGuards[] = {
    {0x005C74E0u,196u,0x475EC934u,0x475EC934u,"PM timestamp producer"},
    {0x0088BCF0u,429u,0x0C78CC9Fu,0x4A81B79Fu,"Date constructor"},
    {0x0088A640u,137u,0xFD95D92Du,0xFD95D92Du,"Date setTime original"},
    {0x0088AA60u,236u,0x2C1BB0C9u,0xC38966B5u,"Date getHours"},
    {0x0088AB50u,236u,0x8DF039ADu,0x41E64535u,"Date getMinutes"},
    {0x00899D20u,183u,0x42B0F2BAu,0x42B0F2BAu,"AS3 setTime wrapper"},
    {0x00897AF0u,96u,0xC9FF9BE0u,0xC9FF9BE0u,"AS3 getHours wrapper"},
    {0x00B36E50u,19u,0x7554C1C5u,0x7554C1C5u,"setTime initializer"},
    {0x00B370E0u,19u,0x6B8A9BB1u,0x6B8A9BB1u,"getHours initializer"},
    {0x00C9E408u,24u,0xA3492146u,0xA3492146u,"setTime method table"},
    {0x00C9E5E8u,24u,0x829E6E18u,0x829E6E18u,"getHours method table"},
    {0x00ADFCF0u,160u,0xD4DD6E78u,0xD4DD6E78u,"native floor"},
    {0x00ADFE2Au,102u,0xE5AB42FFu,0xE5AB42FFu,"native remainder"},
    {0x00ADCC90u,160u,0x612A9814u,0x612A9814u,"native ceil"},
};
#endif
