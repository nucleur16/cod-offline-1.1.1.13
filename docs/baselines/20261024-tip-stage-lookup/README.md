# TipView Stage lookup baseline

The reviewed source changes Stage access in four existing `TipView` method
bodies to `CodoUIComponent.REAL_STAGE`. It preserves the original class and
method ABI, traits, scripts and eight other method bodies. It adds no fields,
methods, overrides or native pointer guards. The stored DoABC tag and patch
metadata are the exact inputs of the tested `voice_ui_mp.ff` candidate
`e5bd95086e93d87196ecc02a85209e76b93b17d6505f87e57d58c3aaa9b6373d`.

Run `python -B -m unittest discover -s tests -p test_tip_stage_lookup.py`.
The portable checks inspect actual stored class metadata and execute the actual
AS method bodies against Stage/Tween doubles, including a failing virtual
Stage getter. They are source/bytecode checks, not native GFx acceptance.

The parent observed e5 frontend startup and SendInvite without immediate crash
in English and Chinese. With QA21, EN1 to ZH2 delivered one invite and Accept
showed two-member rosters on both clients. ZH2 to EN3 delivered one invite,
Decline cleared it, Resend delivered one again, and Accept showed the
two-member roster. These are bounded actual client observations, not a complete
party/lobby acceptance matrix or a proof of the original native crash cause.

A separate right-click on the client's own party row crashed English client1
at03:57:19 UTC. It remains unresolved. No right-click fix is included here.
The earlier851 candidate failed frontend startup in both languages and is
preserved as rejected evidence; its class-layout changes are not used.

Compiled ABI/body/inventory proof is retained in the external qualification
folder `party-tooltip-body-only-20261024/candidate-build-1792813022191623300`.
The required shared UI gate pins the source, stored tag and patch metadata.
The candidate is common to both languages; no language-specific movie is added.
