# Platform scope: Wayland + Niri, on purpose

Nirisnap targets one platform: **Wayland, on Niri.** This is not a starting
point we intend to broaden into a general Linux screenshot tool. It is a
deliberate choice that keeps the code small and lets it use real platform
facilities instead of lowest-common-denominator abstractions. It is a fork of
Omasnap (Hyprland/Omarchy) precisely because Omasnap's own policy directs
ports needing compositor-specific paths to forks rather than backends.

## What "Niri-only" means concretely

- Monitor discovery goes through `niri msg -j` (`focused-output`,
  `outputs`, `workspaces`) — see `src/niri-ipc.cpp`. There is no
  generic-compositor fallback for this, because there is no generic,
  reliable way to ask an arbitrary Wayland compositor "what outputs exist
  and which is focused."
- Window discovery is intentionally absent: Niri IPC reports tiled windows
  with `layout.tile_pos_in_workspace_view = null` and column/row indices
  only. Summing widths to infer screen positions is unreliable (scrolling
  offsets, gaps, tabs, animations, partial visibility), so Nirisnap offers
  region/full-monitor capture and leaves window capture to the explicit
  milestone in `docs/window-capture.md`. See `src/niri-ipc.cpp`
  (`parseNiriWindowsPlaceholder`).
- Pixel capture uses `zwlr_screencopy_manager_v1` (SHM-only, cursor-free,
  `y_invert`-aware) in `src/surface-capture.cpp`. Niri does not implement
  `ext-image-copy-capture`; that protocol is not a fallback here.
- Scroll capture's auto-scroll uses `zwlr_virtual_pointer_v1` bound to the
  target output (`src/scroll-inject.cpp`). There is no Niri natural-scroll
  query, so uinput pre-compensation is disabled: the virtual pointer is the
  default until its Niri behavior is proven.
- The keyboard-grab dance in the scroll and area overlays originated as a
  Hyprland workaround (an exclusive keyboard grab on a layer surface pinned
  pointer focus even over an input-region hole). It is retained pending live
  Niri validation — see `src/scroll-capture.cpp` — not asserted as a Niri
  rule.
- Notifications use best-effort `notify-send` after successful output, with
  no click-to-reopen action yet (see `src/capture.cpp`). Notification
  failure never fails a save.
- OCR languages come from `NIRISNAP_OCR_LANGS` (default `eng`); there is no
  `OMARCHY_*` fallback.

## What's actually generic, and why it can work elsewhere by accident

Underneath the Niri-specific discovery, the capture and injection
mechanisms are standard Wayland protocols: `wlr-screencopy` for reading
pixels, `zwlr_virtual_pointer_v1` for injected scroll input, and
`layer-shell` for every overlay surface. Any wlroots compositor that
implements the same protocols may run region/full-monitor capture and the
editor without modification, because that code never asks "is this Niri" —
it asks the compositor for a capability and uses it if offered.

**This is fine, and it is not a target.** If it works on another wlroots
compositor, that's a side effect of standing on real protocols instead of
a bespoke integration, not a supported configuration. We do not add
fallbacks, feature flags, or compatibility code to make it work somewhere
else. We do not test on anything but Niri. A bug that only reproduces on
another compositor is not a bug we chase.

## Contribution policy for other window managers

A pull request that adds support for, or improves behavior on, a different
Wayland compositor is **out of scope** for this fork in the same way Niri
support was out of scope upstream. The rule is unchanged, only the target
moved: no compositor branching, no backend abstraction, no new dependency
pulled in just for another compositor. If it needs an `#ifdef`, a new
interface, or a "backend" concept, it doesn't belong here — fork it.

## X11, macOS, Windows

Not supported, not planned, not accepted.
