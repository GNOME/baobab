project(
  'baobab', ['vala', 'c'],
  version: '41.0',
  license: 'GPL2+',
  meson_version: '>= 0.50.0',
)

gnome = import('gnome')
i18n = import('i18n')
python = import('python')

glib = dependency('glib-2.0', version: '>=2.44')
gio = dependency('gio-2.0', version: '>=2.44')
gobject = dependency('gobject-2.0', version: '>=2.44')
gtk = dependency('gtk+-3.0', version: '>=3.24.1')
libhandy = dependency('libhandy-1', version: '>=1.0.0')

cc = meson.get_compiler('c')
math = cc.find_library('m', required: false)

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
configure_file(output: 'config.h', configuration: conf)
config_h_dir = include_directories('.')

meson.add_install_script(python.find_installation('python3').path(),
  join_paths(meson.source_root(), 'build-aux', 'post-install.py'))

subdir('data')
subdir('help')
subdir('po')
subdir('src')
