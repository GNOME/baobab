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
	public class Application : Gtk.Application {
		Settings desktop_settings;
		Settings prefs_settings;
		Settings ui_settings;

		static Application baobab;

		protected override void activate () {
			new Window (this);
		}

		protected override void open (File[] files, string hint) {
			foreach (var file in files) {
				var window = new Window (this);
				window.scan_directory (file);
			}
		}

		static void add_excluded_location (HashTable<File, unowned File> table, File file) {
			table.insert (file, file);
		}

		public static HashTable<File, unowned File> get_excluded_locations () {
			var app = baobab;

			var excluded_locations = new HashTable<File, unowned File> (file_hash, file_equal);
			add_excluded_location (excluded_locations, File.new_for_path ("/proc"));
			add_excluded_location (excluded_locations, File.new_for_path ("/sys"));
			add_excluded_location (excluded_locations, File.new_for_path ("/selinux"));
			add_excluded_location (excluded_locations, File.new_for_path ("/selinux"));

			var home = File.new_for_path (Environment.get_home_dir ());
			add_excluded_location (excluded_locations, home.get_child (".gvfs"));

			var root = File.new_for_path ("/");
			foreach (var uri in app.prefs_settings.get_value ("excluded-uris")) {
				var file = File.new_for_uri ((string) uri);
				if (!file.equal (root)) {
					excluded_locations.insert (file, file);
				}
			}

			return excluded_locations;
		}

		protected override void startup () {
			base.startup ();

			baobab = this;

			ui_settings = new Settings ("org.gnome.baobab.ui");
			prefs_settings = new Settings ("org.gnome.baobab.preferences");
			desktop_settings = new Settings ("org.gnome.desktop.interface");
		}

		protected override bool local_command_line ([CCode (array_length = false, array_null_terminated = true)] ref unowned string[] arguments, out int exit_status) {
			if (arguments[1] == "-v") {
				print ("%s %s\n", Environment.get_application_name (), Config.VERSION);
				exit_status = 0;
				return true;
			}

			return base.local_command_line (ref arguments, out exit_status);
		}

		public Application () {
			Object (application_id: "org.gnome.baobab", flags: ApplicationFlags.HANDLES_OPEN);
		}

		public static Settings get_desktop_settings () {
			var app = baobab;
			return app.desktop_settings;
		}

		public static Settings get_prefs_settings () {
			var app = baobab;
			return app.prefs_settings;
		}

		public static Settings get_ui_settings () {
			var app = baobab;
			return app.ui_settings;
		}
	}
}
