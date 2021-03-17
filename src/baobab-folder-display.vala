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
    public class FolderDisplay : Gtk.TreeView {
        [GtkChild]
        public unowned Gtk.TreeViewColumn folder_column;
        [GtkChild]
        public unowned Gtk.TreeViewColumn size_column;
        [GtkChild]
        public unowned Gtk.TreeViewColumn contents_column;
        [GtkChild]
        public unowned Gtk.TreeViewColumn time_modified_column;
        [GtkChild]
        private unowned Gtk.CellRendererPixbuf folder_column_icon_renderer;

        construct {
            row_activated.connect (() => { activated (); });
            model = create_model ();
        }

        public signal void activated ();

        private ulong location_progress_handler;

        Location location_;
        public Location location {
            set {
                if (location_progress_handler > 0) {
                    SignalHandler.disconnect (location_, location_progress_handler);
                    location_progress_handler = 0;
                }

                location_ = value;

                var list_store = (Gtk.ListStore) model;
                list_store.clear ();
                list_store.insert_with_values (null, -1,
                           Scanner.Columns.NAME, location.name);

                folder_column_icon_renderer.visible = false;

                location_progress_handler = location_.progress.connect (() => {
                    Gtk.TreeIter iter;
                    list_store.get_iter_first (out iter);
                    list_store.set (iter,
                            Scanner.Columns.SIZE, location_.scanner.total_size,
                            Scanner.Columns.ELEMENTS, location_.scanner.total_elements);
                });
            }

            get {
                return location_;
            }
        }

        Gtk.TreePath path_;
        public new Gtk.TreePath path {
            set {
                path_ = value;

                Gtk.TreeIter iter;
                location.scanner.get_iter (out iter, value);

                string name;
                string display_name;
                uint64 size;
                int elements;
                uint64 time;
                Scanner.State state;

                location.scanner.get (iter,
                           Scanner.Columns.NAME, out name,
                           Scanner.Columns.DISPLAY_NAME, out display_name,
                           Scanner.Columns.SIZE, out size,
                           Scanner.Columns.ELEMENTS, out elements,
                           Scanner.Columns.TIME_MODIFIED, out time,
                           Scanner.Columns.STATE, out state);

                if (value.get_depth () == 1) {
                    name = location.name;
                    folder_column_icon_renderer.visible = false;
                } else {
                    folder_column_icon_renderer.visible = true;
                }

                var list_store = (Gtk.ListStore) model;
                list_store.clear ();
                list_store.insert_with_values (null, -1,
                           Scanner.Columns.NAME, name,
                           Scanner.Columns.DISPLAY_NAME, display_name,
                           Scanner.Columns.SIZE, size,
                           Scanner.Columns.ELEMENTS, elements,
                           Scanner.Columns.TIME_MODIFIED, time,
                           Scanner.Columns.STATE, state);
            }
            get {
                return path_;
            }
        }

        Gtk.ListStore create_model () {
            var list_store = new Gtk.ListStore.newv (new Type[] {
                typeof (string),  // NAME
                typeof (double),  // PERCENT
                typeof (uint64),  // SIZE
                typeof (uint64),  // TIME_MODIFIED
                typeof (string),  // DISPLAY_NAME
                typeof (int),     // ELEMENTS
                typeof (Scanner.State)    // STATE
            });
            list_store.set_sort_column_id (Scanner.Columns.SIZE, Gtk.SortType.DESCENDING);

            return list_store;
        }
    }
}
