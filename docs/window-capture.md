# Window capture on Niri: product decision

Stock Niri currently cannot provide everything needed to reproduce
Omasnap's window selection, so Nirisnap MVP ships region/full-monitor
capture with the existing editor and leaves window capture clearly
unavailable. This file records why and what the routes are.

## Why exact parity is blocked

- `pos_in_scrolling_layout` is a column/row index, not a screen position.
- `layout.tile_pos_in_workspace_view` is `null` for tiled windows in
  practice (verified locally); the optional pixel position is populated for
  floating windows only.
- Do not infer positions by summing widths: scrolling offsets, gaps, tabs,
  animations, and partial visibility make that unreliable.
- Omasnap's window mode crops a frozen monitor frame including overlap.
  Niri's native window screenshot renders the selected window separately —
  different user-visible behavior.

## Routes evaluated

| Route | Result | Limitation / decision |
|---|---|---|
| Native Niri window capture | Pick via `niri msg -j pick-window` or focused window, capture its ID, open in editor | Works without tiled coordinates, but changes semantics and writes the raw image to the clipboard |
| Exact Omasnap-style selection | Crop selected window rect from frozen output frame | Requires reliable tiled-window viewport geometry/visibility from a future Niri API or maintained extension. Current IPC is insufficient |
| Manual region fallback | User draws around the window | Available in the MVP, but is not automatic window capture |

## Native-capture prototype (if pursued)

1. Run the picker before mapping the editor, or fully unmap the editor and
   release input first. Treat cancellation as cancellation.
2. Capture the explicit selected ID with
   `niri msg action screenshot-window --id <id> --path <absolute-private-path>
   --show-pointer false`. Never depend on focus remaining unchanged.
3. Subscribe to completion events before issuing the action and correlate
   `ScreenshotCaptured.path` with a unique path. The action reply is not
   proof that encoding/writing finished. Add a bounded timeout and validate
   the decoded file.
4. Open the file in the existing image editor, then clean transient files.
   Handle a closed target window and failed writes.

The inspected Niri `save_screenshot` implementation always updates the
clipboard and may send its own notification, even with an explicit file
path. Restoring the clipboard afterward cannot retract an image already
observed by clipboard history. **Do not silently use this route for the
normal edit-before-copy workflow.** Ship it only as a clearly described
opt-in workflow, or keep it experimental until a no-clipboard API exists.

If exact parity is required, define a separate Niri work item for visible
tile/window rectangles, workspace viewport offsets, and overlap/visibility
information. Optional floating-window positions alone do not solve tiled
windows or provide reliable stacking order. Do not make a custom Niri build
a hidden dependency of Nirisnap.
