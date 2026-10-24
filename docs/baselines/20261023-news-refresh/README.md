# Same-process news refresh baseline

This fixture checks the root `news_refresh.c`, `news_refresh_policy.h` and `news_refresh_guards.h`. It compiles the actual policy and executes the original C1 x86 news dispatcher, request setup, body buffering/parser, consume, category publisher and cleanup in Unicorn. The native game executable is a required private input and is not included.

Run with Linux Python 3.9 or newer, GCC, and the pinned packages in `requirements.txt`. `i686-w64-mingw32-gcc` is optional for the Win32 compile-only check. No game process, account, backend service, browser, network request or live installation is used. Package installation is a separate environment preparation step.

```sh
python3 -m pip install -r docs/baselines/20261023-news-refresh/requirements.txt
export CODOL_NEWS_TEST_IMAGE="${OWNED_C1_IMAGE}"
python3 -B docs/baselines/20261023-news-refresh/run_regressions.py \
  --win32-cc i686-w64-mingw32-gcc
```

Set `OWNED_C1_IMAGE` to the image you own before running this example, or pass `--image IMAGE` directly. The only accepted image hashes are:

| Input | SHA-256 |
| --- | --- |
| Retained C1 code image | `d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60` |
| Shipping Kernel33-import executable | `a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976` |

Unknown images are rejected before native emulation. The 15 runtime guard regions must match the explicitly supplied image. No private image bytes, executable, captured traffic, SWF, object file or shared library belong in this baseline directory.

Generated libraries, objects, logs and JSON go to an ephemeral temporary directory by default. The runner prints the final JSON result. Use `--out EXTERNAL_DIRECTORY` to retain them outside the repository. `--source-root CLIENT_SOURCE` selects an explicit source root; by default the runner finds the client root relative to this directory. No current-working-directory convention or machine-specific path is required.

The positive contour includes 21 sequential updates per locale, fixed text-payload ownership, unchanged complete image nodes and bytes, failed/invalid/zero-body responses, native HTTP pending-task retirement and setup failure, the localized empty-news fallback, same-address reconstruction, initial native queue preservation, native cleanup, Chinese CRLF decoding, cyclic queues and clock wraparound. Three test-only builds remove refresh, old-text retirement or zero-body progress. Each must fail its corresponding regression for the intended reason. The runner also mutates all 2,651 guarded bytes individually and rejects each mutation.

The OS HTTP transport, CRT allocation/string helpers, logger/cookie, and GFx VM operations are explicit test doubles. Original native code executes around those boundaries; the harness does not prove real HTTP delivery, AVM behavior, task1480 reception by an actual movie, or visible same-process C1 acceptance. The unmanaged-string copy contract is supported separately by the exact publisher's stack-string use and the SDK reference described in [OWNERSHIP.md](OWNERSHIP.md). The test never invents production responses or changes runtime data.

`qualification.json` records the offline result and its proof boundary. Actual EN/ZH C1 refresh, a second update, localized no-news fallback and a match roundtrip remain separate acceptance evidence. No CI job requiring private game assets is added.
