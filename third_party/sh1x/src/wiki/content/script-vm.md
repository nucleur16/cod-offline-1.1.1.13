---
slug: script-vm
title: The GSC virtual machine
space: architecture
summary: How a .gsc file becomes running threads — compile, variables, the stack-based builtin calling convention, notify and endon.
position: 30
tags: [script, engine]
---

Game Script Code is the language the gametype and map logic are written in. The
virtual machine that runs it is retained CoD4x code in `src/cscr_*.cpp` and
`src/scr_vm_*.c`, and it is one of the parts of the engine that survived the
CoDOL port largely intact — the *scripts* changed, the VM mostly did not.

## The files

| File | Role |
|---|---|
| `cscr_parser.cpp`, `cscr_compiler.cpp` | Parse and compile GSC to bytecode. |
| `cscr_vm.cpp` | The interpreter. |
| `cscr_variable.cpp` | The variable store: objects, arrays, fields, refcounts. |
| `cscr_stringlist.cpp` | The string table behind `VAR_STRING` and `VAR_ISTRING`. |
| `cscr_memorytree.cpp` | `MT_Alloc` / `MT_Free`: the script heap. |
| `cscr_animtree.cpp` | Animation trees referenced from script. |
| `scr_vm_main.c` | Registers every stock builtin and drives script loading. |
| `scr_vm_cmd.c` | `Scr_AddFunction` / `Scr_AddMethod` and the argument accessors. |

## Loading

`GScr_LoadScripts` loads the gametype script and then `maps/mp/<mapname>`.
Before a buffer reaches the parser it passes through a precompile hook, which is
where CoDOL's [syntax lowering](/wiki/subsystem-script) happens:

```c
void Scr_ScriptPreCompile(void *scr_buffer_handle, const char *filepath)
{
#ifdef CODOL_SERVER
    CODOL_PrecompileScript((char *)scr_buffer_handle, filepath);
#endif
}
```

Because the lowering passes preserve byte offsets, compiler error positions
still point at the line the author wrote.

## Values

Every script value is a `VariableValue`: a type tag plus a union. The tags are
the vocabulary of the whole VM, and several of them are types the language never
names directly:

| Tag | |
|---|---|
| `VAR_UNDEFINED` `VAR_STRING` `VAR_ISTRING` `VAR_VECTOR` `VAR_FLOAT` `VAR_INTEGER` | The ordinary value types. `VAR_ISTRING` is a *localized* string. |
| `VAR_POINTER` `VAR_OBJECT` `VAR_ARRAY` `VAR_ENTITY` `VAR_DEAD_ENTITY` | References. An entity that was freed becomes `VAR_DEAD_ENTITY` rather than dangling. |
| `VAR_CODEPOS` `VAR_PRECODEPOS` `VAR_FUNCTION` `VAR_DEVELOPER_CODEPOS` `VAR_INCLUDE_CODEPOS` | Code positions, including the `::name` function pointers. |
| `VAR_THREAD` `VAR_NOTIFY_THREAD` `VAR_TIME_THREAD` `VAR_CHILD_THREAD` `VAR_DEAD_THREAD` | The four kinds of thread, by what they are waiting on. |
| `VAR_THREAD_LIST` `VAR_ENDON_LIST` | The waiter lists behind `notify` and `endon`. |

Strings are interned in the string list, so comparing two `VAR_STRING` values is
a comparison of indices, not of bytes.

## Calling convention for builtins

A builtin takes no C arguments. It reads them off the VM stack by position and
pushes its result back:

```c
typedef void (*xfunction_t)();                       /* name(...)        */
typedef void (*xmethod_t)(scr_entref_t EntRef_);     /* self name(...)   */

typedef struct { uint16_t entnum; uint16_t classnum; } scr_entref_t;
```

| Reading arguments | Returning |
|---|---|
| `Scr_GetNumParam()` | `Scr_AddInt`, `Scr_AddFloat`, `Scr_AddBool` |
| `Scr_GetInt`, `Scr_GetFloat`, `Scr_GetString`, `Scr_GetIString` | `Scr_AddString`, `Scr_AddVector` |
| `Scr_GetVector`, `Scr_GetEntity`, `Scr_GetObject`, `Scr_GetFunc` | `Scr_AddEntity`, `Scr_AddEntityNum` |
| `Scr_GetType`, `Scr_GetConstString` | `Scr_AddArray` and the indexed array helpers |

A whole builtin can therefore be one line:

```c
void CODOL_Scr_Log(void)   { Scr_AddFloat(logf(Scr_GetFloat(0))); }
void CODOL_Scr_IsVec(void) { Scr_AddBool(Scr_GetType(0) == VAR_VECTOR); }
```

This is also why the wiki's builtin pages show a native signature of
`void f(void)` and say so explicitly: the *script-visible* argument contract
lives in the body, not in the C prototype.

## Registration

```c
qboolean Scr_AddFunction(const char *cmd_name, xfunction_t function, qboolean developer);
qboolean Scr_AddMethod(const char *cmd_name, xmethod_t method, qboolean developer);
```

Names are lowercase; GSC is case-insensitive for builtin names. The third
argument is the **developer-only flag**: a builtin registered with it set is
only reachable when the server runs with `developer 1` — which the launcher does
by default, so a script that works locally can fail on a quiet server.

All stock registration happens in `Scr_AddStockFunctions` and
`Scr_AddStockMethods` in `src/scr_vm_main.c`, which is a single flat list of
~870 calls. CoDOL builtins are registered in the same place alongside the IW3
ones, which is why the wiki resolves a builtin's *area* from where its binding
is **defined** rather than where it is registered.

Browse them all: [GSC builtins](/reference/gsc).

## Threads, notify and endon

```c
void __regparm3 VM_Notify(int notifyListOwner, int stringValue, VariableValue *value);
```

A `notify` walks the owner's thread list and wakes the threads waiting on that
string. `endon` is the inverse subscription: a thread registers itself on an
endon list, and the notify kills it instead of resuming it.

That is exactly the machinery `childthread` had to extend — a child thread
inherits the `endon` subscriptions of its calling frame chain, which is why
[`Scr_ChildThread_Start`](/wiki/subsystem-script) exists rather than just
reusing the thread opcode.

## Memory

The script heap is a memory tree (`MT_Alloc`, `MT_Free`, `MT_AllocIndex`), with
indices rather than pointers used where a reference must survive a reallocation.
`MT_DumpTree` prints its state, which is the first thing to reach for when a
script leaks.

## Where CoDOL touches the VM

- **Precompile.** `CODOL_PrecompileScript` runs the syntax-lowering and
  `childthread` passes on the buffer before the parser sees it.
- **Source selection.** `CODOL_ReadScriptSource*` decides which of the three
  script roots a file comes from.
- **Builtins.** Roughly 880 names are registered; the ones that are not from
  IW3 live under `src/codol/script/`.
- **Function-pointer resolution.** `CODOL_SetScriptBuiltinResolver` lets `::name`
  resolve to a registered builtin.
