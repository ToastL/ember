project('ember', 'c',
  version: '0.0.1',
  license: 'MIT',
  meson_version: '>=0.59.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=false',
  ],
)

cc = meson.get_compiler('c')

# Dependencies
wayland_server_dep = dependency('wayland-server')
libdrm_dep = dependency('libdrm')
gbm_dep = dependency('gbm')
egl_dep = dependency('egl')
glesv2_dep = dependency('glesv2')
libinput_dep = dependency('libinput')
libudev_dep = dependency('libudev')
wayland_protos_dep = dependency('wayland-protocols')
xkbcommon_dep = dependency('xkbcommon')

# Wayland Scanner
wayland_scanner = find_program('wayland-scanner')
wayland_scanner_server = generator(
    wayland_scanner,
    output: '@BASENAME@-protocol.c',
    arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_header = generator(
    wayland_scanner,
    output: '@BASENAME@-protocol.h',
    arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
)

# Protocols
wl_protocol_dir = wayland_protos_dep.get_variable(pkgconfig: 'pkgdatadir')
xdg_shell_xml = wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_c = wayland_scanner_server.process(xdg_shell_xml)
xdg_shell_h = wayland_scanner_header.process(xdg_shell_xml)

# Source files
src_files = files(
  'src/main.c',
  # Backend
  'src/backend/drm.c',
  'src/backend/egl.c',
  'src/backend/renderer.c',
  'src/backend/output.c',
  # Input
  'src/input/input.c',
  'src/input/cursor.c',
  'src/input/dispatch.c',
  # Wayland Protocols
  'src/wayland/compositor.c',
  'src/wayland/seat.c',
  'src/wayland/shm.c',
  'src/wayland/shell.c',
  'src/wayland/data_device.c'
)

# Executable
executable(
  'ember',
  [src_files, xdg_shell_c, xdg_shell_h],
  include_directories: [
      include_directories('include'),
      include_directories('include/wayland')
  ],
  dependencies: [
    wayland_server_dep,
    libdrm_dep,
    gbm_dep,
    egl_dep,
    glesv2_dep,
    libinput_dep,
    libudev_dep,
    xkbcommon_dep,
  ],
  install: true,
)
