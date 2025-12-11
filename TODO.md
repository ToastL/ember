# Roadmap: Ember Wayland Compositor (Redux)

We have a working rendering engine. Now we build the server, layer by layer.

## Phase 5: Wayland Server Core
**Goal:** Allow clients to connect and "talk" to us, even if they can't show windows yet.
- [ ] **Socket**: Ensure `WAYLAND_DISPLAY` is active and accessible.
- [ ] **Compositor**: Implement `wl_compositor` interface.
    - Allow clients to `create_surface`.
    - Allow `create_region`.
- [ ] **SHM**: Implement `wl_shm` interface.
    - Allow clients to create shared memory pools (pixel buffers).

## Phase 6: The Shell (Window Management)
**Goal:** Allow clients to define "Windows" (`xdg_surface`).
- [ ] **Output**: Advertise the screen (`wl_output`) so clients know monitor size.
- [ ] **Seat**: Advertise input (`wl_seat`) so clients know we have a mouse/keyboard.
- [ ] **XDG Shell**: Supporting the standard Desktop Protocol.
    - Handle `xdg_wm_base`.
    - Handle `xdg_surface` and `xdg_toplevel`.

## Phase 7: Rendering & Input
**Goal:** Actually draw the client's pixels and send them input events.
- [ ] **Texture Upload**: Convert SHM buffers to OpenGL Textures.
- [ ] **Compositing**: Draw those textures in the render loop.
- [ ] **Input Dispatch**: Route generic libinput events to specific focused windows.
