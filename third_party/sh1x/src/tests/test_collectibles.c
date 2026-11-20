/* Unit test for the C1 'J' public-profile push used by story intel pickups
 * (script_collectibles.c). */
#include "../src/online/script/script_collectibles.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want)) { printf("FAIL %s: got '%s' want '%s'\n", what, got, want); ++failures; }
}

int main(void)
{
    char token[3], command[64];
    size_t n;

    ProfilePushByteToken(0x00, token); expect("token 0", token, "Z");
    ProfilePushByteToken(0x01, token); expect("token 1", token, "O");
    ProfilePushByteToken(0x08, token); expect("token 8", token, "08");
    ProfilePushByteToken(0xA5, token); expect("token a5", token, "A5");
    ProfilePushByteToken(0xFF, token); expect("token ff", token, "FF");

    /* collectibles[3] of mp/playerdata.def v220: record bit 31531 -> byte
     * 3941, value 0x08 once only that bit is set. */
    n = FormatPublicProfilePush(command, sizeof(command), 31531u >> 3, 0x08);
    expect("command", command, "J 3941 08");
    if (n != strlen("J 3941 08")) { printf("FAIL length %u\n", (unsigned)n); ++failures; }
    n = FormatPublicProfilePush(command, sizeof(command), 3942, 1);
    expect("command O", command, "J 3942 O");

    /* C1 refuses offsets at or past the 0x7ffc-byte record. */
    if (FormatPublicProfilePush(command, sizeof(command), 0x7ffc, 1)) {
        printf("FAIL accepted offset 0x7ffc\n"); ++failures;
    }
    if (!FormatPublicProfilePush(command, sizeof(command), 0x7ffb, 1)) {
        printf("FAIL refused offset 0x7ffb\n"); ++failures;
    }
    /* Too small an output buffer is refused, not truncated. */
    if (FormatPublicProfilePush(command, 6, 3941, 8)) {
        printf("FAIL truncated command accepted\n"); ++failures;
    }
    if (failures) return 1;
    printf("test_collectibles: OK\n");
    return 0;
}
