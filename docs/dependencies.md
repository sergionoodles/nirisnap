# Dependencies: learn the current set before adding to it

The dependency list is small on purpose and should stay that way. Before
adding anything — a library, a build tool, an external process — read this
file, then ask whether the thing you need is genuinely absent from it.

## Link-time (build and runtime)

From `CMakeLists.txt`, this is the entire list:

| Dependency | What it's for |
|---|---|
| **Qt6** (Concurrent, Core, Gui, Test, Widgets) 6.8+ | Everything: windowing, painting, the editor UI, the worker-pool threading model ([threading.md](threading.md)), the test harness |
| **LayerShellQt** | Layer-shell surfaces (the capture overlay, the editor, pinned captures) |
| **wayland-client** (pkg-config) | Raw protocol client code (`wlr-screencopy`, `zwlr_virtual_pointer_v1`) that LayerShellQt/QtWayland don't expose |
| **wayland-scanner** + protocol XML | Generates the C bindings for the above at build time; not a runtime dependency |

That's it. No JSON library (Qt's `QJsonDocument` handles `niri msg -j`
output), no image codec beyond what Qt's own PNG support provides, no HTTP,
no logging framework, no CLI-parsing library beyond `QCommandLineParser`,
no config-file parser beyond `QSettings` (used for the one optional INI
file — see below).

Protocol XML is vendored in `protocols/`:
`wlr-screencopy-unstable-v1.xml` (v3, from wlroots/wlr-protocols, MIT —
provenance header inside the file; Niri implements v3 and does not
implement `ext-image-copy-capture`) and
`wlr-virtual-pointer-unstable-v1.xml`. The build does not read
`/usr/share/wayland-protocols` for capture.

## Runtime: external processes, not libraries

Nirisnap shells out to a small number of existing tools instead of linking
their libraries in-process. This is intentional: a `QProcess` call to a
well-maintained CLI tool is a dependency Nirisnap doesn't have to build,
version, or debug.

| Process | Used for | Required? |
|---|---|---|
| `niri` | Output/workspace discovery (`msg -j focused-output/outputs/workspaces`) | Yes — see [platform-scope.md](platform-scope.md) |
| `wl-copy` / `wl-paste` | Writing PNG/text to the Wayland clipboard, and verifying the write | Yes |
| `tesseract` | OCR text recognition | Only if OCR is used; missing tesseract fails just that action |
| `notify-send` | Capture-finished notifications | No — best-effort; failure never fails a save |

Each Niri query runs through `runNiriMsg` in `src/niri-ipc.cpp` with an
argument array, JSON validation, and a bounded timeout — never inline on
the UI thread for capture work (see [threading.md](threading.md)).

## Not a dependency: external Qt platform themes

Some sessions export `QT_QPA_PLATFORMTHEME=gtk3` so Qt apps match GTK apps.
Nirisnap overrides it to `generic` (Qt's built-in theme) for its own
process in `main()` before `QApplication` is constructed: honouring the
session value loads the `qgtk3` plugin, which initialises GTK inside the
process for an overlay that is hand-painted, opens no dialogs and reads no
palette. The chrome and application-default fonts are pinned by
`chromeFont()`, `chromeMonoFont()`, and `chromeDefaultFont()`
(`src/overlay-chrome.cpp`) instead. Do not make startup or chrome rendering
depend on an external desktop theme, `QStyle`- or palette-derived chrome,
or icon-theme lookup.

## The one config file

`~/.config/nirisnap/nirisnap.conf` is optional INI, read with `QSettings`.
It exists for exactly two things people legitimately need to override
(screenshot destination/filename pattern, and preset colors) — not as a
general settings mechanism. See the "minimally configurable" principle in
[AGENTS.md](../AGENTS.md) before adding a new key. There is no automatic
`omasnap.conf` migration and no `OMASNAP_*` aliases; the new names are
`nirisnap.conf` and `NIRISNAP_*`.

## Evaluating a new dependency

Ask, in order:

1. **Can Qt already do this?**
2. **Is this an OS-integration concern better solved by shelling out to an
   existing tool**, the way clipboard, OCR, and notifications are?
3. **Does it exist only to support a non-Niri compositor?** Then it's out
   of scope — see [platform-scope.md](platform-scope.md).
4. **Is it justified anyway?** Then it needs to earn its place in the table
   above, and this file needs to be updated in the same PR.

## Binary size

Single binary, installed to `~/.local/bin/nirisnap`. Every dependency
added here is weight every user carries on every install and every update.
