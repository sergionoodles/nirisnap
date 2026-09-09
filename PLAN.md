# Nirisnap implementation plan

Status: proposed; implementation has not started. Researched on 2026-09-09.

Project root: `/home/sergio/workspace/nirisnap`. Keep the application directly in this directory, with `src/`, `tests/`, and `CMakeLists.txt` at its root.

## Recommendation

Create a Niri-focused fork of Omasnap, provisionally named **Nirisnap**. Keep its C++23 / Qt 6 / LayerShellQt editor and replace its compositor integration. Rewriting the editor in Rust, GTK, or another toolkit would add substantial work without solving the compositor limitations identified below.

Use native `wlr-screencopy` for monitor pixels and Niri JSON IPC for output/workspace metadata. Make region capture, full-monitor capture, and annotation the first usable release. Treat window capture as a separate milestone with explicit behavior choices: stock Niri currently cannot provide everything needed to reproduce Omasnap's window selection.

This should be a downstream fork, rather than a proposal to add multiple compositor backends upstream. Omasnap explicitly directs ports requiring additional compositor-specific paths to forks. Preserve its license and attribution, and update the fork's platform instructions to target Niri. [Upstream platform policy](https://github.com/omacom/omasnap/blob/d339588f3aba554f314d6233da2dddc98f83de55/docs/platform-scope.md)

## Research baseline

Inspected source revisions:

| Project | Revision | Relevant findings |
| --- | --- | --- |
| Omasnap | `d339588f3aba554f314d6233da2dddc98f83de55` | CMake project version 1.20.1; C++23; Qt >= 6.8; native Wayland capture; extensive editor smoke coverage. |
| Niri | `dd75865f547f0eac0e9b6c4d86d2cd00c0744252` | Current upstream source inspected for IPC, capture, layout, and input behavior. This is a research reference, not the installed build. |
| Local session | Niri `26.04 (8ed0da4)` | CLI and running compositor versions match; JSON IPC responds. |

The working directory was empty, with no Git repository or local `AGENTS.md`. Research checkouts were placed outside it in a temporary directory. Only this plan is being added now.

Local read-only checks found:

- CachyOS, an Arch-derived distribution.
- One enabled 2560×1440 output at scale 1; another output is disabled. Mixed scaling and rotation remain untested.
- Qt Core/Widgets 6.11.2, wayland-client 1.26.0, a C++ compiler, `wayland-scanner`, `wl-copy`, `wl-paste`, and `notify-send` are available.
- `cmake`, `ninja`, `tesseract`, `grim`, and `wayland-info` are absent from `PATH`; LayerShellQt and wayland-protocols were not resolved by the dependency probe. Verify package availability during setup.
- Sampled tiled windows have `layout.tile_pos_in_workspace_view = null` despite reporting sizes and column/row indices.

No application was built, screenshots taken, packages installed, or desktop configuration changed during planning. Live capture protocol globals still need to be enumerated.

## What can be reused, and what must change

| Area | Existing files | Planned treatment |
| --- | --- | --- |
| Annotation, rendering, undo/redo | `src/editor.*`, rendering sections of `src/capture.cpp`, `src/cut.*`, `src/text-band.*` | Retain behavior and regression coverage. |
| Monitor/window discovery | `src/capture.cpp`: `parseMonitor`, `parseWindows`, `probeFocusedMonitor`, `captureMonitorPixels` | Extract Niri metadata handling into `src/niri-ipc.*`; remove `hyprctl` calls. |
| Monitor frames | `src/surface-capture.cpp`, capture declarations in `src/capture.hpp` | Replace the ext capture implementation with `wlr-screencopy`, including repeated frames for scrolling. |
| Overlay lifecycle | `src/main.cpp`, `src/editor.cpp`, `src/scroll-capture.cpp` | Keep LayerShellQt; verify output placement, focus, cancellation, and input pass-through under Niri. |
| Pins | `src/pin.*`, `src/pin-layout.*`, `src/pin-file.*` | Retain overlay surfaces; test across workspaces and output changes. |
| Clipboard, OCR, working documents, recents | `src/capture.cpp`, `src/recent-snaps.*`, config modules | Retain; give Nirisnap separate application paths and environment variables. |
| Notifications | `src/capture.cpp` | Replace `omarchy-notification-send` with an asynchronous `notify-send` invocation for the first release. |
| Auto-scroll injection | `src/scroll-inject.*` | Remove Hyprland natural-scroll queries; validate virtual-pointer behavior before enabling auto mode. |
| Build, installation, documentation | `CMakeLists.txt`, `Makefile`, desktop file, installer, CI, `AGENTS.md`, `docs/` | Rename public identity, remove Omarchy installation assumptions, document Niri setup. |
| Capture-specific tests | `tests/surface-capture-smoke.cpp`, fake `hyprctl` setup in `tests/editor-smoke.cpp` | Replace protocol mocks and compositor fixtures; retain unrelated editor tests. |

These boundaries are based on the inspected [build definition](https://github.com/omacom/omasnap/blob/d339588f3aba554f314d6233da2dddc98f83de55/CMakeLists.txt), [capture implementation](https://github.com/omacom/omasnap/blob/d339588f3aba554f314d6233da2dddc98f83de55/src/capture.cpp), [surface capture](https://github.com/omacom/omasnap/blob/d339588f3aba554f314d6233da2dddc98f83de55/src/surface-capture.cpp), and [scroll injection](https://github.com/omacom/omasnap/blob/d339588f3aba554f314d6233da2dddc98f83de55/src/scroll-inject.cpp).

## Two compatibility constraints to resolve explicitly

### 1. Capture needs a protocol replacement

Omasnap requires `ext-image-copy-capture` and output capture sources. The inspected Niri source implements `wlr-screencopy`; no ext image-copy implementation was found. Niri also documents support for tools using wlr-screencopy. Therefore, changing discovery alone is insufficient. Confirm the running session's globals before implementing. [Niri screencasting documentation](https://niri-wm.github.io/niri/Screencasting.html), [Niri screencopy implementation](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/protocols/screencopy.rs)

Implement an in-process SHM capture path. `grim` can serve as a temporary diagnostic reference if installed, but need not become a shipping dependency. Niri's screenshot actions are also unsuitable as the default monitor capture backend because they publish the raw capture to the clipboard before editing.

### 2. Tiled-window hover selection lacks coordinates

`pos_in_scrolling_layout` is a column/row index, not a screen position. The optional pixel position is populated for floating windows in the inspected source, while tiled-window layout generation leaves it unset. Local IPC confirms null positions for tiled windows. Do not infer positions by summing widths: scrolling offsets, gaps, tabs, animations, and partial visibility make that unreliable. [WindowLayout documentation](https://niri-wm.github.io/niri/niri_ipc/struct.WindowLayout.html), [tiled layout implementation](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/layout/scrolling.rs), [floating layout implementation](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/layout/floating.rs)

Omasnap's window mode crops a frozen monitor frame, including overlap. Niri's native window screenshot instead renders the selected window separately. These are different user-visible behaviors. [Omasnap capture behavior](https://github.com/omacom/omasnap/blob/d339588f3aba554f314d6233da2dddc98f83de55/README.md), [Niri window capture implementation](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/niri.rs)

Recommended initial policy: ship region/full-monitor capture with the existing editor, and make unsupported window capture clearly unavailable. Do not claim full Omasnap parity. Milestone 4 below resolves how to add window capture; it is not a prerequisite for a useful initial release.

## Intended architecture

Keep one executable and one Niri integration. Use small modules, without introducing a general compositor plugin system:

```text
CLI / single-instance lifecycle
    -> Niri metadata queries (asynchronous QProcess + niri msg -j)
    -> output-name mapping to wl_output and QScreen
    -> wlr-screencopy frame, before overlay mapping
    -> CaptureData (native pixels + logical display dimensions)
    -> existing LayerShellQt selector and annotation editor
    -> existing flattened PNG / clipboard / document / pin paths
```

Start with `niri msg -j focused-output`, `outputs`, `workspaces`, and, when needed, `windows` or `focused-window`. Use argument arrays, JSON validation, bounded timeouts, and cancellation. A direct `$NIRI_SOCKET` client can replace subprocesses if startup measurements justify it; that would require adding Qt Network for `QLocalSocket` or implementing socket I/O separately.

Treat output names as join keys. Read logical geometry and scale from `logical`, and physical mode dimensions through `current_mode`. Disabled outputs have absent logical information. Use 64-bit-safe window/workspace IDs, never workspace indices as identities. Preserve optional fields rather than turning missing values into zeroes.

IPC replies are not an atomic desktop snapshot. Query the target before the overlay takes focus, validate output/workspace identity around capture, and cancel or retry boundedly if it changes. A persistent event stream is a later option, not a guarantee that metadata and pixels are synchronized. [Niri IPC contract](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/niri-ipc/src/lib.rs)

## Milestones and acceptance gates

### 0. Establish the fork and build baseline

- [ ] Preserve `PLAN.md`, then initialize upstream history directly in this directory: initialize Git, add `https://github.com/omacom/omasnap.git` as `upstream`, fetch, and create a local branch at the pinned Omasnap revision. Check for file collisions before checkout. Do not create a nested `omasnap/` project.
- [ ] Keep the original MIT license/copyright and bundled font licenses. Record the fork point and origin in the README.
- [ ] Update imported `AGENTS.md` and `docs/platform-scope.md` to match the authorized Niri fork, while retaining relevant editor/threading principles.
- [ ] Install missing build dependencies. Expected Arch-family set: `base-devel cmake ninja pkgconf qt6-base qt6-wayland layer-shell-qt wayland wayland-protocols`; retain `wl-clipboard`, and add `tesseract tesseract-data-eng` for OCR. Verify actual package names before installation.
- [ ] Run the unmodified upstream `make check` baseline, recording any pre-existing failures and which optional analyzers ran.
- [ ] Enumerate the live Wayland registry and record versions for layer-shell, screencopy, SHM, output naming, and virtual-pointer. Verify installed Niri source behavior if it differs from the research revision.
- [ ] Save synthetic or sanitized IPC fixtures, including missing tiled positions and disabled outputs. Do not commit real window titles or screenshots.

**Gate:** reproducible build/test baseline, verified capture protocol, and a documented local capability report. No expectation that the unmodified executable can capture under Niri.

### 1. Implement Niri discovery and native monitor capture

- [ ] Add `src/niri-ipc.cpp/.hpp` for focused output and workspace discovery. Move blocking startup discovery off the UI thread as well as capture work.
- [ ] Add the wlr-screencopy XML with its provenance/license and generate protocol code in CMake. Remove the replaced ext protocol requirements and hard-coded `/usr/share/wayland-protocols` lookups where unnecessary.
- [ ] Replace `captureOutputSurface` internals: bind supported protocol versions, select the named output, receive format/size/stride, allocate SHM, request a cursor-free frame, and handle ready/failure/disconnect/timeout cleanup.
- [ ] Respect advertised stride and buffer format, check allocation arithmetic, retain image storage for its full lifetime, and handle `y_invert`. Support protocol event ordering appropriate to the negotiated version.
- [ ] Normalize output rotation/reflection exactly once. Wlr-screencopy has different metadata from ext capture; do not copy its transform handling blindly.
- [ ] Preserve fractional logical coordinates where needed. Derive pixel mapping from actual captured image dimensions versus logical preview dimensions, with consistent floor/ceil edge rounding and clipping.
- [ ] Update protocol mocks and add tests for malformed IPC, missing outputs, timeout cleanup, pixel formats/stride, inversion, and transformed frame dimensions.

**Gate:** focused-output capture produces correctly oriented native-resolution pixels, with deterministic failure handling and no clipboard mutation. Repeat-capture entry points compile, even if scrolling remains disabled initially.

### 2. Deliver the first usable capture/editor loop

- [ ] Connect region and full-monitor startup to the new backend. Keep full-monitor semantics scoped to the focused output; spanning all monitors is outside the initial release.
- [ ] Keep the overlay unmapped until the initial frame is complete. Match `QScreen` by output name; never silently capture one output and display its overlay on another.
- [ ] Use the overlay layer, retain the intended exclusive-zone behavior, and verify keyboard focus and input regions on Niri. Ensure opening the tool does not rearrange tiled windows.
- [ ] Preserve editing tools, crop/canvas growth, undo/redo, export, quick `--copy`/`--save`, `--file`, and `--clipboard` workflows.
- [ ] Adapt returning from Edit to Select and capture-mode cycling: unsupported Window/Scroll modes must be unavailable with a useful explanation until implemented.
- [ ] Preserve single-instance toggling, file-open handover, cancellation, and stale-lock recovery. Cover cancellation during discovery/capture, not only after the editor appears.
- [ ] On output removal or incompatible geometry changes, close cleanly or restart capture; do not keep a misaligned selection active. In Overview, permit region/full-monitor pixels but do not synthesize window targets.
- [ ] Update the editor smoke harness to use fake Niri responses instead of fake `hyprctl`.

**Gate:** hotkey → selection → annotation → copy/save works in the live Niri session; a second invocation dismisses it. No overlay in its own initial screenshot, no unexpected clipboard changes before explicit output, and native-pixel export passes visual checks.

### 3. Finish Nirisnap identity and desktop integration

- [ ] Rename the executable, desktop entry, application identity, layer namespaces (`nirisnap`, `nirisnap-pin`), build targets, and public CLI text coherently.
- [ ] Use separate config/state/runtime locations, such as `$XDG_CONFIG_HOME/nirisnap/nirisnap.conf`, `$XDG_STATE_HOME/nirisnap/recent/`, and `$XDG_RUNTIME_DIR/nirisnap/`, with equivalent private fallback handling.
- [ ] Rename `OMASNAP_*` settings/test hooks to `NIRISNAP_*`; remove `OMARCHY_OCR_LANGS` fallback. Keep the existing config format without adding an automatic Omasnap migration.
- [ ] Replace Omarchy notifications with best-effort generic notifications after successful output. Defer click-to-reopen until an action-capable notification implementation is tested; notification failure must not fail a save.
- [ ] Validate OCR missing-language errors, editable recents, crash recovery, and pins across workspace changes. Test pin edit handover and cleanup independently of the main instance lock.
- [ ] Provide prefix-based CMake installation and a Niri config example. Do not automatically edit the user's bindings.
- [ ] Adapt CI to build/test Nirisnap and stage an installation, with executable/desktop-entry/font-license checks. Retain optional analyzer reporting.

Suggested bindings, to merge with existing bindings once installed:

```kdl
binds {
    Print { spawn "nirisnap"; }
    Ctrl+Print { spawn "nirisnap" "--capture-fullscreen"; }
}
```

Validate the example with `niri validate`. Account for existing screenshot shortcuts and shortcuts reserved by the user's Niri config. In particular, do not assume Omasnap's Super+Arrow window-navigation shortcuts will reach the application. [Niri key bindings](https://niri-wm.github.io/niri/Configuration%3A-Key-Bindings.html)

If documenting screencast exclusion, explain that Niri's `block-out-from` replaces surfaces with black rectangles; it is not an invisible-surface capture mechanism or a substitute for capturing before mapping. Test any example against both the editor and pins. [Niri layer rules](https://niri-wm.github.io/niri/Configuration%3A-Layer-Rules.html)

**Gate:** installable Nirisnap MVP with region/full-monitor capture, editor, file/clipboard input, output, OCR, recents, and pins; no runtime Hyprland or Omarchy dependency.

### 4. Add window capture with an explicit product decision

Evaluate these routes before advertising window support:

| Route | Result | Limitation / decision |
| --- | --- | --- |
| Native Niri window capture | Pick a window through `niri msg -j pick-window`, or query the focused window, then capture its ID and open the result in the editor. | Works without tiled screen coordinates, but changes capture semantics and writes the raw image to the clipboard. |
| Exact Omasnap-style selection | Crop the selected window rectangle from the frozen output frame. | Requires reliable tiled-window viewport geometry/visibility from a future Niri API or a maintained compositor extension. Current IPC is insufficient. |
| Manual region fallback | User draws around the desired window. | Available in the MVP, but is not automatic window capture. |

For a native-capture prototype:

1. Run the picker before mapping the editor, or fully unmap the editor and release input first. Treat cancellation as cancellation, not an empty screenshot.
2. Capture the explicit selected ID with `niri msg action screenshot-window --id <id> --path <absolute-private-path> --show-pointer false`. Never depend on focus remaining unchanged after opening an overlay.
3. Subscribe to completion events before issuing the action and correlate `ScreenshotCaptured.path` with a unique path. The action reply is not proof that encoding/writing finished. Add a bounded timeout and validate the decoded file.
4. Open the file in the existing image editor, then clean transient files when ownership permits. Handle a closed target window and failed writes.

The inspected Niri `save_screenshot` implementation always updates the clipboard and may send its own notification, including when an explicit file path is supplied. Restoring the clipboard afterward cannot retract an image already observed by clipboard history. Consequently, **do not silently use this route for the normal edit-before-copy workflow**. Ship it only as a clearly described opt-in workflow, or keep it experimental until a no-clipboard API is available. [Niri screenshot actions](https://niri-wm.github.io/niri/niri_ipc/enum.Action.html), [completion and clipboard implementation](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/niri.rs)

If exact parity is required, define a separate Niri work item for visible tile/window rectangles, workspace viewport offsets, and overlap/visibility information. Optional floating-window positions alone do not solve tiled windows or provide reliable stacking order. Do not make a custom Niri build a hidden dependency of Nirisnap.

**Gate:** document the chosen semantics and clipboard behavior; test tiled/floating/fullscreen windows, picker cancellation, target closure, and output scaling. Exact hover/crop parity remains incomplete until its compositor dependency is resolved.

### 5. Port scrolling capture

- [ ] Reimplement `OutputCapture::open/grab/close` on wlr-screencopy with reusable connection/buffers where valid and a fresh protocol frame per capture.
- [ ] Bring up manual scrolling first, retaining the existing stitcher and offline replay tests. Validate vertical/horizontal stitching, fixed headers, duplicate frames, page end, and cancellation.
- [ ] Verify Niri keyboard and pointer focus while the scroll region passes input to the underlying application. Remove Hyprland-specific workarounds only after live validation.
- [ ] Ensure each sampled region excludes Nirisnap controls, scrims, and other moving overlays; sample the exposed page pixels without self-capture artifacts.
- [ ] Enable auto-scroll only after virtual-pointer output targeting, direction, focus, and frame acknowledgment are proven on Niri. Remove the Hyprland natural-scroll query; do not require privileged uinput access for the default feature.
- [ ] If injection is unavailable or unreliable, keep manual scrolling usable and show auto mode as unavailable. Test natural-scroll configurations and user interruption explicitly.

Niri contains virtual-pointer support, but that alone does not establish compatibility with Omasnap's injection assumptions. [Niri virtual pointer implementation](https://github.com/niri-wm/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/protocols/virtual_pointer.rs)

**Gate:** manual stitched capture passes live tests and inherited stitch fixtures; auto mode is advertised only when its separate integration tests pass.

## Verification and release criteria

Run the adapted `make check` after behavioral changes. Preserve broad existing editor coverage and add tests at the changed integration boundaries. Offscreen Qt tests cannot establish real compositor behavior, so record live checks separately.

| Test group | Cases / expected outcome |
| --- | --- |
| Display geometry | Scale 1, 1.25, 1.5, 2; rotated/flipped outputs; mixed DPI; negative output origins; exact native crop dimensions and no double rotation. |
| Niri layout | Empty workspace, disabled output, floating/tiled/fullscreen apps, workspace scrolling, Overview, output disconnect; stable placement or clear cancellation. |
| Input/lifecycle | Repeated hotkey, Escape, file handover, failures during startup, scroll input hole, pin focus; no stuck grabs or surviving capture workers. |
| Outputs | Copy-only/save-only/both; clipboard survives app exit; missing helpers and unwritable destination; no success notification for failed output. |
| Editor regression | Undo/replay, crop, fractional-scale annotations, cut, text, redaction export, OCR, recents, pin reopen. |
| Capture contract | No clipboard change during normal selection/editing; no editor pixels in initial capture; protocol errors free buffers and connections. |
| Performance | Record hotkey-to-first-paint and peak memory on the same output/build; use inherited startup tracing to locate regressions. |

Initial support target: the verified local Niri 26.04 session, with runtime capability checks. Broader version support requires its own tested matrix. No compositor modifications, cross-monitor region stitching, portal/PipeWire capture backend, or toolkit rewrite are required for the MVP.

Suggested implementation order: **0 → 1 → 2 → 3**, release the useful MVP, then **4 and 5** as separately testable feature work. Keep exact window-selection parity visibly tracked as a compositor-dependent requirement rather than counting it as completed by a fallback.
