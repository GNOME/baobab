/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Ryan Lortie <desrt@desrt.ca>
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
		Gtk.TreeModel? model;
		Gtk.Builder builder;

		public Window (Application app) {
			Object (application: app);

			// Build ourselves.
			builder = new Gtk.Builder ();
			try {
				builder.add_from_file (Config.PKGDATADIR + "/baobab-main-window.ui");
			} catch (Error e) {
				error ("loading main builder file: %s", e.message);
			}

			var ui_settings = Application.get_ui_settings ();

			// Setup the logic for switching between the chart types.
			var chart_type_combo = builder.get_object ("chart-type-combo");
			var charts_notebook = builder.get_object ("chart-notebook");
			chart_type_combo.bind_property ("active", charts_notebook, "page", BindingFlags.SYNC_CREATE);
			ui_settings.bind ("active-chart", chart_type_combo, "active-id", SettingsBindFlags.GET_NO_CHANGES);

			// Setup the logic for statusbar visibility.
			var statusbar = builder.get_object ("statusbar") as Gtk.Widget;
			statusbar.visible = ui_settings.get_boolean ("statusbar-visible");

			// Go live.
			add (builder.get_object ("window-contents") as Gtk.Widget);
			title = _("Disk Usage Analyzer");
			set_default_size (800, 500);
			show ();
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

		public void scan_directory (File directory) {
			if (!check_dir (directory)) {
				return;
			}

			var scanner = new ThreadedScanner ();
			scanner.scan (directory);
			model = scanner;
			var rings_chart = builder.get_object ("rings-chart") as Chart;
			var treemap = builder.get_object ("treemap") as Chart;
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
			var treeview = builder.get_object ("treeview") as Gtk.TreeView;
			treeview.model = model;
		}
	}
}
