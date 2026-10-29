# AS3 local arithmetic wordcode correction

The Quit button's failure was an access violation, not an accepted quit request.
Read-only analysis used a completed local full crash dump; no second debugger
attached to the live client. The dump remains local and is not a Git artifact. The captured movie SHA256 is `9631a3ee26f388de2d51798db2863d44eca3c9e80965980bf6f8c3b52a436cc5`.

The VM at 0x38CD76F0 has 17 CallFrames: size at VM+0x84, page-pointer array
at VM+0x90 (0x3A6BF8A0), first page0x3A6C7380. Each frame is 0x48 bytes.
The separate array at VM+0x48 is the ScopeStack, not the CallStack. CallFrame
file+0x14 and method-body-index+0x18 identify the executed ABC; live interned
string pools match the captured movie's actual DoABC constant pools. The full
minimal original ABC/runtime-wordcode fault receipt is retained in
`docs/baselines/20261029-gfx-local-operands/crash_repro.json`. The full
resolved call stack is retained in
`docs/baselines/20261029-gfx-local-operands/as3_callframes.json`.

Routing was correct: BaseButton click -> MenuPanelView ->
MainMenuController.onGotoQuitHandle -> BaseController.gotoPage ->
PageNavigationController.gotoPageByPageObj -> PageManagerMain.showPage ->
BaseView activation -> SummaryView.enableData -> summaryData setter ->
SummaryView.hideAllAchievementInfo (method body 15). No quit command had been
issued. The final helper loops over the three authored achievement widgets.

The original ABC ends in valid bytes `92 01 D1 24 03 15 C4 FF FF 47`:
`inclocal 1; getlocal1; pushbyte 3; iflt ...; returnvoid`. However its generated
32-bit wordcode at 0x3B5A02FC starts `92 D1 24 03 15 ...`, omitting operand 1.
The native executor at 0x93E930 reads0xD1 (the following getlocal1 opcode) as
register index 209. With register base0x38CBEC88, it reads Value 0x38CBF998,
whose kind 9/payload0xB74510 is unrelated to the local loop counter. Numeric
conversion treats the payload as an object, ultimately faulting at 0x9155CD.
This is a native producer/consumer contract violation; source-level event tests
cannot reproduce it with JavaScript's ordinary numeric loop semantics.

Primary C1 code identifies both defective generic producers:

| Operation | Producer | Original emitter call | Original conversion call |
| --- | --- | --- | --- |
| inclocal 0x92 | 0x9117A0 | 0x9117B0 -> 0x9077D0 | 0x9117E7 -> 0x907E30 |
| declocal 0x94 | 0x911830 | 0x911840 -> 0x9077D0 | 0x911877 -> 0x907E30 |

The caller at 0x937F4F/0x937F97 already decodes the correct U30 operand. The
chosen overload0x9077D0 writes only the opcode; overload0x907870 writes opcode
and operand. Integer producer0x911CD0 uses the latter correctly. Opcode-info
entries at 0xB74310 +2*opcode are already `11 00` for92/94/C2/C3: operandCount 1.
Changing metadata would not fix the explicit one-word emitter.

SDK4.3 `AS3_Tracer.cpp` 1202/1213 shows the same problematic generic-emission
shape; its PushNewOpCode overloads at 9220/9235 distinguish one-word versus
two-word output. SDK4.2 uses a different outer-emission arrangement, with
exec_inclocal/declocal in `AS3_Tracer.h` 685/692 updating abstract types only.
These references support the intended contract; the actual C1 instructions and
dump establish the defect and selected repair addresses.

The parent's policy inserts `FF 75 08` (push decoded register operand) into each
0x90-byte native producer, consuming three of its six trailing padding bytes.
It preserves the original entry and every abstract-type/refcount instruction.
Only three relocated E8 calls are rebased: new offsets 0x13 -> 0x907870,
0x4A -> 0x907E30 and0x82 -> 0x9058D0. Internal short branches move with their
destinations. Full-span FNV source guards are 4F98EE89/C9003EE5. There is no
trampoline, custom numeric implementation, AS-loop rewrite or exception bypass.

`python -m unittest discover -s tests -p test_gfx_local_operands.py` passes:

- The actual shipping policy compiles into a C driver. Every one-bit byte
  mutation across both source spans is refused without writing the output.
  Wrong addresses/sizes, null/same buffers and already-patched spans are refused.
- The resulting exact policy bytes execute in Unicorn at the original native
  entrypoints. Both original and fixed producer bodies run actual C1 stack-size
  accounting and native word-array writers for registers 1/127/128/4095.
  Original output omits every operand; fixed output is always[opcode,operand].
- Twelve additional original/fixed cases execute retained native cleanup for
  shared-reference decrement, last-reference virtual free, and strong-string
  reference decrement. Stack balance, nonvolatile registers and the converted
  register address remain identical. Only abstract-type assignment and the final
  external heap-free target are mocked; actual cleanup/wordcode emission run.

The 28 body-case results are retained in
`docs/baselines/20261029-gfx-local-operands/policy_producer_proof.json`.
These are native instruction and dump tests, not live popup acceptance. The
parent owns the runtime integration/build/deployment and subsequent live test.

Parent integration: both complete producer spans and five native helper/caller guards are checked before either write. The correction installs before the C1 game entry. Shipping profile (LOCAL_TESTING=0, DIAGNOSTICS=0) built DLL SHA256 `06374ACA4052059809260A245AD635A5F7A1E17D41B1DA8BF92FE7CF8AC8A347`, retaining the console worker and all previous fixes. The native files are pinned in ui_baseline.json; shared baseline and prior quit routing regressions also pass.
