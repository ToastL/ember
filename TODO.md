# Roadmap: Ember Wayland Compositor (Redux)

We have a working rendering engine. Now we build the server, layer by layer.

## Phase 5: Wayland Server Core ✅
**Goal:** Allow clients to connect and "talk" to us, even if they can't show windows yet.
- [x] `wl_display` socket verification.
- [x] Implement `wl_compositor` global.
- [x] Implement `wl_shm` global.
- [x] Verify (via check_globals).

## Phase 6: The Shell (Window Management) ✅
**Goal:** Allow clients to define "Windows" (`xdg_surface`).
- [x] **Output**: Advertise the screen (`wl_output`) so clients know monitor size.
- [x] **Seat**: Advertise input (`wl_seat`) so clients know we have a mouse/keyboard.
- [x] **XDG Shell**: Supporting the standard Desktop Protocol.
    - [x] Handle `xdg_wm_base`.
    - [x] Handle `xdg_surface` and `xdg_toplevel`.

## Phase 7: Rendering & Input ✅
**Goal:** Actually draw the client's pixels and send them input events.
- [x] **wl_region**: Fixed "invalid object" protocol error.
- [x] **wl_callback**: Implemented frame callbacks.
- [x] **wl_data_device_manager**: Clipboard/DnD support (fixed GTK seat warnings).
- [x] **Texture Upload**: SHM buffers → OpenGL Textures.
- [x] **Compositing**: Draw client surfaces in render loop.
- [x] **🎉 CLIENTS RENDERING ON SCREEN!** (QEMU working!)
- [x] **Mouse Cursor**: Visible cursor that tracks libinput movement.
- [ ] **Input Dispatch**: Route libinput events to focused windows.

## Phase 8: Visuals & Window Management (The "Look")
**Goal:** Control how the desktop feels.
- [ ] **Positioning**: Smart placement (center, tiling, or manual).
- [ ] **Decorations**: Decide between SSD (we draw titlebars) or CSD (app draws them).
- [ ] **Effects**: Shadows, Transparency, Animations, Rounded Corners.
- [ ] **Background**: Wallpaper support.
