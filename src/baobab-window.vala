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
		Gtk.Widget chart_type_combo;
		Chart rings_chart;
		Chart treemap_chart;
		Scanner? scanner;

		static Gdk.Cursor busy_cursor;

		private const GLib.ActionEntry[] action_entries = {
			{ "scan-home", on_scan_home_activate },
			{ "scan-filesystem", on_scan_filesystem_activate },
			{ "scan-folder", on_scan_folder_activate },
			{ "scan-remote", on_scan_remote_activate },
			{ "stop", on_stop_activate },
			{ "reload", on_reload_activate },
			{ "show-toolbar", on_show_toolbar },
			{ "show-allocated", on_show_allocated },
			{ "expand-all", on_expand_all },
			{ "collapse-all", on_collapse_all },
			{ "help", on_help_activate },
			{ "about", on_about_activate }
		};

		protected struct ActionState {
			string name;
			bool enable;
		}

		private const ActionState[] actions_while_scanning = {
			{ "scan-home", false },
			{ "scan-filesystem", false },
			{ "scan-folder", false },
			{ "scan-remote", false },
			{ "stop", true },
			{ "reload", false },
			{ "show-allocated", false },
			{ "expand-all", false },
			{ "collapse-all", false }
		};

		private enum DndTargets {
			URI_LIST
		}

		private const Gtk.TargetEntry dnd_target_list[1] = {
		    {"text/uri-list", 0, DndTargets.URI_LIST}
		};

		public Window (Application app) {
			Object (application: app);

			busy_cursor = new Gdk.Cursor (Gdk.CursorType.WATCH);

			add_action_entries (action_entries, this);

			// Build ourselves.
			var builder = new Gtk.Builder ();
			try {
				builder.add_from_file (Config.PKGDATADIR + "/baobab-main-window.ui");
			} catch (Error e) {
				error ("loading main builder file: %s", e.message);
			}

			// Cache some objects from the builder.
			chart_type_combo = builder.get_object ("chart-type-combo") as Gtk.Widget;
			rings_chart = builder.get_object ("rings-chart") as Chart;
			treemap_chart = builder.get_object ("treemap-chart") as Chart;
			treeview = builder.get_object ("treeview") as Gtk.TreeView;

			var ui_settings = Application.get_ui_settings ();

			// Setup the logic for switching between the chart types.
			var charts_notebook = builder.get_object ("chart-notebook");
			chart_type_combo.bind_property ("active", charts_notebook, "page", BindingFlags.SYNC_CREATE);
			ui_settings.bind ("active-chart", chart_type_combo, "active-id", SettingsBindFlags.GET_NO_CHANGES);

			// Setup drag-n-drop
			drag_data_received.connect(on_drag_data_received);
			enable_drop ();

			add (builder.get_object ("window-contents") as Gtk.Widget);
			title = _("Disk Usage Analyzer");
			set_default_size (800, 500);

			set_busy (false);

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
				scanner.cancel ();
			}
		}

		void on_reload_activate () {
			if (scanner != null) {
				scan_directory (scanner.directory);
			}
		}

		void on_show_toolbar () {
		}

		void on_show_allocated () {
		}

		void on_expand_all () {
			treeview.expand_all ();
		}

		void on_collapse_all () {
			treeview.collapse_all ();
		}

		void on_help_activate () {
			try {
				Gtk.show_uri (get_screen (), "help:baobab", Gtk.get_current_event_time ());
			} catch (Error e) {
				warning ("Failed to show help: %s", e.message);
			}
		}

		void on_about_activate () {
			const string authors[] = {
				"Ryan Lortie <desrt@desrt.ca>",
				"Fabio Marzocca <thesaltydog@gmail.com>",
				"Paolo Borelli <pborelli@gnome.com>",
				"Benoît Dejean <benoit@placenet.org>",
				"Igalia (rings-chart and treemap widget) <www.igalia.com>"
			};

			const string copyright = "Copyright \xc2\xa9 2005-2011 Fabio Marzocca, Paolo Borelli, Benoît Dejean, Igalia\n" +
			                         "Copyright \xc2\xa9 2011-2012 Ryan Lortie, Paolo Borelli\n";

			Gtk.show_about_dialog (this,
			                       "program-name", _("Baobab"),
			                       "logo-icon-name", "baobab",
			                       "version", Config.VERSION,
			                       "comments", "A graphical tool to analyze disk usage.",
			                       "copyright", copyright,
			                       "license-type", Gtk.License.GPL_2_0,
			                       "wrap-license", false,
			                       "authors", authors,
			                       "translator-credits", _("translator-credits"),
			                       null);
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

		void set_busy (bool busy) {
			Gdk.Cursor? cursor = null;

			if (busy) {
				cursor = busy_cursor;
				disable_drop ();
				rings_chart.freeze_updates();
				treemap_chart.freeze_updates ();
				chart_type_combo.set_sensitive (false);
			} else {
				enable_drop ();
				rings_chart.thaw_updates();
				treemap_chart.thaw_updates ();
				chart_type_combo.set_sensitive (true);
			}

			var window = get_window ();
			if (window != null) {
				window.set_cursor (cursor);
			}

			foreach (ActionState action_state in actions_while_scanning) {
				var action = lookup_action (action_state.name) as SimpleAction;
				action.set_enabled (busy == action_state.enable);
			}
		}

		public bool check_dir (File directory) {
			//if (Application.is_excluded_location (directory)) {
			//	message("", _("Cannot check an excluded folder!"), Gtk.MessageType.INFO);
			//	return false;
			//}

			try {
				var info = directory.query_info (FileAttribute.STANDARD_TYPE, FileQueryInfoFlags.NONE, null);
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
			model.bind_property ("max-depth", treemap_chart, "max-depth", BindingFlags.SYNC_CREATE);
			treemap_chart.set_model_with_columns (model,
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

			set_busy (true);
			scanner.completed.connect(() => {
				set_busy (false);
			});
		}

		public void scan_directory (File directory) {
			if (!check_dir (directory)) {
				return;
			}

			scanner = new ThreadedScanner (directory);
			set_model (scanner);

			scanner.scan ();
		}
	}
}
