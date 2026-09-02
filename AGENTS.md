# AGENTS.md

## What this is

- X11 compositor (xcompmgr/compton lineage), single binary `fastcompmgr`. Plain C99 Makefile build, no build-system beyond that.
- `fastcompmgr.c` (~2800 lines) is the event loop + painting; the `cm-*.c` files are support modules. `ringbuffer.h` is a header-only macro ring buffer.
- The project's selling point is performance: occluded windows are skipped via ignore-region culling, X requests are pipelined, and the per-frame paint path must not allocate. Don't regress this.

## Build & test

- `make` builds only the `fastcompmgr` binary.
- `make test` builds and runs the deterministic (no X server) unit suite: `test_timing_buffer`, `test_comp_rect`, `test_win_hash`, `test_pipeline`. It does NOT build/verify the binary — run both: `make && make test`.
- `make test-integration` runs the real `fastcompmgr` binary under `xvfb-run` and hammers it with `test_helper_win`, verifying the compositor survives and holds the `_NET_WM_CM_S<n>` selection. It skips cleanly (exit 0) if `xvfb-run` is unavailable. Requires `make` and `make test_helper_win` first.
- No CI exists (`.github` has only funding/issue templates): `make test` is the primary automated check. Run it after touching `cm-event.c`, `ringbuffer.h`, `cm-util.h`, `comp_rect.c`, or `cm-window.c`.
- Tests that reach static internals `#include` the source directly (e.g. `test_win_hash.c`/`test_pipeline.c` include `cm-window.c` + `cm-event.c` through `test_support.h`, which defines the globals those TUs need; include it from exactly one TU per binary). New tests should follow the same pattern where they need internals.

## X request / error conventions

- Any X request whose error events must be tolerated is preceded by `set_ignore(dpy, NextRequest(dpy));` (see history: "Fix asynchronous BadWindow errors causing lag"). When adding an X call in the event loop or paint path, bracket it the same way — omitting it surfaces spurious BadWindow errors that stall the compositor.
- Resolve every atom once at startup (`main()` in `fastcompmgr.c`, incl. `root_background_props_init()`), then compare `Atom` values as integers. Never call `XInternAtom(...)` inside an event handler — it is a round trip per event.

## Window bookkeeping

- `win *list` (`cm-window.c`) is a singly-linked list in stacking order; `paint_all` depends on that order. List head = TOPMOST window (`add_win` inserts before `prev`).
- `find_win()` is O(1) via a growable hash map in `cm-window.c` (starts at 64 slots, doubles at load ≥ 0.5; key 0 = empty, `(Window)-1` = tombstone). Keep insert/remove in sync when touching window lifecycle: call `win_hash_insert()` after linking a new `win` into `list` (`add_win`), and `win_hash_remove()` before `free()` (`finish_destroy_win`). Don't scan `list` to look windows up.

## Hot-path rules

- `paint_all` / `do_paint` / the event loop are per-frame hot paths — no allocations, no blocking calls there beyond the deliberate `XSync`.
- `do_paint` MUST call `XSync(dpy, False)` after every `paint_all`. It is the frame-pacing mechanism: with no other pacing (the loop repaints on demand), removing or throttling it lets frames queue up on the X connection, the server falls behind, and on-screen video lags/stutters. Verified regression: throttling to every 4th frame caused visible lag.
- Occlusion culling lives in `comp_rect.c` (`rect_paint_needed`): the intersection must use `x2`/`y2` on both branches. A regression here silently destroys the culling benefit.

## Gotchas

- Fading is intentionally broken (README says so); don't spend effort on it.
- `test_timing_buffer.c` `#include "cm-event.c"` directly to reach static internals (and `#define`s `False`/`True` itself), so edits to `set_ignore`/`discard_ignore` are covered by this test.
- Build flags include `-march=native -flto`; the timing tests are millisecond-resolution and pass under them.

## TDD workflow

Every change — bug fix, feature, or refactor — starts with a failing test, then the fix that makes it pass. This is what makes `make test` able to catch regressions: if a test can't fail, it's not guarding anything.

- **Write the failing test first** against the real source (via `test_support.h` `#include` for internals, or against `comp_rect.c` for the culling kernel). Run it, confirm it fails for the reason you're about to fix, then make it pass.
- **Wire new tests into the Makefile**: add the binary to `TESTS` (build rule + `clean` + `.gitignore`). If it needs no X server, it belongs in the `make test` unit suite; if it must exercise the running compositor, add it under `test-integration`.
- **Verify the guard actually trips**: after the test passes, temporarily restore the bug and confirm the test fails again.
- **Run the full suite before finishing**: `make && make test && make test-integration`. If a test is flaky or the model it asserts is wrong, fix the test's expectations to match the documented behavior rather than deleting coverage.
- `rect_paint_needed` tracks a **single** rect — the largest single window/intersect seen so far (strict `>` keeps the first on ties) — not a union accumulator. Tests (and culling reasoning) must model it that way, or they fail.
- The `list` head is the TOPMOST window and culling iterates top→bottom; occlusion tests must build stacking order with that orientation (`add_win(dpy, id, prev)` inserts before `prev`).