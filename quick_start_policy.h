#pragma once
#include <stdint.h>
#include <string.h>

/* Permission comes only from the genuine AS bridge. Native Ready snapshots
 * constrain that permission; they never create it. 30s is an explicit local
 * permission lifetime, covering the stock 10s enter-room delay. */
#define QSG_INTENT_MS 30000u
#define QSG_EARLY_INTENT_MS 120000u
#define QSG_MAX_MEMBERS 12u
typedef struct {
    uint32_t controller;
    uint64_t room;
    uint32_t count;
    uint64_t members[QSG_MAX_MEMBERS];
    unsigned char settings[29], next_entry, last_entry;
    char mode[32], alternate[64], map[64];
} QsgIdentity;
typedef struct {
    int managed, valid, host, all_ready, phase_known;
    uint32_t phase;
    QsgIdentity identity;
} QsgSnapshot;
typedef struct {
    unsigned state; /* 0 unrelated, 1 pending, 2 consumed, 3 invalidated, 4 waiting */
    uint32_t born, phase, valid_for_ms;
    QsgIdentity identity;
} QsgPolicy;

static int QsgEligible(const QsgSnapshot *s)
{
    return s->managed && s->valid && s->host && s->all_ready &&
        s->phase >= 3 && s->phase <= 5;
}
static int QsgIntentEligible(const QsgSnapshot *s)
{
    /* The genuine AS bridge can precede native's enter-room delay on a
     * successor. Capture that intent in waiting phase1; native consumption
     * still requires phase5 and the same complete Ready identity. */
    return s->managed && s->valid && s->host && s->all_ready &&
        s->phase >= 1 && s->phase <= 5;
}
static void QsgObserve(QsgPolicy *p, const QsgSnapshot *s, uint32_t now)
{
    if (!s->phase_known) {
        if (p->state) p->state=3; /* Missing data is not a proven phase exit. */
        return;
    }
    if (s->phase < 1 || s->phase > 5) {
        memset(p, 0, sizeof(*p));
        return;
    }
    if (s->phase == 1 && p->state == 2) {
        /* A completed native start returning to waiting begins another
         * cycle. It still needs another genuine bridge before consumption. */
        memset(p, 0, sizeof(*p));
    }
    if (!s->managed) {
        if (p->state) p->state=3; /* Scope drift cannot escape an owned cycle. */
        return;
    }
    if (!p->state) p->state=4;
    /* Stock mmbook advances next-entry after consumption. Only leaving the
     * allowed phase chain rearms a consumed generation. */
    if (p->state != 1) return;
    if (!QsgIntentEligible(s) || s->phase < p->phase ||
        memcmp(&p->identity, &s->identity, sizeof(p->identity)) ||
        (uint32_t)(now - p->born) >= p->valid_for_ms) {
        p->state = 3;
        return;
    }
    p->phase = s->phase;
}
static int QsgGrant(QsgPolicy *p, const QsgSnapshot *s, uint32_t now)
{
    QsgObserve(p, s, now);
    if (p->state != 4 || !QsgIntentEligible(s)) return 0;
    p->state = 1;
    p->born = now;
    /* Exact native frame passes through phase2 and can retain its stock
     * 90-second collection wait plus 10-second enter-room countdown. Early
     * genuine intent covers that bounded sequence; ordinary phase3+ intent
     * retains the existing 30-second lifetime. No duplicate extends either. */
    p->valid_for_ms = s->phase <= 2 ? QSG_EARLY_INTENT_MS : QSG_INTENT_MS;
    p->phase = s->phase;
    p->identity = s->identity;
    return 1;
}
static int QsgConsume(QsgPolicy *p, const QsgSnapshot *s, uint32_t now)
{
    QsgObserve(p, s, now);
    if (!s->managed) return p->state==0; /* Preserve genuinely unrelated modes. */
    if (p->state != 1 || !QsgEligible(s) || s->phase != 5) return 0;
    p->state = 2; /* Disarm before returning to native StartGame. */
    return 1;
}
