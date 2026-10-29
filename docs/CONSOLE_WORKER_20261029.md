# Console raster worker

The user reported frame lag while the console was open and requested a separate
thread. Before this change, every dirty console Present created/resized GDI
resources, cleared the framebuffer-sized DIB, converted/drew text, flushed GDI,
and scanned every painted pixel to produce alpha. It also took the log lock
for a snapshot and could write the k33shot BMP on the render thread. Captured
log messages dirtied even a collapsed console with no visible new result.

`console.c` now starts one process-lifetime raster worker. It owns the console
GDI DC, bitmap/font, UTF/GBK conversion and glyph rasterization, alpha pass,
history load/save, and console BMP writes. Requests coalesce at a maximum of one
raster every16ms, with bounds of8192 per axis and16M pixels. Allocation failure
retries without blocking Present. DIB/font replacement deselects old objects
before deleting them; the offscreen resize test confirms stable GDI handles.

Input editing, completion, command execution and traversal of actual C1 dvars
remain on their existing window thread. Opening an empty prompt skips C1's
reader lock and full dvar traversal. A separate UI lock protects the copied
input/completion/history state. The worker uses try-locks to copy at most160
log lines,24 suggestions,5 details and12 result lines, then releases both locks
before GDI, allocation or disk I/O. It never reads game pointers. Log capture
keeps its existing short ring-buffer lock, without waiting behind raster work;
unrelated collapsed output no longer requests fresh pixels. Explicit user
commands such as condump retain their existing execution semantics.

`console_mailbox.h` supplies two bounded CPU frame slots. Full-fence atomic
ownership changes prevent a producer from resizing or overwriting pixels held
by a reader. A busy consumer causes the producer to reclaim an unconsumed READY
frame or retry later. Present never waits, snapshots, rasterizes or performs
file I/O: it claims completed matching-size frames, uploads pixels and draws
using the existing D3D device thread. A serial check prevents an older slot from
replacing a newer texture. Only the newest matching frame is uploaded, at most once per Present.
First open/resize draws no uninitialized texture while
the worker prepares the first matching frame. D3D texture upload, state capture
and the quad remain on their owner thread; these device costs are not claimed
to run asynchronously.

Validation performed without window/input or changes to active C1/server:

- Shipping `K33_NO_COPY=1` build succeeded; no installation copy.
- `python -m unittest discover -s tests -p test_console_worker.py`:5 architecture
  checks covering render ownership, I/O, bounded snapshots and the empty prompt.
- Production mailbox C test:100000 publications with concurrent slow consumer,
  resizing/reclamation, occupied-slot refusal and stable pixel/metadata checks.
  Actual Windows run consumed17351 frames; Linux ASan+UBSan run consumed28650.
  Counts depend on scheduling; unconsumed frames are intentionally dropped.
- `python tests/console_raster_smoke.py` extracts and compiles the actual
  production GDI raster functions, then renders40 collapsed/expanded frames
  at720p/1080p with resizing, text/colour and alpha checks on a separate worker.
  A forced CreateDIBSection failure clears stale pointers/dimensions; returning
  to the old resolution then reallocates successfully. Latest run78ms total;
  GDI object count stabilized at5 after warmup. This
  isolated measurement is not an in-game frame-rate result.

- `python tests/console_draw_smoke.py` executes the actual production Present
  consumer with a fake D3D device: two READY slots in either serial order cause
  one upload of the newest; a newer wrong-size frame is discarded. It verifies
  held slots return FREE. This is a consumer-body test, not live device timing.

Final shipping DLL:12940288 bytes, SHA256
`F3FA3B0BD26AAA6CC8022A313B3CA4AD399F82C08C24F8C337C8B1EF1C85D92F`.

Live C1 console responsiveness remains an acceptance check after the parent's
review/deployment. No client/server restart, cursor/focus/keyboard input or
worker commit is part of this change.
