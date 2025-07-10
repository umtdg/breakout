project(
  'breakout',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1.0',
  default_options : ['warning_level=3', 'c_std=c99'],
)

cc = meson.get_compiler('c')

math_dep = cc.find_library('m', required: true)
raylib = dependency('raylib', required: true)

dependencies = [
  math_dep,
  raylib
]

exe = executable(
  'breakout',
  'breakout.c',
  dependencies : dependencies,
  install : true,
)

test('basic', exe)
