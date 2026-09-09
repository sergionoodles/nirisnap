# Nirisnap — Agent Guide

Nirisnap is a Niri-focused fork of [Omasnap](https://github.com/omacom/omasnap)
(pinned at `d339588f3aba554f314d6233da2dddc98f83de55`, v1.20.1). It keeps
Omasnap's fast native Wayland screenshot and annotation overlay (region and
full-monitor capture, then vector-layer annotation: arrows, lines, freehand,
highlighter, rectangles, ellipses, numbered markers, text, OCR) and replaces
its Hyprland/Omarchy integration with Niri JSON IPC + `wlr-screencopy`.
Finished captures go to clipboard, `~/Pictures/Screenshots`, or a pinned
always-on-top layer surface.

Fork point and origin are recorded in `README.md`. Original MIT
license/copyright and bundled font licenses are preserved (`LICENSE`,
`assets/*-OFL.txt`, `assets/FONTS.md`).

## Project principles

Each of these has a longer writeup under `docs/` — read it before making a
change that touches the principle, not just this summary. Editor/threading
principles are inherited unchanged from Omasnap; platform principles target
Niri.

- **A specialized tool, not a general app.** Nirisnap does one job — capture,
  annotate, output — and does it fast. It is not a drawing program, not a
  file manager, not a general Wayland utility. A feature that isn't in
  service of "screenshot, mark it up, send it somewhere" doesn't belong
  here, however useful it might be on its own.
- **The main thread never blocks.** Capture, paint, and input handling are
  the UI thread's whole job. Disk I/O on a full-resolution image, spawning
  a process, PNG encoding — all of it runs on a worker via
  `QtConcurrent`/`QFutureWatcher`, never inline. Niri discovery
  (`niri msg -j`) is also off the UI thread except the fast focused-output
  probe used for overlay placement. See
  [docs/threading.md](docs/threading.md).
- **Every operation is undoable.** The operation log is the source of
  truth; the visible image is rebuilt from it. Rendering for editing is a
  pure, repeatable function of that log — nothing is baked into the working
  image as you draw. Output is applied only on **Copy**, **Save**, or both,
  which is the one moment a flattened image is produced. Redaction is the
  deliberate, documented exception: it must actually destroy pixels at
  render time so nothing recoverable leaks into an export, while remaining
  a normal, undoable log entry until you export. See
  [docs/editing-model.md](docs/editing-model.md).
- **Minimally configurable — pre-configured to be right.**
  No settings UI, no wizards, no onboarding. The defaults are the product;
  a config key is a narrow escape hatch for a real divergent need (where to
  save, what to name it, preset colors), never a general mechanism. Adding
  a new key needs the same justification the existing ones had, not "this
  would be nice to expose." Config lives at
  `~/.config/nirisnap/nirisnap.conf`; no automatic Omasnap migration.
- **Speed first.** Instant capture, annotate, copy. No startup bloat.
- **Wayland only, Niri only.** Monitor discovery (`niri msg -j`
  focused-output/outputs/workspaces), pixel capture (`wlr-screencopy`),
  input quirks, and notification conventions are all Niri-specific on
  purpose. Code that happens to also run on another wlroots compositor,
  because it stands on a real Wayland protocol rather than a Niri shortcut,
  is a fine accident — it is not a target, not tested, and not a bug magnet
  we chase. No X11, no macOS/Windows. See
  [docs/platform-scope.md](docs/platform-scope.md).
- **Lean, learned dependencies.** The dependency set is Qt6 + LayerShellQt +
  wayland-client, plus shelling out to a few existing tools
  (`niri`, `wl-copy`/`wl-paste`, `tesseract`, `notify-send`)
  instead of linking their equivalents in-process. Know this list before
  proposing an addition to it. See [docs/dependencies.md](docs/dependencies.md).
- **Single small binary.** Everything (capture, editor, pin mode, scroll
  capture scaffolding) runs from the one `nirisnap` executable. Every new
  dependency or vendored asset is weight every install carries.
- **No backwards compatibility.** Break keybindings, CLI flags, file
  formats, or internals whenever it keeps the code simpler or the tool
  faster. Do not add compatibility shims, deprecation aliases, or migration
  code. In particular there is no `omasnap.conf` migration and no
  `OMASNAP_*`/`OMARCHY_*` aliases: the new names are `nirisnap.conf` and
  `NIRISNAP_*`.
- **Minimal chrome.** `notify-send` when available (best-effort; notification
  failure never fails a save), `NIRISNAP_OCR_LANGS` for OCR languages,
  minimal vector-drawn icons (no icon-theme dependency), the bundled Neucha
  font. Chrome text uses `chromeFont()`/`chromeMonoFont()`
  (`src/overlay-chrome.cpp`), pinned in code, and `main()` installs
  `chromeDefaultFont()` as the application font; external desktop platform
  themes are deliberately bypassed at startup in favour of Qt's built-in
  `generic` theme (see [docs/dependencies.md](docs/dependencies.md)). Chrome
  must use the pinned fonts and explicit colours; do not derive chrome from
  `QFontDatabase::systemFont`, `QStyle`, or `palette()`.

## Repository layout

| Path | Purpose |
|---|---|
| `src/main.cpp` | CLI parsing, single-instance lock, mode dispatch |
| `src/instance-lock.cpp/.hpp` | Single-instance handover: cancel a running overlay, or stop it and take over for `--file` |
| `src/niri-ipc.cpp/.hpp` | Niri metadata: `focused-output`/`outputs`/`workspaces` parsing, monitor mapping |
| `src/capture.cpp/.hpp` | Capture, render pipeline, output (clipboard/save/notify), source+JSON operation-log persistence, config loading glue |
| `src/editor.cpp/.hpp` | Annotation editor: tools, vector layers, operation-log undo/redo, the select↔edit phase machine, export |
| `src/overlay-chrome.cpp/.hpp` | Shared chrome every overlay wears: the capture-kind tab strip, hotkey legend, status pill |
| `src/scroll-capture.cpp/.hpp` | The scroll-capture panel: region-live page, manual/auto mode, grips, stitched result (manual first; auto gated) |
| `src/scroll-inject.cpp/.hpp` | Auto-scroll wheel injection (`zwlr_virtual_pointer_v1`; uinput disabled until a Niri scroll policy is queryable) |
| `src/auto-capture.cpp/.hpp`, `src/stitch.cpp/.hpp` | Pure, offline-testable frame classification and stitching |
| `src/stitch-replay.cpp` | Standalone tool: replay a dumped frame directory through the stitcher with no compositor |
| `src/surface-capture.cpp` | In-process output capture via `wlr-screencopy` (SHM-only, cursor-free, y-invert aware) |
| `src/cut.cpp/.hpp` | Cut-band tool: remove a strip and collapse the gap |
| `src/recent-snaps.cpp/.hpp` | The recents shelf: shelving/reopening working documents |
| `src/output-config.cpp/.hpp`, `src/palette-config.cpp/.hpp` | The optional `nirisnap.conf` INI: output destination/filename, color presets |
| `src/pin.cpp/.hpp`, `src/pin-file.cpp/.hpp`, `src/pin-layout.cpp/.hpp` | Pinned-capture layer-shell surfaces (bottom-right, all workspaces) |
| `src/icons.cpp/.hpp` | Vector icon renderer for toolbar and pin controls |
| `src/cli-path.cpp/.hpp` | Command-line image target resolution |
| `src/eyedropper.cpp/.hpp` | Display-to-source color sampling |
| `tests/*-smoke.cpp/.hpp` | Headless Qt Test coverage: offscreen region clicks, async capture, single-instance handover, stitching fixtures, Niri IPC fixtures |
| `tests/fixtures/niri-*.json` | Sanitized Niri IPC samples (missing tiled positions, disabled outputs) |
| `docs/` | Longer writeups of the principles above — read before changing behavior they cover |
| `docs/capability-report.md` | Verified local session: Wayland globals, Niri version, outputs |
| `docs/niri-setup.md` | Niri binds example (`niri validate`), layer rules, screenshot exclusion |
| `docs/window-capture.md` | Window-capture product decision and clipboard constraints |
| `CMakeLists.txt` | Build definition; **the version lives here** (`project(nirisnap VERSION ...)`) |

## Build and verify

```bash
make check
```

`make check` configures and builds the project, runs the complete headless
offscreen Qt smoke suite (including simulated region clicks and asynchronous
capture), runs `clang-tidy`, and runs `clazy-standalone`/`qmllint` when those
tools and source types are available. Use `make build` for a build-only pass,
`make smoke` for the behavioral smoke suite, and `make install` to install to
`~/.local`.

Always run `make check` after behavioral changes. CI
(`.github/workflows/build-linux.yml`) runs the same build and smoke on every
push and PR.

Dependencies (Arch): `base-devel cmake ninja pkgconf qt6-base qt6-wayland
layer-shell-qt wayland wayland-protocols wl-clipboard tesseract
tesseract-data-eng`. See [docs/dependencies.md](docs/dependencies.md) before
adding to this list. `niri` must be running for live capture; `notify-send`
(libnotify) is optional best-effort.

## Release process

1. Bump `project(nirisnap VERSION ...)` in `CMakeLists.txt`.
2. Build and run the smoke test (above).
3. Commit, tag `v<version>`, push main and the tag. The GitHub workflow
   attaches the build artifact to the release automatically.

See `README.md` for user-facing features, keybindings, and install
instructions — keep it in sync when behavior changes.
