/* See match_gp.h. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "match_gp.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same_text(const char *a, const char *b)
{
    if (!a || !b) return 0;
    for (; *a && *b; ++a, ++b)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

static const char *cell(const engine_table_t *table, int row, int column)
{
    const char *text;
    if (!table || !table->values || row < 0 || column < 0 ||
        row >= table->rowCount || column >= table->columnCount) return NULL;
    text = table->values[row * table->columnCount + column];
    return text && *text ? text : NULL;
}

static int number(const char *text, double *out)
{
    char *end;
    double value;
    if (!text) return 0;
    value = strtod(text, &end);
    if (end == text) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\r') ++end;
    if (*end) return 0;
    *out = value;
    return 1;
}

static int find_row(const engine_table_t *table, const char *key)
{
    int row;
    if (!table || !key) return -1;
    for (row = 0; row < table->rowCount; ++row)
        if (same_text(cell(table, row, 0), key)) return row;
    return -1;
}

static int find_column(const engine_table_t *table, const char *name)
{
    int column;
    for (column = 0; table && column < table->columnCount; ++column)
        if (same_text(cell(table, 0, column), name)) return column;
    return -1;
}

int MatchGPParams(const engine_table_t *table, const char *mode,
                        match_gp_params_t *out)
{
    static const char *const names[6] = {
        "matchreward_timecap", "matchreward_scale_win", "matchreward_scale_loss",
        "matchreward_scale_tie", "matchreward_perminute", "matchreward_levelscale"
    };
    double values[6];
    int row, i;
    if (!table || !mode || !out) return 0;
    row = find_row(table, mode);
    if (row <= 0) return 0;              /* row 0 is the header */
    for (i = 0; i < 6; ++i) {
        int column = find_column(table, names[i]);
        if (column < 0 || !number(cell(table, row, column), &values[i])) return 0;
    }
    out->timecap_minutes = values[0];
    out->scale_win = values[1];
    out->scale_loss = values[2];
    out->scale_tie = values[3];
    out->perminute = values[4];
    out->levelscale = values[5];
    return 1;
}

double MatchGPLeagueScale(const engine_table_t *table, const char *row_name, int league)
{
    double value;
    int row;
    if (!row_name || !*row_name || same_text(row_name, "ranked")) return 1.0;
    if (league < 0 || league > 4) return 1.0;
    row = find_row(table, row_name);
    if (row < 0 || !number(cell(table, row, 1 + league), &value) || value < 0.0) return 1.0;
    return value;
}

int MatchGPAmount(const match_gp_params_t *params, int play_ms, int outcome,
                        int level, double league_scale, double activity_scale)
{
    double minutes, scale, amount;
    if (!params || play_ms <= 0 || level < 0) return 0;
    minutes = play_ms / 60000.0;
    if (minutes > params->timecap_minutes) minutes = params->timecap_minutes;
    switch (outcome) {
    case GP_OUTCOME_WIN: scale = params->scale_win; break;
    case GP_OUTCOME_TIE: scale = params->scale_tie; break;
    default:                   scale = params->scale_loss; break;
    }
    amount = params->perminute * minutes * scale * (1.0 + params->levelscale * level) *
             league_scale * activity_scale;
    /* int() truncation as _rank.gsc does; the epsilon only keeps an exact
     * product such as 10 x 10 x 1.5 = 150 from landing on 149.999... */
    if (!(amount > 0.0)) return 0;
    if (amount > 1000000000.0) amount = 1000000000.0;
    return (int)(amount + 1e-6);
}

/* ---- activities.info ---------------------------------------------------- */

typedef struct {
    int valid_from, valid_until;          /* YYYYMMDD */
    int begin_h, begin_m, end_h, end_m;   /* daily window */
    double gp_scale;
    int match_type;                       /* 1 playlist, 2 room, 3 all */
    int daily_once;
    int all_maps, all_modes;
    char maps[1024], modes[1024];
} activity_t;

/* C1 copies 4, 2 and 2 characters and atoi()s each piece (0x5082A0..0x5082F3). */
static int ymd(const char *text)
{
    char part[5];
    int year, month, day;
    size_t n = strlen(text);
    memset(part, 0, sizeof(part));
    strncpy(part, text, 4); year = atoi(part);
    memset(part, 0, sizeof(part));
    if (n > 4) strncpy(part, text + 4, 2);
    month = atoi(part);
    memset(part, 0, sizeof(part));
    if (n > 6) strncpy(part, text + 6, 2);
    day = atoi(part);
    return (year * 100 + month) * 100 + day;
}

static void set_list(char *dst, size_t size, int *all, const char *value)
{
    if (!strcmp(value, "all")) { *all = 1; return; }
    *all = 0;
    strncpy(dst, value, size - 1);
    dst[size - 1] = 0;
}

static void activity_set(activity_t *a, const char *key, const char *value)
{
    if (!strcmp(key, "valid_from")) a->valid_from = ymd(value);
    else if (!strcmp(key, "valid_until")) a->valid_until = ymd(value);
    else if (!strcmp(key, "daily_begin")) sscanf(value, "%d:%d", &a->begin_h, &a->begin_m);
    else if (!strcmp(key, "daily_end")) sscanf(value, "%d:%d", &a->end_h, &a->end_m);
    else if (!strcmp(key, "gp_scale")) { float f = 0.0f; if (sscanf(value, "%f", &f) == 1) a->gp_scale = f; }
    else if (!strcmp(key, "maps")) set_list(a->maps, sizeof(a->maps), &a->all_maps, value);
    else if (!strcmp(key, "modes")) set_list(a->modes, sizeof(a->modes), &a->all_modes, value);
    else if (!strcmp(key, "match")) {
        /* 0x508795..0x508931: strstr for "all", then "playlist", then "room". */
        if (strstr(value, "all")) a->match_type = 3;
        else if (strstr(value, "playlist")) a->match_type |= 1;
        else if (strstr(value, "room")) a->match_type |= 2;
    }
    else if (!strcmp(key, "daily_once")) a->daily_once = !strcmp(value, "yes");
}

static int list_has(const char *list, const char *name)
{
    char copy[1024], *token, *next;
    if (!name || !*name) return 0;
    strncpy(copy, list, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = 0;
    for (token = copy; token; token = next) {
        char *tail;
        next = strchr(token, ',');
        if (next) *next++ = 0;
        while (*token == ' ' || *token == '\t') ++token;
        tail = token + strlen(token);
        while (tail > token && (tail[-1] == ' ' || tail[-1] == '\t')) *--tail = 0;
        if (*token && same_text(token, name)) return 1;
    }
    return 0;
}

/* C1 0x508A50. */
static int activity_active(const activity_t *a, const struct tm *now, const char *map,
                           const char *gametype, int playlist)
{
    int today = ((now->tm_year + 1900) * 100 + now->tm_mon + 1) * 100 + now->tm_mday;
    int second = (now->tm_hour * 60 + now->tm_min) * 60 + now->tm_sec;
    if (today < a->valid_from || today > a->valid_until) return 0;
    if (second < (a->begin_h * 60 + a->begin_m) * 60) return 0;
    if (second > (a->end_h * 60 + a->end_m + 1) * 60) return 0;
    if (!a->match_type) return 0;
    if (a->match_type != 3) {
        if (playlist ? a->match_type != 1 : a->match_type != 2) return 0;
    }
    if (!a->all_maps && !list_has(a->maps, map)) return 0;
    if (!a->all_modes && !list_has(a->modes, gametype)) return 0;
    return 1;
}

static void trim(char **start, char **end)
{
    while (*start < *end && isspace((unsigned char)**start)) ++*start;
    while (*end > *start && isspace((unsigned char)(*end)[-1])) --*end;
}

double ActivityGPScale(const char *text, size_t length, const struct tm *now,
                             const char *map, const char *gametype, int playlist,
                             int *events)
{
    activity_t current;
    int open = 0, active = 0;
    double scale = 1.0;
    size_t pos = 0;
    if (events) *events = 0;
    if (!text || !now) return 1.0;
    memset(&current, 0, sizeof(current));
    for (;;) {
        char line[1100], *start, *end, *eq;
        size_t n = 0;
        int at_end = pos >= length;
        if (!at_end) {
            while (pos < length && text[pos] != '\n' && text[pos] != '\r' && text[pos]) {
                if (n + 1 < sizeof(line)) line[n++] = text[pos];
                ++pos;
            }
            while (pos < length && (text[pos] == '\n' || text[pos] == '\r' || !text[pos])) ++pos;
        }
        line[n] = 0;
        start = line; end = line + n;
        trim(&start, &end);
        *end = 0;
        if (at_end || *start == '[') {
            if (open && !current.daily_once &&
                activity_active(&current, now, map, gametype, playlist)) {
                scale *= current.gp_scale;
                ++active;
            }
            if (at_end) break;
            memset(&current, 0, sizeof(current));
            open = 1;
            continue;
        }
        if (!*start || *start == ';' || !open) continue;
        eq = strchr(start, '=');
        if (!eq) continue;
        {
            char *k0 = start, *k1 = eq, *v0 = eq + 1, *v1 = end;
            trim(&k0, &k1); trim(&v0, &v1);
            *k1 = 0; *v1 = 0;
            activity_set(&current, k0, v0);
        }
    }
    if (events) *events = active;
    return scale;
}
