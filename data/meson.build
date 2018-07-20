icondir = join_paths(get_option('datadir'), 'icons/hicolor')
foreach size: ['scalable', 'symbolic']
  install_subdir('icons/hicolor/' + size, install_dir: icondir)
endforeach

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
)

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

