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
    public class FolderDisplay : Adw.Bin {
        [GtkChild]
        public unowned Gtk.ColumnView columnview;
        [GtkChild]
        public unowned Gtk.ColumnViewColumn folder_column;
        [GtkChild]
        public unowned Gtk.ColumnViewColumn size_column;
        [GtkChild]
        public unowned Gtk.ColumnViewColumn contents_column;
        [GtkChild]
        public unowned Gtk.ColumnViewColumn time_modified_column;
        [GtkChild]
        public unowned Gtk.SortListModel columnview_sort_model;

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

                var results = new Scanner.Results.empty ();
                results.display_name = location.name;
                var list_store = new ListStore (typeof (Scanner.Results));
                list_store.append (results);
                columnview_sort_model.model = list_store;

                location_progress_handler = location_.progress.connect (() => {
                    results.size = location_.scanner.total_size;
                    results.elements = location_.scanner.total_elements;
                });
            }

            get {
                return location_;
            }
        }

        Scanner.Results path_;
        public new Scanner.Results path {
            set {
                path_ = value;

                var list_store = new ListStore (typeof (Scanner.Results));
                list_store.append (path);
                columnview_sort_model.model = list_store;
            }
            get {
                return path_;
            }
        }

        public Gtk.Sorter sorter {
            get {
                return columnview.sorter;
            }
        }

        private Gtk.SizeGroup _size_size_group;
        public Gtk.SizeGroup size_size_group {
            get {
                if (_size_size_group == null) {
                    _size_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                }
                return _size_size_group;
            }
        }

        private Gtk.SizeGroup _folder_size_group;
        public Gtk.SizeGroup folder_size_group {
            get {
                if (_folder_size_group == null) {
                    _folder_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                }
                return _folder_size_group;
            }
        }

        private Gtk.SizeGroup _contents_size_group;
        public Gtk.SizeGroup contents_size_group {
            get {
                if (_contents_size_group == null) {
                    _contents_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                }
                return _contents_size_group;
            }
        }

        private Gtk.SizeGroup _time_modified_size_group;
        public Gtk.SizeGroup time_modified_size_group {
            get {
                if (_time_modified_size_group == null) {
                    _time_modified_size_group = new Gtk.SizeGroup (Gtk.SizeGroupMode.HORIZONTAL);
                }
                return _time_modified_size_group;
            }
        }

        construct {
            add_header_cells_to_size_groups ();
            columnview.sort_by_column (size_column, Gtk.SortType.DESCENDING);
        }

        void add_header_cells_to_size_groups () {
            // Hack poking into Gtk.ColumnView's internal structure to find the
            // header row's cells and add them to the size groups, nothing
            // guarantees this structure won't change and this hack works for
            // all GTK 4 version.
            // In GTK 4.14, the structure is like this:
            // GtkColumnView
            // ├── GtkColumnViewRowWidget     — header
            // │   ├── GtkColumnViewTitleCell — title
            // │   ┆
            // ╰── GtkColumnListView          — content
            var row_type = Type.from_name ("GtkColumnViewRowWidget");
            var title_type = Type.from_name ("GtkColumnViewTitle");
            for (var child = columnview.get_first_child (); child != null; child = child.get_next_sibling ()) {
                if (child.get_type () != row_type) {
                    continue;
                }

                var name_cell = child.get_first_child ();
                if (name_cell.get_type () != title_type) {
                    continue;
                }

                var size_cell = name_cell.get_next_sibling ();
                if (size_cell.get_type () != title_type) {
                    continue;
                }

                var contents_cell = size_cell.get_next_sibling ();
                if (contents_cell.get_type () != title_type) {
                    continue;
                }

                var time_modified_cell = contents_cell.get_next_sibling ();
                if (time_modified_cell.get_type () != title_type) {
                    continue;
                }

                folder_size_group.add_widget (name_cell.get_first_child ());
                size_size_group.add_widget (size_cell.get_first_child ());
                contents_size_group.add_widget (contents_cell.get_first_child ());
                time_modified_size_group.add_widget (time_modified_cell.get_first_child ());
            }
        }

        [GtkCallback]
        void on_columnview_activate () {
            activated ();
        }

        [GtkCallback]
        void on_columnview_notify_sorter () {
            notify_property ("sorter");
        }

        [GtkCallback]
        private void folder_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new FolderCell ();
            item.child = child;
            folder_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void folder_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var results = item.item as Scanner.Results;
            var child = item.child as FolderCell;
            child.item = results;
        }

        [GtkCallback]
        private void folder_cell_unbind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = item.child as FolderCell;
            child.item = null;
        }

        [GtkCallback]
        private void folder_cell_teardown (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            folder_size_group.remove_widget (item.child);
        }

        [GtkCallback]
        private void size_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new SizeCell ();
            item.child = child;
            size_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void size_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var results = item.item as Scanner.Results;
            var child = item.child as SizeCell;
            child.item = results;
        }

        [GtkCallback]
        private void size_cell_unbind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = item.child as SizeCell;
            child.item = null;
        }

        [GtkCallback]
        private void size_cell_teardown (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            size_size_group.remove_widget (item.child);
        }

        [GtkCallback]
        private void contents_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new ContentsCell ();
            item.child = child;
            contents_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void contents_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var results = item.item as Scanner.Results;
            var child = item.child as ContentsCell;
            child.item = results;
        }

        [GtkCallback]
        private void contents_cell_unbind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = item.child as ContentsCell;
            child.item = null;
        }

        [GtkCallback]
        private void contents_cell_teardown (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            contents_size_group.remove_widget (item.child);
        }

        [GtkCallback]
        private void time_modified_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new TimeModifiedCell ();
            item.child = child;
            time_modified_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void time_modified_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var results = item.item as Scanner.Results;
            var child = item.child as TimeModifiedCell;
            child.item = results;
        }

        [GtkCallback]
        private void time_modified_cell_unbind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = item.child as TimeModifiedCell;
            child.item = null;
        }

        [GtkCallback]
        private void time_modified_cell_teardown (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            time_modified_size_group.remove_widget (item.child);
        }
    }
}
