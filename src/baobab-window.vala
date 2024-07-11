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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-main-window.ui")]
    public class Window : Adw.ApplicationWindow {
        [GtkChild]
        private unowned Gtk.EventControllerFocus focus_controller;
        [GtkChild]
        private unowned Gtk.DropTarget drop_target;
        [GtkChild]
        private unowned Pathbar pathbar;
        [GtkChild]
        private unowned Adw.NavigationView nav_view;
        [GtkChild]
        private unowned Gtk.Widget home_page;
        [GtkChild]
        private unowned Gtk.Widget result_page;
        [GtkChild]
        private unowned Adw.ToastOverlay toast_overlay;
        [GtkChild]
        private unowned Adw.Banner banner;
        [GtkChild]
        private unowned LocationList location_list;
        [GtkChild]
        private unowned FolderDisplay folder_display;
        [GtkChild]
        private unowned Gtk.ColumnView columnview;
        [GtkChild]
        private unowned Gtk.SingleSelection columnview_selection;
        [GtkChild]
        private unowned Gtk.SortListModel columnview_sort_model;
        [GtkChild]
        private unowned Gtk.PopoverMenu treeview_popover_menu;
        [GtkChild]
        private unowned Adw.ViewStack chart_stack;
        [GtkChild]
        private unowned Gtk.Stack spinner_stack;
        [GtkChild]
        private unowned Adw.ViewSwitcher chart_stack_switcher;
        [GtkChild]
        private unowned Chart rings_chart;
        [GtkChild]
        private unowned Chart treemap_chart;
        [GtkChild]
        private unowned Adw.Spinner spinner;

        private Location? active_location = null;
        private bool is_busy = false;
        private ulong scan_completed_handler = 0;
        private uint scanning_progress_id = 0;

        static Gdk.Cursor busy_cursor;

        private const GLib.ActionEntry[] action_entries = {
            { "show-primary-menu", on_show_primary_menu_activate, null, "false", null },
            { "scan-folder", on_scan_folder_activate },
            { "reload", on_reload_activate },
            { "clear-recent", on_clear_recent },
            { "show-preferences", on_show_preferences },
            { "help", on_help_activate },
            { "about", on_about_activate },
            { "show-treeview-popover", on_show_treeview_popover },
            { "treeview-expand-row", on_treeview_expand_row },
            { "treeview-collapse-row", on_treeview_collapse_row },
            { "treeview-go-up", go_up_treeview },
            { "treeview-open-folder", on_treeview_open_folder },
            { "treeview-copy", on_treeview_copy },
            { "treeview-trash", on_treeview_trash }
        };

        protected struct ActionState {
            string name;
            bool enable;
        }

        private const ActionState[] actions_while_scanning = {
            { "scan-folder", false },
        };

        public Window (Application app) {
            Object (application: app);

            if (busy_cursor == null) {
                busy_cursor = new Gdk.Cursor.from_name ("wait", null);
            }

            var ui_settings = new Settings ("org.gnome.baobab.ui");
            ui_settings.delay ();

            add_action_entries (action_entries, this);
            var action = ui_settings.create_action ("active-chart");
            add_action (action);

            location_list.location_activated.connect (location_activated);

            var builder = new Gtk.Builder.from_resource("/org/gnome/baobab/ui/baobab-treeview-menu.ui");
            GLib.MenuModel treeview_menu = (GLib.MenuModel) builder.get_object ("treeview_menu");

            hide_columnview_header ();
            treeview_popover_menu.set_menu_model (treeview_menu);

            ui_settings.bind ("active-chart", chart_stack, "visible-child-name", SettingsBindFlags.DEFAULT);
            chart_stack.destroy.connect (() => { Settings.unbind (chart_stack, "visible-child-name"); });

            rings_chart.item_activated.connect (on_chart_item_activated);
            treemap_chart.item_activated.connect (on_chart_item_activated);
            pathbar.item_activated.connect (on_pathbar_item_activated);
            folder_display.activated.connect (on_folder_display_activated);

            // Setup drag-n-drop
            drop_target.set_gtypes ({typeof (Gdk.FileList)});
            drop_target.on_drop.connect (on_drop);
            drop_target.accept.connect (on_drop_target_accept);

            // Setup window geometry saving
            if (ui_settings.get_boolean ("is-maximized")) {
                maximize ();
            }

            int width, height;
            ui_settings.get ("window-size", "(ii)", out width, out height);
            set_default_size (width, height);

            ui_settings.bind ("is-maximized", this, "maximized", GLib.SettingsBindFlags.SET);

            close_request.connect (() => {
                if (!maximized) {
                    int win_width, win_height;
                    get_default_size (out win_width, out win_height);
                    ui_settings.set ("window-size", "(ii)", win_width, win_height);
                }
                ui_settings.apply ();
                return false;
            });

            set_ui_state (home_page, false);

            focus_controller.enter.connect (() => {
                application.withdraw_notification ("scan-completed");
            });

            present ();
        }

        void on_show_primary_menu_activate (SimpleAction action) {
            var state = action.get_state ().get_boolean ();
            action.set_state (new Variant.boolean (!state));
        }

        [GtkCallback]
        void results_hidden () {
            cancel_scan ();
            banner.revealed = false;
            set_ui_state (home_page, false);
        }

        void on_scan_folder_activate () {
            var file_chooser = new Gtk.FileChooserNative (_("Select Folder"), this,
                                                          Gtk.FileChooserAction.SELECT_FOLDER,
                                                          null, null);

            file_chooser.modal = true;

            file_chooser.add_choice ("recurse",
                                    _("Recursively analyze mount points"),
                                    null, null);

            file_chooser.response.connect ((response) => {
                if (response == Gtk.ResponseType.ACCEPT) {
                    bool recurse = bool.parse (file_chooser.get_choice ("recurse"));
                    var flags = recurse ? ScanFlags.NONE : ScanFlags.EXCLUDE_MOUNTS;
                    scan_directory (file_chooser.get_file (), flags);
                }
                file_chooser.destroy ();
            });

            file_chooser.show ();
        }

        void location_activated (Location location) {
            set_busy (true);
            location.mount_volume.begin ((location_, res) => {
                try {
                    location.mount_volume.end (res);
                    scan_location (location);
                } catch (Error e) {
                    set_busy (false);
                    warning ("Could not analyze volume: %s\n", e.message);
                    toast (_("Could not analyze volume"));
                }
            });
        }

        void on_reload_activate () {
            scan_location (active_location, true);
        }

        void cancel_scan () {
            if (active_location != null && active_location.scanner != null) {
                active_location.scanner.cancel ();
            }

            if (scan_completed_handler > 0) {
                active_location.scanner.disconnect (scan_completed_handler);
                scan_completed_handler = 0;
            }

            active_location = null;
        }

        void on_clear_recent () {
            location_list.clear_recent ();
        }

        void on_show_preferences () {
            var dialog = new PreferencesDialog ();
            dialog.present (this);
        }

        void on_help_activate () {
            Gtk.show_uri (this, "help:baobab", Gdk.CURRENT_TIME);
        }

        void on_about_activate () {
            const string developers[] = {
                "Ryan Lortie <desrt@desrt.ca>",
                "Fabio Marzocca <thesaltydog@gmail.com>",
                "Paolo Borelli <pborelli@gnome.com>",
                "Stefano Facchini <stefano.facchini@gmail.com>",
                "Benoît Dejean <benoit@placenet.org>",
                "Igalia (rings-chart and treemap widget) <www.igalia.com>",
                null
            };

            const string copyright = "Copyright \xc2\xa9 2005-2011 Fabio Marzocca, Paolo Borelli, Benoît Dejean, Igalia\n" +
                                     "Copyright \xc2\xa9 2011-2012 Ryan Lortie, Paolo Borelli, Stefano Facchini\n";

            var about = new Adw.AboutDialog() {
                application_name = _("Disk Usage Analyzer"),
                application_icon = "org.gnome.baobab",
                developer_name = _("The GNOME Project"),
                version = Config.VERSION,
                website = "https://apps.gnome.org/Baobab/",
                issue_url = "https://gitlab.gnome.org/GNOME/baobab/-/issues/new",
                copyright = copyright,
                license_type = Gtk.License.GPL_2_0,
                developers = developers,
                translator_credits = _("translator-credits"),
            };

            about.present(this);
        }

        void on_chart_item_activated (Chart chart, Scanner.Results item) {
            reroot_treeview (item);
        }

        void on_pathbar_item_activated (Pathbar pathbar, Scanner.Results path) {
            reroot_treeview (path);
        }

        void on_folder_display_activated () {
            go_up_treeview ();
        }

        void go_up_treeview () {
            var path = folder_display.path;
            if (path != null && path.get_depth () > 1) {
                reroot_treeview (path.parent);
            }
        }

        bool on_drop_target_accept (Gtk.DropTarget target, Gdk.Drop drop) {
            return !is_busy;
        }

        bool on_drop (Gtk.DropTarget target, Value value, double x, double y) {
            if (value.type () == typeof (Gdk.FileList)) {
                unowned var files = (SList<File>) value.get_boxed ();
                File dir = null;
                if (files != null && files.length () == 1) {
                    dir = files.nth (0).data;
                }

                if (dir != null) {
                    scan_directory (dir);
                }

                return true;
            }

            return false;
        }

        bool show_treeview_popover (Gtk.PopoverMenu popover, int x, int y) {
            Gdk.Rectangle  rect = { x, y, 0, 0, };
            popover.set_pointing_to (rect);
            popover.popup ();
            return Gdk.EVENT_STOP;
        }

        public void open_item (Scanner.Results results) {
            var file = active_location.scanner.get_file (results);
            try {
                AppInfo.launch_default_for_uri (file.get_uri (), null);
            } catch (Error e) {
                warning ("Failed to open file: %s\n", e.message);
                toast (_("Failed to open file"));
            }
        }

        public void copy_path (Scanner.Results results) {
            var parse_name = active_location.scanner.get_file (results).get_parse_name ();
            var clipboard = get_clipboard ();
            clipboard.set_text (parse_name);
        }

        public void trash_file (Scanner.Results results) {
            var file = active_location.scanner.get_file (results);
            try {
                file.trash ();
            } catch (Error e) {
                if (!e.matches (GLib.IOError.quark (), GLib.IOError.NOT_FOUND)) {
                    warning ("Failed to move to file to the trash: %s", e.message);
                    toast (_("Failed to trash file"));
                    return;
                }
            }

            var parent = results.parent;
            uint position = 0;
            if (results.parent.children_list_store.find (results, out position)) {
                results.parent.children_list_store.remove (position);
            }
        }

        bool columnview_selection_find (Object? item, bool passthrough, out uint position) {
            // Naive search, there likely are ways to improve this,
            // e.g. using the columnview's sort to do a sort-based
            // search, or having a way to map an object to its
            // position directly into the model.

            // We want to scroll to a row, select it and focus it, sadly
            // Gtk.ColumnView.scroll_to() only lets us scroll to a row's
            // position, so we need to get it. Gtk.TreeListRow.get_position()
            // gives us the position of the row in the original unsorted model,
            // and our Gtk.ColumnView gets a sorted version of it, so in order
            // to scroll to the row, we have to find its position in the sorted
            // model. This is a naive way of finding that position, we could
            // rely on the fact the rows are sorted to find its position in a
            // more efficient way, and GTK may offer a way to scroll to items
            // (rows) directly in the future, see:
            // https://gitlab.gnome.org/GNOME/gtk/-/issues/6747#note_2126025

            if (item == null) {
                return false;
            }

            for (uint i = 0; i < columnview_selection.n_items && i < uint.MAX; i++) {
                if (passthrough) {
                    // The item is a Scanner.Results contained by a Gtk.TreeListRow
                    var row = columnview_selection.get_item (i) as Gtk.TreeListRow;
                    if (row.item == item) {
                        position = i;
                        return true;
                    }
                } else {
                    // The item is a Gtk.TreeListRow containing a Scanner.Results
                    if (columnview_selection.get_item (i) == item) {
                        position = i;
                        return true;
                    }
                }
            }

            return false;
        }

        Scanner.Results? get_selected_item () {
            if (!(columnview_selection.selected_item is Gtk.TreeListRow)) {
                return null;
            }

            var row = columnview_selection.selected_item as Gtk.TreeListRow;
            return row.item as Scanner.Results?;
        }

        void on_show_treeview_popover () {
            show_treeview_popover (treeview_popover_menu,
                                   columnview.get_allocated_width () / 2,
                                   columnview.get_allocated_height () / 2);
        }

        void on_treeview_expand_row () {
            var selected_item = columnview_selection.selected_item;
            if (!(selected_item is Gtk.TreeListRow)) {
                return;
            }

            var selected_row = selected_item as Gtk.TreeListRow;
            selected_row.expanded = true;
        }

        void on_treeview_collapse_row () {
            var selected_item = columnview_selection.selected_item;
            if (!(selected_item is Gtk.TreeListRow)) {
                return;
            }

            var selected_row = selected_item as Gtk.TreeListRow;
            if (selected_row.expanded) {
                selected_row.expanded = false;
            } else if (selected_row.depth > 0) {
                uint position = 0;
                if (columnview_selection_find (selected_row.get_parent (), false, out position)) {
                    columnview.scroll_to (position,
                                          null,
                                          Gtk.ListScrollFlags.FOCUS | Gtk.ListScrollFlags.SELECT,
                                          null);
                }
            }
        }

        void on_treeview_open_folder () {
            var item = get_selected_item ();
            if (item != null)
                open_item (item);
        }

        void on_treeview_copy () {
            var item = get_selected_item ();
            if (item != null)
                copy_path (item);
        }

        void on_treeview_trash () {
            var item = get_selected_item ();
            if (item != null)
                trash_file (item);
        }

        void hide_columnview_header () {
            // Hack poking into Gtk.ColumnView's internal structure to find the
            // header row and hide it, nothing guarantees this structure won't
            // change and this hack works for all GTK 4 version.
            // In GTK 4.14, the structure is like this:
            // GtkColumnView
            // ├── GtkColumnViewRowWidget — header
            // ╰── GtkColumnListView      — content
            var row_type = Type.from_name ("GtkColumnViewRowWidget");
            for (var child = columnview.get_first_child (); child != null; child = child.get_next_sibling ()) {
                if (child.get_type () != row_type) {
                    continue;
                }

                child.visible = false;
            }
        }

        [GtkCallback]
        void treeview_right_click_gesture_pressed (int n_press, double x, double y) {
            var child = columnview.pick (x, y, Gtk.PickFlags.INSENSITIVE | Gtk.PickFlags.NON_TARGETABLE);
            var row = child.get_ancestor (Type.from_name ("GtkColumnViewRowWidget"));

            if (row == null) {
                return;
            }

            for (var cell = row.get_first_child (); cell != null; cell = cell.get_next_sibling ()) {
                var cell_content = cell.get_first_child ();

                if (cell_content == null ||
                    cell_content.get_type () != typeof (FileCell)) {
                    continue;
                }

                var file_cell = cell_content as FileCell;
                uint position = 0;

                if (columnview_selection_find (file_cell.item, true, out position)) {
                    columnview.scroll_to (position,
                                          null,
                                          Gtk.ListScrollFlags.FOCUS | Gtk.ListScrollFlags.SELECT,
                                          null);
                }
                break;
            }

            show_treeview_popover (treeview_popover_menu,  (int) x, (int) y);
        }

        [GtkCallback]
        void columnview_activate (uint position) {
            var row = columnview.model.get_object (position) as Gtk.TreeListRow;
            var path = row.item as Scanner.Results;
            reroot_treeview (path, true);
        }

        void reroot_treeview (Scanner.Results path, bool select_first = false) {
            if (path.is_empty) {
                return;
            }

            folder_display.path = path;
            rings_chart.tree_root = path;
            treemap_chart.tree_root = path;
            pathbar.path = path;

            columnview_sort_model.model = path.create_tree_model ();

            if (select_first) {
                columnview_selection.selected = 0;
            }
        }

        void toast (string title) {
            var toast = new Adw.Toast (title);
            toast_overlay.add_toast (toast);
        }

        void set_busy (bool busy) {
            Gdk.Cursor? new_cursor = null;
            is_busy = busy;

            if (busy) {
                new_cursor = busy_cursor;
                chart_stack_switcher.sensitive = false;
                spinner_stack.visible_child = spinner;
                pathbar.sensitive = false;
            } else {
                spinner_stack.visible_child = chart_stack;
                chart_stack_switcher.sensitive = true;
                pathbar.sensitive = true;
            }

            this.cursor = new_cursor;

            foreach (ActionState action_state in actions_while_scanning) {
                var action = lookup_action (action_state.name) as SimpleAction;
                action.set_enabled (busy == action_state.enable);
            }
        }

        void set_ui_state (Gtk.Widget child, bool busy) {
            set_busy (busy);

            if (child == home_page) {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (false);
                this.title = _("Devices & Locations");
                nav_view.pop ();
            } else {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (true);
                this.title = active_location.name;
                if (nav_view.visible_page != result_page) {
                    nav_view.push_by_tag ("results");
                }
            }
        }

        /*void scanner_has_first_row (Gtk.TreeModel model, Gtk.TreePath path, Gtk.TreeIter iter) {
            model.row_has_child_toggled.disconnect (scanner_has_first_row);
            reroot_treeview (path);
        }

        void reroot_when_scanner_not_empty () {
            Gtk.TreeIter first;

            if (active_location.scanner.get_iter_first (out first)) {
                reroot_treeview (new Gtk.TreePath.first ());
            } else {
                active_location.scanner.row_has_child_toggled.connect (scanner_has_first_row);
            }
        }*/

        void set_chart_location (Location location) {
            rings_chart.location = location;
            treemap_chart.location = location;
        }

        void scanner_completed () {
            var scanner = active_location.scanner;

            if (scan_completed_handler > 0) {
                scanner.disconnect (scan_completed_handler);
                scan_completed_handler = 0;
            }

            if (scanning_progress_id > 0) {
                Source.remove (scanning_progress_id);
                scanning_progress_id = 0;
            }

            try {
                scanner.finish();
            } catch (IOError.CANCELLED e) {
                // Handle cancellation silently
                return;
            } catch (Error e) {
                warning ("Could not scan folder %s: %s", scanner.directory.get_parse_name (), e.message);
                toast (_("Could not scan folder"));
                set_ui_state (home_page, false);
                return;
            }

            reroot_treeview (scanner.root);
            set_chart_location (active_location);
            set_ui_state (result_page, false);

            // Make sure to update the folder display after the scan
            folder_display.path = scanner.root;

            banner.revealed = !scanner.show_allocated_size;

            if (!is_active) {
                var notification = new Notification(_("Scan completed"));
                notification.set_body (_("Completed scan of “%s”").printf (scanner.directory.get_parse_name ()));
                get_application ().send_notification ("scan-completed", notification);
            }
        }

        void scan_location (Location location, bool force = false) {
            cancel_scan ();

            active_location = location;

            pathbar.location = location;
            folder_display.location = location;

            // Update the timestamp for GtkRecentManager
            location_list.add_location (location);

            columnview_sort_model.model = null;

            var scanner = location.scanner;
            scan_completed_handler = scanner.completed.connect (scanner_completed);

            banner.revealed = false;

            scanning_progress_id = Timeout.add (500, () => {
                location.progress ();
                return Source.CONTINUE;
            });

            set_ui_state (result_page, true);
            scanner.scan (force);
        }

        public void scan_directory (File directory, ScanFlags flags=ScanFlags.NONE) {
            FileInfo info = null;
            try {
                info = directory.query_info (FileAttribute.STANDARD_TYPE, FileQueryInfoFlags.NONE, null);
            } catch (Error e) {
            }

            if (info == null || info.get_file_type () != FileType.DIRECTORY) {
                toast (_("“%s” is not a valid folder").printf (directory.get_parse_name ()));
                return;
            }

            var location = new Location.for_file (directory, flags);
            scan_location (location);
        }

        [GtkCallback]
        private void folder_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new FileCell ();
            item.child = child;
            folder_display.folder_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void folder_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var row = item.item as Gtk.TreeListRow;
            var results = row.item as Scanner.Results;
            var child = item.child as FileCell;
            child.item = results;
            child.list_row = row;
        }

        [GtkCallback]
        private void folder_cell_unbind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = item.child as FileCell;
            child.item = null;
        }

        [GtkCallback]
        private void folder_cell_teardown (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            folder_display.folder_size_group.remove_widget (item.child);
        }

        [GtkCallback]
        private void size_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new SizeCell ();
            item.child = child;
            folder_display.size_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void size_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var row = item.item as Gtk.TreeListRow;
            var results = row.item as Scanner.Results;
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
            folder_display.size_size_group.remove_widget (item.child);
        }

        [GtkCallback]
        private void contents_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new ContentsCell ();
            item.child = child;
            folder_display.contents_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void contents_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var row = item.item as Gtk.TreeListRow;
            var results = row.item as Scanner.Results;
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
            folder_display.contents_size_group.remove_widget (item.child);
        }

        [GtkCallback]
        private void time_modified_cell_setup (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var child = new TimeModifiedCell ();
            item.child = child;
            folder_display.time_modified_size_group.add_widget (item.child);
        }

        [GtkCallback]
        private void time_modified_cell_bind (GLib.Object object) {
            var item = object as Gtk.ColumnViewCell;
            var row = item.item as Gtk.TreeListRow;
            var results = row.item as Scanner.Results;
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
            folder_display.time_modified_size_group.remove_widget (item.child);
        }
    }
}
