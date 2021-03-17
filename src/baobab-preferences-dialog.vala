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
    class ExcludedRow : Gtk.ListBoxRow {
        [GtkChild]
        private unowned Gtk.Label name_label;
        [GtkChild]
        private unowned Gtk.Button remove_button;

        public signal void removed ();

        public ExcludedRow (File file) {
            if (file.has_uri_scheme ("file")) {
                name_label.label = file.get_path ();
            } else {
                name_label.label = file.get_uri ();
            }
            remove_button.clicked.connect (() => { removed (); });
        }
    }

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-preferences-dialog.ui")]
    public class PreferencesDialog : Hdy.PreferencesWindow {
        [GtkChild]
        private unowned Gtk.ListBox excluded_list_box;

        private Settings prefs_settings;

        construct {
            prefs_settings = new Settings ("org.gnome.baobab.preferences");

            excluded_list_box.row_activated.connect (() => {
                // The only activatable row is "Add location"
                var file_chooser = new Gtk.FileChooserDialog (_("Select Location to Ignore"), this,
                                                             Gtk.FileChooserAction.SELECT_FOLDER,
                                                              _("_Cancel"), Gtk.ResponseType.CANCEL,
                                                              _("_Open"), Gtk.ResponseType.OK);

                file_chooser.local_only = false;
                file_chooser.modal = true;
                file_chooser.set_default_response (Gtk.ResponseType.OK);

                file_chooser.response.connect ((response) => {
                    if (response == Gtk.ResponseType.OK) {
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
            excluded_list_box.foreach ((widget) => { widget.destroy (); });

            foreach (var uri in prefs_settings.get_strv ("excluded-uris")) {
                var file = File.new_for_uri (uri);
                var row = new ExcludedRow (file);
                excluded_list_box.insert (row, -1);

                row.removed.connect (() => {
                    remove_uri (uri);
                    populate ();
                });
            }

            var label = new Gtk.Label (_("Add Location…"));
            label.margin = 12;
            label.show ();
            excluded_list_box.insert (label, -1);
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
