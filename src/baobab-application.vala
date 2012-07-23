/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
        static Application baobab;

        private const GLib.ActionEntry[] action_entries = {
            { "quit", on_quit_activate }
        };

        Settings desktop_settings;
        Settings prefs_settings;
        Settings ui_settings;

        protected override void activate () {
            new Window (this);
        }

        protected override void open (File[] files, string hint) {
            foreach (var file in files) {
                var window = new Window (this);
                window.scan_directory (file);
            }
        }

        public static HashTable<File, unowned File> get_excluded_locations () {
            var app = baobab;

            var excluded_locations = new HashTable<File, unowned File> (File.hash, File.equal);
            excluded_locations.add (File.new_for_path ("/proc"));
            excluded_locations.add (File.new_for_path ("/sys"));
            excluded_locations.add (File.new_for_path ("/selinux"));

            var home = File.new_for_path (Environment.get_home_dir ());
            excluded_locations.add (home.get_child (".gvfs"));

            var root = File.new_for_path ("/");
            foreach (var uri in app.prefs_settings.get_value ("excluded-uris")) {
                var file = File.new_for_uri ((string) uri);
                if (!file.equal (root)) {
                    excluded_locations.add (file);
                }
            }

            return excluded_locations;
        }

        protected override void startup () {
            base.startup ();

            baobab = this;

            // Settings
            ui_settings = new Settings ("org.gnome.baobab.ui");
            prefs_settings = new Settings ("org.gnome.baobab.preferences");
            desktop_settings = new Settings ("org.gnome.desktop.interface");

            // Menus: in gnome shell we just use the app menu, since the remaining
            // items are too few to look ok in a menubar and they are not essential
            var gtk_settings = Gtk.Settings.get_default ();
            var builder = new Gtk.Builder ();
            try {
                builder.add_from_resource ("/org/gnome/baobab/ui/baobab-menu.ui");
            } catch (Error e) {
                error ("loading menu builder file: %s", e.message);
            }
            if (gtk_settings.gtk_shell_shows_app_menu) {
                var app_menu = builder.get_object ("appmenu") as MenuModel;
                set_app_menu (app_menu);
            } else {
                var menubar = builder.get_object ("menubar") as MenuModel;
                set_menubar (menubar);
            }
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

            add_action_entries (action_entries, this);
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

        void on_quit_activate () {
            quit ();
        }
    }
}
