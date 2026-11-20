---
slug: gsc-builtins
title: Working with GSC builtins
space: reference
summary: Functions versus methods, the developer flag, how a name is bound to native code, and what the reference can and cannot tell you about arguments.
position: 20
tags: [script, wiki]
---

There are **881 registered GSC names** in this server. The
[builtin browser](/reference/gsc) lists all of them; this page explains what the
columns mean.

## Functions and methods

```gsc
ent = getent( "targetname", "key" );   // function: called bare
player giveWeapon( "ak47_mp" );        // method: called on a receiver
```

The distinction is fixed at registration:

```c
Scr_AddFunction("getent", GScr_GetEnt, 0);          /* xfunction_t */
Scr_AddMethod("giveweapon", PlayerCmd_GiveWeapon, 0); /* xmethod_t: takes scr_entref_t */
```

A method receives `scr_entref_t { entnum, classnum }` — the receiver — as its
only C argument. A function receives nothing. Both read their script arguments
off the VM stack.

The same *name* can be registered both ways. When that happens the builtin page
lists every registration site, because which one the VM resolves depends on
which registration ran last.

## The developer flag

The third argument to `Scr_AddFunction` is the developer-only flag. A builtin
registered with it set exists only when the server runs with `developer 1`.

The launcher sets `developer 1` by default, which is convenient and also a trap:
a script that works on your machine can fail on a server started with
`CODOL_DEVELOPER=0`. The browser marks these with a `dev` badge, and you can
[list them](/reference/gsc).

## Binding

Every builtin page shows three locations, and they are usually three different
files:

| | |
|---|---|
| **Registered at** | The `Scr_AddFunction` call. For nearly everything that is `src/scr_vm_main.c`. |
| **Bound symbol** | The native function name passed to it. |
| **Defined at** | Where that function is actually implemented. |

The third is the interesting one. **398 builtins are implemented in retained x86
assembly** — their "defined at" is a label in `src/asmsource/`, and there is no C
source to read. See [Retained x86 assembly](/wiki/retained-assembly).

## Arguments are not in the signature

A builtin's native signature is `void f(void)` or `void f(scr_entref_t)`. The
script-visible argument contract lives in the body:

```c
void CODOL_Scr_Log(void) { Scr_AddFloat(logf(Scr_GetFloat(0))); }
```

So the reference can tell you a builtin exists, how it is called, where it lives
and what its doc comment says — but for the argument list you have to read the
body, or a doc comment that spells it out. Many CoDOL builtins do:

> `magicbullet(<weaponName>, <start>, <end>[, <attacker>])`. Function (not a
> method).

See [the GSC virtual machine](/wiki/script-vm) for the full accessor list
(`Scr_GetInt`, `Scr_GetString`, `Scr_GetEntity`, …) and the return pushers
(`Scr_AddInt`, `Scr_AddVector`, …).

## Where a builtin lives

Builtins are filed under the area of the file that *defines* them, not the file
that registers them:

| Area | What you will find |
|---|---|
| [`script`](/reference/gsc?area=script) | The CoDOL builtin surface — `codol_script_<topic>.c`. |
| [`asmsource`](/reference/gsc?area=asmsource) | Retained IW3 builtins, in assembly. |
| [`engine`](/reference/gsc?area=engine) | CoD4x builtins still implemented in C. |
| `player`, `weapon`, `stats`, … | Builtins defined next to the subsystem they act on. |

## Adding one

1. Write the implementation in the area it belongs to, as
   `void CODOL_Scr_<Name>(void)` or `void CODOL_Scr_<Name>(scr_entref_t)`.
2. Declare it in `src/scr_vm_functions.h`.
3. Register it in `src/scr_vm_main.c` with `Scr_AddFunction` or
   `Scr_AddMethod`, lowercase name, developer flag `0` unless it really is a
   debugging aid.
4. Write a doc comment above the implementation that states the script
   signature. It becomes the description on the builtin's wiki page.
5. Re-run `python -m app.cli index`.
