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

# Source files
src_files = files(
  'src/main.c',
  'src/backend/drm.c',
  'src/input/seat.c'
)

# Executable
executable(
  'ember',
  src_files,
  include_directories: include_directories('include'),
  dependencies: [
    wayland_server_dep,
    libdrm_dep,
    gbm_dep,
    egl_dep,
    glesv2_dep,
    libinput_dep,
    libudev_dep,
  ],
  install: true,
)
