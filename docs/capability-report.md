# Capability report (verified local session, 2026-09-09)

- OS: CachyOS (Arch-derived).
- Niri: `26.04 (8ed0da4)` — CLI and compositor versions match; JSON IPC responds.
- Output: one enabled `HDMI-A-1` 2560×1440 @ scale 1, transform `Normal`;
  another output is disabled (absent `logical`). Mixed scaling/rotation untested.
- Qt: Core/Widgets 6.11.2, wayland-client 1.26.0, `wayland-scanner`,
  `wl-copy`, `notify-send` present. `cmake`, `ninja`, `tesseract`, `grim`,
  `wayland-info`, LayerShellQt, wayland-protocols were absent from `PATH` at
  plan time — verify package availability during setup.

## Live Wayland registry (sorted)

```
ext_background_effect_manager_v1 v1
ext_data_control_manager_v1 v1
ext_foreign_toplevel_list_v1 v1
ext_idle_notifier_v1 v2
ext_session_lock_manager_v1 v1
ext_workspace_manager_v1 v1
mutter_x11_interop v1
org_kde_kwin_server_decoration_manager v1
wl_compositor v6
wl_data_device_manager v3
wl_output v4
wl_seat v9
wl_shm v2
wl_subcompositor v1
wp_cursor_shape_manager_v1 v2
wp_drm_lease_device_v1 v1
wp_fractional_scale_manager_v1 v1
wp_fractional_scale_manager_v1 v1
wp_presentation v2
wp_security_context_manager_v1 v1
wp_viewporter v1
xdg_activation_v1 v1
xdg_wm_base v7
zwlr_data_control_manager_v1 v2
zwlr_foreign_toplevel_manager_v1 v3
zwlr_gamma_control_manager_v1 v1
zwlr_layer_shell_v1 v5
zwlr_output_manager_v1 v4
zwlr_screencopy_manager_v1 v3
zwlr_virtual_pointer_manager_v1 v2
zwp_idle_inhibit_manager_v1 v1
zwp_input_method_manager_v2 v1
zwp_keyboard_shortcuts_inhibit_manager_v1 v1
zwp_linux_dmabuf_v1 v5
zwp_pointer_constraints_v1 v1
zwp_pointer_gestures_v1 v3
zwp_primary_selection_device_manager_v1 v1
zwp_relative_pointer_manager_v1 v1
zwp_tablet_manager_v2 v1
zwp_text_input_manager_v3 v1
zwp_virtual_keyboard_manager_v1 v1
zxdg_decoration_manager_v1 v1
zxdg_exporter_v2 v1
zxdg_importer_v2 v1
zxdg_output_manager_v1 v3
```

Notes:

- `zwlr_screencopy_manager_v1 v3` present; no `ext-image-copy-capture`
  globals (confirms PLAN compatibility constraint #1: Niri needs
  wlr-screencopy, not ext capture).
- `zwlr_layer_shell_v1 v5`, `wl_shm v2`, `wl_output v4`,
  `zwlr_virtual_pointer_manager_v1 v2` present.
- `wp_fractional_scale_manager_v1` present; fractional-scale rendering
  still derives pixel mapping from actual captured image dimensions vs
  logical preview dimensions.

## Niri IPC samples

Sanitized fixtures live in `tests/fixtures/niri-*.json`. Tiled windows
report `layout.tile_pos_in_workspace_view = null` with column/row indices
only — no screen coordinates are inferred from them.
