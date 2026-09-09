# Niri setup for Nirisnap

Nirisnap does not edit your Niri config. Merge the binds you want, then
validate.

## Suggested binds

```kdl
binds {
    Print { spawn "nirisnap"; }
    Ctrl+Print { spawn "nirisnap" "--capture-fullscreen"; }
}
```

Validate with:

```bash
niri validate
```

Account for existing screenshot shortcuts and shortcuts reserved by your
Niri config.

## Screenshot exclusion (optional)

Niri's `block-out-from` replaces matching surfaces with black rectangles;
it is not an invisible-surface capture mechanism or a substitute for
capturing before mapping. Nirisnap already captures before mapping its
overlay, so no exclusion is required for the initial frame. If you add a
`block-out-from` rule, test it against both the editor overlay
(`nirisnap` scope) and pins (`nirisnap-pin` scope).

Example (test before keeping):

```kdl
layer-rules {
    // Example only — verify against editor + pins in your session.
    // block-out-from "screencast" { }
}
```

See Niri docs for [key bindings](https://niri-wm.github.io/niri/Configuration%3A-Key-Bindings.html)
and [layer rules](https://niri-wm.github.io/niri/Configuration%3A-Layer-Rules.html).

## What Nirisnap needs from the compositor

- `niri` on `PATH` while Nirisnap runs (`niri msg -j focused-output`,
  `outputs`, `workspaces`).
- `zwlr_screencopy_manager_v1` (Niri 26.04 exposes v3), `wl_shm`,
  `wl_output`, and `zwlr_layer_shell_v1`.
