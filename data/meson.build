icondir = join_paths(get_option('datadir'), 'icons/hicolor')
foreach size: ['scalable', 'symbolic']
  install_subdir('icons/hicolor/' + size, install_dir: icondir)
endforeach

resource_files = files('baobab.gresource.xml')
resources = gnome.compile_resources('org.gnome.baobab', resource_files, c_name: 'resources')

install_man ('baobab.1')

desktop_file = i18n.merge_file(
  input: 'org.gnome.baobab.desktop.in',
  output: 'org.gnome.baobab.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

appdata_file = i18n.merge_file(
  input: 'org.gnome.baobab.appdata.xml.in',
  output: 'org.gnome.baobab.appdata.xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)# Validating app data

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata', appstream_util,
    args: [
      'validate-relax', '--nonet', appdata_file.full_path()
    ]
  )
endif

install_data (
  'org.gnome.baobab.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
)

service_conf = configuration_data ()
service_conf.set ('bindir', join_paths (get_option('prefix'), get_option('bindir')))

configure_file (
  input: 'org.gnome.baobab.service.in',
  output: 'org.gnome.baobab.service',
  configuration: service_conf,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services')
)

