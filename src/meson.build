vapi_sources = [
  'config.vapi',
]

baobab_vala_sources = [
  'baobab-application.vala',
  'baobab-cellrenderers.vala',
  'baobab-chart.vala',
  'baobab-contents-cell.vala',
  'baobab-file-cell.vala',
  'baobab-folder-cell.vala',
  'baobab-folder-display.vala',
  'baobab-location-list.vala',
  'baobab-location.vala',
  'baobab-name-cell.vala',
  'baobab-pathbar.vala',
  'baobab-preferences-dialog.vala',
  'baobab-progress-cell.vala',
  'baobab-ringschart.vala',
  'baobab-scanner.vala',
  'baobab-size-cell.vala',
  'baobab-time-modified-cell.vala',
  'baobab-tree-list-row-sorter.vala',
  'baobab-treemap.vala',
  'baobab-window.vala',
  'main.vala'
]

baobab_sources = [
  vapi_sources,
  baobab_vala_sources,
  resources
]

baobab_vala_args = [
  '--target-glib', '2.44',
  '--vapidir', meson.current_source_dir(),
  '--gresources', resource_files,
]

baobab_c_args = [
  '-include', 'config.h'
]

baobab_dependencies = [
  glib,
  gio,
  gobject,
  gtk,
  libhandy,
  math
]

executable('baobab', baobab_sources,
  include_directories: config_h_dir,
  vala_args: baobab_vala_args,
  c_args: baobab_c_args,
  dependencies: baobab_dependencies,
  export_dynamic: true,
  win_subsystem: 'windows',
  install: true
)
