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

    public class Application : Adw.Application {

        private Window window;

        static bool all_file_systems;

        const OptionEntry[] option_entries = {
            { "all-file-systems", 'a', 0, OptionArg.NONE, ref all_file_systems,
              N_("Do not skip directories on different file systems. Ignored if DIRECTORY is not specified."), null },
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

            var scan_flags = all_file_systems ? ScanFlags.NONE : ScanFlags.EXCLUDE_MOUNTS;
            window.scan_directory (files[0], scan_flags);
        }

        void ensure_window () {
            if (window == null) {
                window = new Window (this);
            }
        }

        public static new Application get_default () {
            return (Application) GLib.Application.get_default ();
        }

        public GenericSet<string> get_excluded_locations () {
            var excluded_locations = new GenericSet<string> (str_hash, str_equal);

            var prefs_settings = new Settings ("org.gnome.baobab.preferences");
            foreach (var uri in prefs_settings.get_strv ("excluded-uris")) {
                excluded_locations.add (uri);
            }

            return excluded_locations;
        }

        protected override void startup () {
            base.startup ();

            set_accels_for_action ("win.show-preferences", { "<Primary>comma" });
            set_accels_for_action ("win.show-primary-menu", { "F10" });
            set_accels_for_action ("win.scan-folder", { "<Primary>o" });
            set_accels_for_action ("win.reload", { "<Primary>r" });
            set_accels_for_action ("win.help", { "F1" });
            set_accels_for_action ("app.quit", { "<Primary>q" });
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

            set_resource_base_path("/org/gnome/baobab/");

            add_main_option_entries (option_entries);
            set_option_context_parameter_string ("[DIRECTORY]");

            add_action_entries (action_entries, this);
        }

        void on_quit_activate () {
            quit ();
        }
    }
}
