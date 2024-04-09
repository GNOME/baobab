/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2020  Stefano Facchini <stefano.facchini@gmail.com>
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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-excluded-row.ui")]
    class ExcludedRow : Adw.ActionRow {
        [GtkChild]
        private unowned Gtk.Button remove_button;

        public signal void removed ();

        public ExcludedRow (File file) {
            if (file.has_uri_scheme ("file")) {
                title = file.get_path ();
            } else {
                title = file.get_uri ();
            }
            remove_button.clicked.connect (() => { removed (); });
        }
    }

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-preferences-dialog.ui")]
    public class PreferencesDialog : Adw.PreferencesDialog {
        [GtkChild]
        private unowned Gtk.ListBox excluded_list_box;

        private Settings prefs_settings;

        construct {
            prefs_settings = new Settings ("org.gnome.baobab.preferences");

            excluded_list_box.row_activated.connect (() => {
                // The only activatable row is "Add location"
                var file_chooser = new Gtk.FileChooserNative (_("Select Location to Ignore"),
                                                              (Gtk.Window) this.get_root (),
                                                               Gtk.FileChooserAction.SELECT_FOLDER,
                                                               null, null);

                file_chooser.modal = true;

                file_chooser.response.connect ((response) => {
                    if (response == Gtk.ResponseType.ACCEPT) {
                        var uri = file_chooser.get_file ().get_uri ();
                        add_uri (uri);
                        populate ();
                    }
                    file_chooser.destroy ();
                });

                file_chooser.show ();
            });

            populate ();
        }

        void populate () {
            for (Gtk.Widget? child = excluded_list_box.get_first_child (); child != null; child = excluded_list_box.get_first_child ()) {
                excluded_list_box.remove (child);
            }

            foreach (var uri in prefs_settings.get_strv ("excluded-uris")) {
                var file = File.new_for_uri (uri);
                var row = new ExcludedRow (file);
                excluded_list_box.append (row);

                row.removed.connect (() => {
                    remove_uri (uri);
                    populate ();
                });
            }

            var button_row = new Adw.ButtonRow ();

            button_row.title = (_("_Add Location"));
            button_row.start_icon_name = ("list-add-symbolic");
            button_row.use_underline = true;

            excluded_list_box.append (button_row);
        }

        void add_uri (string uri) {
            var uris = prefs_settings.get_strv ("excluded-uris");
            if (! (uri in uris)) {
                uris += uri;
                prefs_settings.set_strv ("excluded-uris", uris);
            }
        }

        void remove_uri (string uri) {
            string[] uris = {};
            foreach (var uri_iter in prefs_settings.get_strv ("excluded-uris")) {
                if (uri_iter != uri) {
                    uris += uri_iter;
                }
            }
            prefs_settings.set_strv ("excluded-uris", uris);
        }
    }
}
