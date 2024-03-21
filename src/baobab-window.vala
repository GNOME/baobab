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
        private unowned Gtk.TreeView treeview;
        [GtkChild]
        private unowned Gtk.PopoverMenu treeview_popover_menu;
        [GtkChild]
        private unowned Gtk.TreeViewColumn size_column;
        [GtkChild]
        private unowned Gtk.TreeViewColumn contents_column;
        [GtkChild]
        private unowned Gtk.TreeViewColumn time_modified_column;
        [GtkChild]
        private unowned Gtk.GestureClick treeview_right_click_gesture;
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
        private unowned Gtk.Spinner spinner;

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

            setup_treeview ();
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
            dialog.modal = true;
            dialog.set_transient_for (this);
            dialog.present ();
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

            var about = new Adw.AboutWindow() {
                transient_for = this,
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

            about.present();
        }

        void on_chart_item_activated (Chart chart, Gtk.TreeIter iter) {
            var path = active_location.scanner.get_path (iter);
            reroot_treeview (path);
        }

        void on_pathbar_item_activated (Pathbar pathbar, Gtk.TreePath path) {
            reroot_treeview (path);
        }

        void on_folder_display_activated () {
            go_up_treeview ();
        }

        void go_up_treeview () {
            var path = folder_display.path;
            if (path != null && path.get_depth () > 1) {
                var cursor_path = path.copy ();
                path.up ();
                reroot_treeview (path);
                cursor_path = convert_child_path_to_path (cursor_path);
                treeview.set_cursor (cursor_path, null, false);
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

        public void open_item (Gtk.TreeIter iter) {
            var file = active_location.scanner.get_file (iter);
            try {
                AppInfo.launch_default_for_uri (file.get_uri (), null);
            } catch (Error e) {
                warning ("Failed to open file: %s\n", e.message);
                toast (_("Failed to open file"));
            }
        }

        public void copy_path (Gtk.TreeIter iter) {
            var parse_name = active_location.scanner.get_file (iter).get_parse_name ();
            var clipboard = get_clipboard ();
            clipboard.set_text (parse_name);
        }

        public void trash_file (Gtk.TreeIter iter) {
            var file = active_location.scanner.get_file (iter);
            try {
                file.trash ();
                active_location.scanner.remove (ref iter);
            } catch (Error e) {
                warning ("Failed to move to file to the trash: %s", e.message);
                toast (_("Failed to trash file"));
            }
        }

        bool get_selected_iter (out Gtk.TreeIter iter) {
            Gtk.TreeIter wrapper_iter;

            var selection = treeview.get_selection ();
            var result = selection.get_selected (null, out wrapper_iter);

            convert_iter_to_child_iter (out iter, wrapper_iter);

            return result;
        }

        void convert_iter_to_child_iter (out Gtk.TreeIter child_iter, Gtk.TreeIter iter) {
            Gtk.TreeIter filter_iter;

            var sort = (Gtk.TreeModelSort) treeview.model;
            sort.convert_iter_to_child_iter (out filter_iter, iter);

            var filter = (Gtk.TreeModelFilter) sort.model;
            filter.convert_iter_to_child_iter (out child_iter, filter_iter);
        }

        Gtk.TreePath convert_path_to_child_path (Gtk.TreePath path) {
            var sort = (Gtk.TreeModelSort) treeview.model;
            var filter_path = sort.convert_path_to_child_path (path);

            var filter = (Gtk.TreeModelFilter) sort.model;
            return filter.convert_path_to_child_path (filter_path);
        }

        Gtk.TreePath convert_child_path_to_path (Gtk.TreePath path) {
            var sort = (Gtk.TreeModelSort) treeview.model;
            var filter = (Gtk.TreeModelFilter) sort.model;
            var filter_path = filter.convert_child_path_to_path (path);
            return sort.convert_child_path_to_path (filter_path);
        }

        void on_show_treeview_popover () {
            show_treeview_popover (treeview_popover_menu,
                                   treeview.get_allocated_width () / 2,
                                   treeview.get_allocated_height () / 2);
        }

        void on_treeview_expand_row () {
            Gtk.TreeIter iter;
            if (treeview.get_selection ().get_selected (null, out iter)) {
                Gtk.TreePath path = treeview.model.get_path (iter);
                treeview.expand_row (path, false);
            }
        }

        void on_treeview_collapse_row () {
            Gtk.TreeIter iter;
            if (treeview.get_selection ().get_selected (null, out iter)) {
                Gtk.TreePath path = treeview.model.get_path (iter);
                if (!treeview.collapse_row (path) && path.up ()) {
                    treeview.set_cursor(path, null, false);
                }
            }
        }

        void on_treeview_open_folder () {
            Gtk.TreeIter iter;
            if (get_selected_iter (out iter)) {
                open_item (iter);
            }
        }

        void on_treeview_copy () {
            Gtk.TreeIter iter;
            if (get_selected_iter (out iter)) {
                copy_path (iter);
            }
        }

        void on_treeview_trash () {
            Gtk.TreeIter iter;
            if (get_selected_iter (out iter)) {
                trash_file (iter);
            }
        }

        void setup_treeview () {
            treeview_right_click_gesture.pressed.connect ((n_press, x, y) => {
                Gtk.TreePath path;
                if (treeview.get_path_at_pos ((int) x, (int) y, out path, null, null, null)) {
                    treeview.get_selection ().select_path (path);
                    show_treeview_popover (treeview_popover_menu,  (int) x, (int) y);
                }
            });

            treeview.row_activated.connect ((wrapper_path, column) => {
                var path = convert_path_to_child_path (wrapper_path);
                reroot_treeview (path, true);
            });

            var folder_display_model = (Gtk.TreeSortable) folder_display.model;
            folder_display_model.sort_column_changed.connect (reset_treeview_sorting);

            folder_display.size_column.notify["width"].connect (copy_treeview_column_sizes);
            folder_display.contents_column.notify["width"].connect (copy_treeview_column_sizes);
            folder_display.time_modified_column.notify["width"].connect (copy_treeview_column_sizes);
        }

        void copy_treeview_column_sizes () {
            size_column.min_width = folder_display.size_column.width;
            contents_column.min_width = folder_display.contents_column.width;
            time_modified_column.min_width = folder_display.time_modified_column.width;
        }

        void reset_treeview_sorting () {
            int id;
            Gtk.SortType sort_type;

            var folder_display_model = (Gtk.TreeSortable) folder_display.model;
            var treeview_model = (Gtk.TreeSortable) treeview.model;

            folder_display_model.get_sort_column_id (out id, out sort_type);
            treeview_model.set_sort_column_id (id, sort_type);
        }


        void reroot_treeview (Gtk.TreePath path, bool select_first = false) {
            Gtk.TreeIter iter;
            active_location.scanner.get_iter (out iter, path);
            if (!active_location.scanner.iter_has_child (iter)) {
                return;
            }

            rings_chart.tree_root = path;
            treemap_chart.tree_root = path;
            folder_display.path = path;
            pathbar.path = path;

            var filter = new Gtk.TreeModelFilter (active_location.scanner, path);
            treeview.model = new Gtk.TreeModelSort.with_model (filter);
            reset_treeview_sorting ();

            if (select_first) {
                treeview.set_cursor (new Gtk.TreePath.first (), null, false);
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
                spinner.start ();
                pathbar.sensitive = false;
            } else {
                spinner.stop ();
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

            reroot_treeview (new Gtk.TreePath.first ());
            set_chart_location (active_location);
            set_ui_state (result_page, false);

            // Make sure to update the folder display after the scan
            folder_display.path = new Gtk.TreePath.first ();
            copy_treeview_column_sizes ();

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

            treeview.model = null;

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
    }
}
