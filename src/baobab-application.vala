/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Ryan Lortie <desrt@desrt.ca>
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
 * Copyright (C) 2012  Stefano Facchini <stefano.facchini@gmail.com>
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

        private Window window;

        const OptionEntry[] option_entries = {
            { "version", 'v', 0, OptionArg.NONE, null, N_("Print version information and exit"), null },
            { null }
        };

        const GLib.ActionEntry[] action_entries = {
            { "quit", on_quit_activate }
        };

        protected override void activate () {
            ensure_window ();
            window.present ();
        }

        protected override void open (File[] files, string hint) {
            ensure_window ();
            window.scan_directory (files[0], ScanFlags.EXCLUDE_MOUNTS);
        }

        void ensure_window () {
            if (window == null) {
                window = new Window (this);
            }
        }

        public static new Application get_default () {
            return (Application) GLib.Application.get_default ();
        }

        public GenericSet<File> get_excluded_locations () {
            var excluded_locations = new GenericSet<File> (File.hash, File.equal);
            excluded_locations.add (File.new_for_path ("/proc"));
            excluded_locations.add (File.new_for_path ("/sys"));
            excluded_locations.add (File.new_for_path ("/selinux"));

            var home = File.new_for_path (Environment.get_home_dir ());
            excluded_locations.add (home.get_child (".gvfs"));

            var root = File.new_for_path ("/");
            var prefs_settings = new Settings ("org.gnome.baobab.preferences");
            foreach (var uri in prefs_settings.get_value ("excluded-uris")) {
                var file = File.new_for_uri ((string) uri);
                if (!file.equal (root)) {
                    excluded_locations.add (file);
                }
            }

            return excluded_locations;
        }

        protected override void startup () {
            base.startup ();

            // Load custom CSS
            var css_provider = new Gtk.CssProvider ();
            var css_file = File.new_for_uri ("resource:///org/gnome/baobab/baobab.css");
            try {
              css_provider.load_from_file (css_file);
            } catch (Error e) {
                warning ("loading css: %s", e.message);
            }
            Gtk.StyleContext.add_provider_for_screen (Gdk.Screen.get_default (), css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

            set_accels_for_action ("win.scan-folder", { "<Primary>o" });
            set_accels_for_action ("win.reload", { "<Primary>r" });
        }

        protected override int handle_local_options (GLib.VariantDict options) {
            if (options.contains("version")) {
                print ("%s %s\n", Environment.get_application_name (), Config.VERSION);
                return 0;
            }

            return -1; 
        }

        public Application () {
            Object (application_id: "org.gnome.baobab", flags: ApplicationFlags.HANDLES_OPEN);

            add_main_option_entries (option_entries);
            add_action_entries (action_entries, this);
        }

        void on_quit_activate () {
            quit ();
        }
    }
}
