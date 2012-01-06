/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Ryan Lortie <desrt@desrt.ca>
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

namespace Baobab {
	public class Window : Gtk.ApplicationWindow {
		Gtk.TreeView treeview;
		Chart rings_chart;
		Chart treemap;
		Scanner? scanner;

		private const GLib.ActionEntry[] action_entries = {
			{ "scan-home",       on_scan_home_activate       },
			{ "scan-filesystem", on_scan_filesystem_activate },
			{ "scan-folder",     on_scan_folder_activate     },
			{ "scan-remote",     on_scan_remote_activate     },
			{ "stop",            on_stop_activate            },
			{ "refresh",         on_refresh_activate         }
		};

		private enum DndTargets {
			URI_LIST
		}

		private const Gtk.TargetEntry dnd_target_list[1] = {
		    {"text/uri-list", 0, DndTargets.URI_LIST}
		};

		public Window (Application app) {
			Object (application: app);

			add_action_entries (action_entries, this);

			// Build ourselves.
			var builder = new Gtk.Builder ();
			try {
				builder.add_from_file (Config.PKGDATADIR + "/baobab-main-window.ui");
			} catch (Error e) {
				error ("loading main builder file: %s", e.message);
			}

			// Cache some objects from the builder.
			rings_chart = builder.get_object ("rings-chart") as Chart;
			treemap = builder.get_object ("treemap") as Chart;
			treeview = builder.get_object ("treeview") as Gtk.TreeView;

			var ui_settings = Application.get_ui_settings ();

			// Setup the logic for switching between the chart types.
			var chart_type_combo = builder.get_object ("chart-type-combo");
			var charts_notebook = builder.get_object ("chart-notebook");
			chart_type_combo.bind_property ("active", charts_notebook, "page", BindingFlags.SYNC_CREATE);
			ui_settings.bind ("active-chart", chart_type_combo, "active-id", SettingsBindFlags.GET_NO_CHANGES);

			// Setup the logic for statusbar visibility.
			var statusbar = builder.get_object ("statusbar") as Gtk.Widget;
			statusbar.visible = ui_settings.get_boolean ("statusbar-visible");

			// Setup drag-n-drop
			drag_data_received.connect(on_drag_data_received);
			enable_drop ();

			// Go live.
			add (builder.get_object ("window-contents") as Gtk.Widget);
			title = _("Disk Usage Analyzer");
			set_default_size (800, 500);
			show ();
		}

		void on_scan_home_activate () {
			var dir = File.new_for_path (GLib.Environment.get_home_dir ());
			scan_directory (dir);
		}

		void on_scan_filesystem_activate () {
			var dir = File.new_for_uri ("file:///");
			scan_directory (dir);
		}

		void on_scan_folder_activate () {
			var file_chooser = new Gtk.FileChooserDialog (_("Select Folder"), this,
			                                              Gtk.FileChooserAction.SELECT_FOLDER,
			                                              Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
			                                              Gtk.Stock.OPEN, Gtk.ResponseType.ACCEPT);

			file_chooser.set_modal (true);

			file_chooser.response.connect ((response) => {
				if (response == Gtk.ResponseType.ACCEPT) {
					var dir = file_chooser.get_file ();
					scan_directory (dir);
				}
				file_chooser.destroy ();
			});

			file_chooser.show ();
		}

		void on_scan_remote_activate () {
			print ("sr\n");
		}

		void on_stop_activate () {
			if (scanner != null) {
				scanner.stop ();
			}
		}

		void on_refresh_activate () {
			if (scanner != null) {
				scan_directory (scanner.directory);
			}
		}

		void on_drag_data_received (Gtk.Widget widget, Gdk.DragContext context, int x, int y,
		                            Gtk.SelectionData selection_data, uint target_type, uint time) {
			File dir = null;

			if ((selection_data != null) && (selection_data.get_length () >= 0) && (target_type == DndTargets.URI_LIST)) {
				var uris = GLib.Uri.list_extract_uris ((string) selection_data.get_data ());
				if (uris != null && uris.length == 1) {
					dir = File.new_for_uri (uris[0]);
				}
			}

			if (dir != null) {
				// finish drop before scanning, since the it can time out
				Gtk.drag_finish (context, true, false, time);
				scan_directory (dir);
			} else {
				Gtk.drag_finish (context, false, false, time);
			}
		}

		void enable_drop () {
			Gtk.drag_dest_set (this,
			                   Gtk.DestDefaults.DROP | Gtk.DestDefaults.MOTION | Gtk.DestDefaults.HIGHLIGHT,
			                   dnd_target_list,
			                   Gdk.DragAction.COPY);
		}

		void disable_drop () {
			Gtk.drag_dest_unset (this);
		}

		void message (string primary_msg, string secondary_msg, Gtk.MessageType type) {
			var dialog = new Gtk.MessageDialog (this, Gtk.DialogFlags.DESTROY_WITH_PARENT, type,
			                                    Gtk.ButtonsType.OK, "%s", primary_msg);
			dialog.format_secondary_text ("%s", secondary_msg);
			dialog.run (); /* XXX kill it with fire */
			dialog.destroy ();
		}

		public bool check_dir (File directory) {
			if (Application.is_excluded_location (directory)) {
				message("", _("Cannot check an excluded folder!"), Gtk.MessageType.INFO);
				return false;
			}

			try {
				var info = directory.query_info (FILE_ATTRIBUTE_STANDARD_TYPE, FileQueryInfoFlags.NONE, null);
				if (info.get_file_type () != FileType.DIRECTORY/* || is_virtual_filesystem ()*/) {
					var primary = _("\"%s\" is not a valid folder").printf (directory.get_parse_name ());
					message (primary, _("Could not analyze disk usage."), Gtk.MessageType.ERROR);
					return false;
				}
				return true;
			} catch (Error e) {
				message ("", e.message, Gtk.MessageType.INFO);
				return false;
			}
		}

		void first_row_has_child (Gtk.TreeModel model, Gtk.TreePath path, Gtk.TreeIter iter) {
			model.row_has_child_toggled.disconnect (first_row_has_child);
			treeview.expand_row (path, false);
		}

		void set_model (Scanner model) {
			Gtk.TreeIter first;

			treeview.model = model;

			if (model.iter_children (out first, null) && model.iter_has_child (first)) {
				treeview.expand_row (model.get_path (first), false);
			} else {
				model.row_has_child_toggled.connect (first_row_has_child);
			}

			model.bind_property ("max-depth", rings_chart, "max-depth", BindingFlags.SYNC_CREATE);
			model.bind_property ("max-depth", treemap, "max-depth", BindingFlags.SYNC_CREATE);
			treemap.set_model_with_columns (model,
			                                Scanner.Columns.DISPLAY_NAME,
			                                Scanner.Columns.SIZE,
			                                Scanner.Columns.PARSE_NAME,
			                                Scanner.Columns.PERCENT,
			                                Scanner.Columns.ELEMENTS, null);
			rings_chart.set_model_with_columns (model,
			                                    Scanner.Columns.DISPLAY_NAME,
			                                    Scanner.Columns.SIZE,
			                                    Scanner.Columns.PARSE_NAME,
			                                    Scanner.Columns.PERCENT,
			                                    Scanner.Columns.ELEMENTS, null);
		}

		public void scan_directory (File directory) {
			if (!check_dir (directory)) {
				return;
			}

			disable_drop ();

			scanner = new ThreadedScanner ();
			scanner.scan (directory);

			set_model (scanner);

			enable_drop ();
		}
	}
}
