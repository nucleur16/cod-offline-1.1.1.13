# Separate production script packages

`tools/script_profile.py` reads an immutable IWD and writes a new archive
and per-entry/per-edit receipt into a new directory. It never edits the active
rawfiles tree or an existing release. Development copies every entry unchanged.

Production removes developer blocks and standalone calls to println, print3d,
printstar and line. It first verifies that all four are uniquely registered with
the developer-only flag in the exact materialized native profile. Assertions,
errors, gameplay notifications and ordinary print/logprint are preserved.
Arguments to removed diagnostics are not evaluated. Non-script asset bytes,
navigation data and script line numbers remain unchanged.

Use `--input <pack> --output <new-directory> --profile production
--native-src <materialized-profile-src>`. Resolve the generated script tree
against that same profile and native binary, then qualify the exact archive in
the actual VM before installation. This transform alone does not establish a
performance improvement or real-client acceptance.

The signed package, scripts, client and overlay still need combined release
qualification. Never modify an immutable package used by an active match.
