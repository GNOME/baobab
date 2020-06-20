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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-folder-display.ui")]
    public class FolderDisplay : Gtk.Grid {
        static construct {
            set_css_name ("folder-display");
        }

        [GtkChild]
        private Gtk.Label folder_name_primary;
        [GtkChild]
        private Gtk.Label folder_name_secondary;
        [GtkChild]
        private Gtk.Label folder_size;
        [GtkChild]
        private Gtk.Label folder_elements;
        [GtkChild]
        private Gtk.Label folder_time;

        Location location_;
        public Location location {
            set {
                location_ = value;

                set_name_from_location ();
                folder_size.label = "";
                folder_elements.label = "";
                folder_time.label = "";
            }

            get {
                return location_;
            }
        }

        public new Gtk.TreePath path {
            set {
                Gtk.TreeIter iter;
                location.scanner.get_iter (out iter, value);

                string name;
                string display_name;
                uint64 size;
                int elements;
                uint64 time;

                location.scanner.get (iter,
                           Scanner.Columns.NAME, out name,
                           Scanner.Columns.DISPLAY_NAME, out display_name,
                           Scanner.Columns.SIZE, out size,
                           Scanner.Columns.ELEMENTS, out elements,
                           Scanner.Columns.TIME_MODIFIED, out time);

                if (value.get_depth () == 1) {
                    set_name_from_location ();
                } else {
                    folder_name_primary.label = format_name (display_name, name);
                    folder_name_secondary.label = "";
                }
                folder_size.label = format_size (size);
                folder_elements.label = format_items (elements);
                folder_time.label = format_time_approximate (time);
            }
        }

        void set_name_from_location () {
            folder_name_primary.label = location.name;
            folder_name_secondary.label = location.file.get_parse_name ();
        }
    }
}
