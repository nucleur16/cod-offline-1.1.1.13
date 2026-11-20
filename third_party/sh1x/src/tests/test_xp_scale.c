/* Regression for the public combat-training 25% XP rate. Links the real
 * cvar registration/set/read implementation and the real GSC dvar builtins.
 * Only VM argument/result plumbing and engine infrastructure are fixtures. */
#include "cvar.h"
#include "scr_vm.h"
#include "cscr_variable.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void GScr_GetCvarFloat(void);
void GScr_GetCvarInt(void);
static float vm_float;
static int vm_int;
int Scr_GetNumParam(void) { return 1; }
char *Scr_GetString(unsigned index) { assert(index == 0); return "scr_xpscale"; }
unsigned Scr_GetType(unsigned index) { (void)index; assert(0); return 0; }
int Scr_GetInt(unsigned index) { (void)index; assert(0); return 0; }
float Scr_GetFloat(unsigned index) { (void)index; assert(0); return 0; }
void Scr_AddFloat(float value) { vm_float = value; }
void Scr_AddInt(int value) { vm_int = value; }
void Scr_Error(const char *message) { fprintf(stderr, "%s\n", message); abort(); }
void Sys_EnterCriticalSection(int section) { (void)section; }
void Sys_LeaveCriticalSection(int section) { (void)section; }
char *CopyString(const char *text) { return strdup(text); }
void FreeString(char *text) { free(text); }
int Q_vsnprintf(char *dest, size_t size, const char *format, va_list args)
{ return vsnprintf(dest, size, format, args); }
void Com_Printf(conChannel_t channel, const char *format, ...) { (void)channel; (void)format; }
void Com_DPrintf(conChannel_t channel, const char *format, ...) { (void)channel; (void)format; }
void Com_Error(int level, const char *format, ...)
{ (void)level; va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args); abort(); }

static void require_source(const char *path, const char *text)
{
    FILE *file = fopen(path, "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); long size = ftell(file); assert(size >= 0);
    rewind(file); char *source = calloc((size_t)size + 1, 1); assert(source);
    assert(fread(source, 1, (size_t)size, file) == (size_t)size); fclose(file);
    if (!strstr(source, text)) { fprintf(stderr, "%s lacks %s\n", path, text); abort(); }
    free(source);
}

int main(void)
{
    /* Guard both production endpoints, so changing either back to integer
     * reading/registration fails even though this fixture uses a float. */
    require_source("src/sv_main.c", "Cvar_RegisterFloat(\"scr_xpscale\", 1.0f, 0.0f, 4.0f");
    require_source("bin/home/codol_rawfiles/maps/mp/gametypes/_rank.gsc",
                   "level.xpScale = getDvarFloat( \"scr_xpscale\" );");
    Cvar_Set2("scr_xpscale", "0.25", qfalse); /* Console set before native registration. */
    cvar_t *scale = Cvar_RegisterFloat("scr_xpscale", 1.0f, 0.0f, 4.0f, 0, "Rank XP multiplier");
    assert(scale && scale->type == CVAR_FLOAT && scale->floatval == 0.25f);
    assert(scale->resetFloatval == 1.0f && scale->limits.fmin == 0.0f && scale->limits.fmax == 4.0f);
    GScr_GetCvarFloat(); assert(vm_float == 0.25f);
    GScr_GetCvarInt(); assert(vm_int == 0); /* Original failure mechanism. */
    assert(vm_float > 0); /* syncXPStat's scale gate stays open. */
    Cvar_Set("scr_xpscale", "1"); GScr_GetCvarFloat(); assert(vm_float == 1.0f);
    Cvar_Set("scr_xpscale", "0"); GScr_GetCvarFloat(); assert(vm_float == 0);
    Cvar_Set("scr_xpscale", "0.25"); GScr_GetCvarFloat(); assert(vm_float == 0.25f);
    puts("PASS: 25% XP survives console set, native float registration and GSC float read; default/disabled rates preserved");
    return 0;
}
