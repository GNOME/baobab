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
    public class Window : Hdy.ApplicationWindow {
        [GtkChild]
        private unowned Gtk.Box vbox;
        [GtkChild]
        private unowned Hdy.HeaderBar header_bar;
        [GtkChild]
        private unowned Pathbar pathbar;
        [GtkChild]
        private unowned Gtk.Button back_button;
        [GtkChild]
        private unowned Gtk.Button reload_button;
        [GtkChild]
        private unowned Gtk.MenuButton menu_button;
        [GtkChild]
        private unowned Gtk.Stack main_stack;
        [GtkChild]
        private unowned Gtk.Widget home_page;
        [GtkChild]
        private unowned Gtk.Widget result_page;
        [GtkChild]
        private unowned Gtk.InfoBar infobar;
        [GtkChild]
        private unowned Gtk.Label infobar_primary_label;
        [GtkChild]
        private unowned Gtk.Label infobar_secondary_label;
        [GtkChild]
        private unowned Gtk.Button infobar_close_button;
        [GtkChild]
        private unowned LocationList location_list;
        [GtkChild]
        private unowned FolderDisplay folder_display;
        [GtkChild]
        private unowned Gtk.TreeView treeview;
        [GtkChild]
        private unowned Gtk.Menu treeview_popup_menu;
        [GtkChild]
        private unowned Gtk.MenuItem treeview_popup_open;
        [GtkChild]
        private unowned Gtk.MenuItem treeview_popup_copy;
        [GtkChild]
        private unowned Gtk.MenuItem treeview_popup_trash;
        [GtkChild]
        private unowned Gtk.TreeViewColumn size_column;
        [GtkChild]
        private unowned Gtk.TreeViewColumn contents_column;
        [GtkChild]
        private unowned Gtk.TreeViewColumn time_modified_column;
        [GtkChild]
        private unowned Gtk.Stack chart_stack;
        [GtkChild]
        private unowned Gtk.Stack spinner_stack;
        [GtkChild]
        private unowned Gtk.StackSwitcher chart_stack_switcher;
        [GtkChild]
        private unowned Chart rings_chart;
        [GtkChild]
        private unowned Chart treemap_chart;
        [GtkChild]
        private unowned Gtk.Spinner spinner;

        private Location? active_location = null;
        private ulong scan_completed_handler = 0;
        private uint scanning_progress_id = 0;

        static Gdk.Cursor busy_cursor;

        private const GLib.ActionEntry[] action_entries = {
            { "show-primary-menu", on_show_primary_menu_activate, null, "false", null },
            { "show-home-page", on_show_home_page_activate },
            { "scan-folder", on_scan_folder_activate },
            { "reload", on_reload_activate },
            { "clear-recent", on_clear_recent },
            { "show-preferences", on_show_preferences },
            { "help", on_help_activate },
            { "about", on_about_activate }
        };

        protected struct ActionState {
            string name;
            bool enable;
        }

        private const ActionState[] actions_while_scanning = {
            { "scan-folder", false },
        };

        private enum DndTargets {
            URI_LIST
        }

        private const Gtk.TargetEntry dnd_target_list[1] = {
            {"text/uri-list", 0, DndTargets.URI_LIST}
        };

        public Window (Application app) {
            Object (application: app);

            if (busy_cursor == null) {
                busy_cursor = new Gdk.Cursor.from_name (get_display(), "wait");
            }

            var ui_settings = new Settings ("org.gnome.baobab.ui");
            ui_settings.delay ();

            add_action_entries (action_entries, this);
            var action = ui_settings.create_action ("active-chart");
            add_action (action);

            location_list.location_activated.connect (location_activated);

            setup_treeview ();

            infobar_close_button.clicked.connect (() => { clear_message (); });

            ui_settings.bind ("active-chart", chart_stack, "visible-child-name", SettingsBindFlags.DEFAULT);
            chart_stack.destroy.connect (() => { Settings.unbind (chart_stack, "visible-child-name"); });

            rings_chart.item_activated.connect (on_chart_item_activated);
            treemap_chart.item_activated.connect (on_chart_item_activated);
            pathbar.item_activated.connect (on_pathbar_item_activated);
            folder_display.activated.connect (on_folder_display_activated);

            // Setup drag-n-drop
            drag_data_received.connect (on_drag_data_received);
            enable_drop ();

            // Setup window geometry saving
            Gdk.WindowState window_state = (Gdk.WindowState) ui_settings.get_int ("window-state");
            if (Gdk.WindowState.MAXIMIZED in window_state) {
                maximize ();
            }

            int width, height;
            ui_settings.get ("window-size", "(ii)", out width, out height);
            resize (width, height);

            window_state_event.connect ((event) => {
                ui_settings.set_int ("window-state", event.new_window_state);
                return false;
            });

            configure_event.connect ((event) => {
                if (!(Gdk.WindowState.MAXIMIZED in get_window ().get_state ())) {
                    get_size (out width, out height);
                    ui_settings.set ("window-size", "(ii)", width, height);
                }
                return false;
            });

            destroy.connect (() => {
                ui_settings.apply ();
            });

            set_ui_state (home_page, false);

            button_press_event.connect ((event) => {
                // mouse back button
                if (event.button == 8) {
                    lookup_action ("show-home-page").activate (null);
                    return Gdk.EVENT_STOP;
                }
                return Gdk.EVENT_PROPAGATE;
            });

            show ();
        }

        void on_show_primary_menu_activate (SimpleAction action) {
            var state = action.get_state ().get_boolean ();
            action.set_state (new Variant.boolean (!state));
        }

        void on_show_home_page_activate () {
            cancel_scan ();
            clear_message ();
            set_ui_state (home_page, false);
        }

        void on_scan_folder_activate () {
            var file_chooser = new Gtk.FileChooserDialog (_("Select Folder"), this,
                                                          Gtk.FileChooserAction.SELECT_FOLDER,
                                                          _("_Cancel"), Gtk.ResponseType.CANCEL,
                                                          _("_Open"), Gtk.ResponseType.OK);

            file_chooser.local_only = false;
            file_chooser.modal = true;
            file_chooser.set_default_response (Gtk.ResponseType.OK);

            var check_button = new Gtk.CheckButton.with_label (_("Recursively analyze mount points"));
            file_chooser.extra_widget = check_button;

            file_chooser.response.connect ((response) => {
                if (response == Gtk.ResponseType.OK) {
                    var flags = check_button.active ? ScanFlags.NONE : ScanFlags.EXCLUDE_MOUNTS;
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
                    message (_("Could not analyze volume."), e.message, Gtk.MessageType.ERROR);
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
            dialog.show ();
        }

        void on_help_activate () {
            try {
                Gtk.show_uri (get_screen (), "help:baobab", Gtk.get_current_event_time ());
            } catch (Error e) {
                message (_("Failed to show help"), e.message, Gtk.MessageType.WARNING);
            }
        }

        void on_about_activate () {
            const string authors[] = {
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

            Gtk.show_about_dialog (this,
                                   "program-name", _("Disk Usage Analyzer"),
                                   "logo-icon-name", "org.gnome.baobab",
                                   "version", Config.VERSION,
                                   "comments", _("A graphical tool to analyze disk usage."),
                                   "website", "https://wiki.gnome.org/action/show/Apps/DiskUsageAnalyzer",
                                   "copyright", copyright,
                                   "license-type", Gtk.License.GPL_2_0,
                                   "wrap-license", false,
                                   "authors", authors,
                                   "translator-credits", _("translator-credits"),
                                   null);
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

        void on_drag_data_received (Gtk.Widget widget, Gdk.DragContext context, int x, int y,
                                    Gtk.SelectionData selection_data, uint target_type, uint time) {
            File dir = null;

            if ((selection_data != null) && (selection_data.get_length () >= 0) && (target_type == DndTargets.URI_LIST)) {
                var uris = GLib.Uri.list_extract_uris ((string) selection_data.get_data ());
                if (uris != null && uris.length == 1) {
                    dir = File.new_for_uri (uris[0]);
                }
            }

            if (dir != null) {
                // finish drop before scanning, since the it can time out
                Gtk.drag_finish (context, true, false, time);
                scan_directory (dir);
            } else {
                Gtk.drag_finish (context, false, false, time);
            }
        }

        void enable_drop () {
            Gtk.drag_dest_set (this,
                               Gtk.DestDefaults.DROP | Gtk.DestDefaults.MOTION | Gtk.DestDefaults.HIGHLIGHT,
                               dnd_target_list,
                               Gdk.DragAction.COPY);
        }

        void disable_drop () {
            Gtk.drag_dest_unset (this);
        }

        bool show_treeview_popup (Gtk.Menu popup, Gdk.EventButton? event) {
            if (event != null) {
                popup.popup_at_pointer (event);
            } else {
                popup.popup_at_widget (treeview, Gdk.Gravity.CENTER, Gdk.Gravity.CENTER);
                popup.select_first (false);
            }
            return Gdk.EVENT_STOP;
        }

        public void open_item (Gtk.TreeIter iter) {
            var file = active_location.scanner.get_file (iter);
            try {
                AppInfo.launch_default_for_uri (file.get_uri (), null);
            } catch (Error e) {
                message (_("Failed to open file"), e.message, Gtk.MessageType.ERROR);
            }
        }

        public void copy_path (Gtk.TreeIter iter) {
            var parse_name = active_location.scanner.get_file (iter).get_parse_name ();
            var clipboard = Gtk.Clipboard.get (Gdk.SELECTION_CLIPBOARD);
            clipboard.set_text (parse_name, -1);
            clipboard.store ();
        }

        public void trash_file (Gtk.TreeIter iter) {
            var file = active_location.scanner.get_file (iter);
            try {
                file.trash ();
                active_location.scanner.remove (ref iter);
            } catch (Error e) {
                message (_("Failed to move file to the trash"), e.message, Gtk.MessageType.ERROR);
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

        void setup_treeview () {
            treeview.button_press_event.connect ((event) => {
                if (event.triggers_context_menu ()) {
                    Gtk.TreePath path;
                    if (treeview.get_path_at_pos ((int)event.x, (int)event.y, out path, null, null, null)) {
                        treeview.get_selection ().select_path (path);
                        return show_treeview_popup (treeview_popup_menu, event);
                    }
                }

                return false;
            });

            treeview.key_press_event.connect ((event) => {
                Gtk.TreeIter iter;
                if (treeview.get_selection ().get_selected (null, out iter)) {
                    Gtk.TreePath path = treeview.model.get_path (iter);
                    if (event.keyval == Gdk.Key.Right) {
                        if (treeview.expand_row (path, false)) {
                            return true;
                        }
                    } else if (event.keyval == Gdk.Key.Left) {
                        if (treeview.collapse_row (path)) {
                            return true;
                        } else if (path.up ()) {
                            treeview.set_cursor(path, null, false);
                            return true;
                        }
                    } else if (event.keyval == Gdk.Key.space) {
                        if (treeview.expand_row (path, false)) {
                            return true;
                        } else if (treeview.collapse_row (path)) {
                            return true;
                        }
                    } else if (event.keyval == Gdk.Key.Up && event.state == Gdk.ModifierType.MOD1_MASK) {
                        go_up_treeview ();
                        return true;
                    }
                }

                return false;
            });

            treeview.popup_menu.connect (() => {
                return show_treeview_popup (treeview_popup_menu, null);
            });

            treeview_popup_open.activate.connect (() => {
                Gtk.TreeIter iter;
                if (get_selected_iter (out iter)) {
                    open_item (iter);
                }
            });

            treeview_popup_copy.activate.connect (() => {
                Gtk.TreeIter iter;
                if (get_selected_iter (out iter)) {
                    copy_path (iter);
                }
            });

            treeview_popup_trash.activate.connect (() => {
                Gtk.TreeIter iter;
                if (get_selected_iter (out iter)) {
                    trash_file (iter);
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

            rings_chart.root = path;
            treemap_chart.root = path;
            folder_display.path = path;
            pathbar.path = path;

            var filter = new Gtk.TreeModelFilter (active_location.scanner, path);
            treeview.model = new Gtk.TreeModelSort.with_model (filter);
            reset_treeview_sorting ();

            if (select_first) {
                treeview.set_cursor (new Gtk.TreePath.first (), null, false);
            }
        }

        void message (string primary_msg, string secondary_msg, Gtk.MessageType type) {
            infobar.message_type = type;
            infobar_primary_label.label = "<b>%s</b>".printf (primary_msg);
            infobar_secondary_label.label = "<small>%s</small>".printf (secondary_msg);
            infobar.show ();
        }

        void clear_message () {
            infobar.hide ();
        }

        void set_busy (bool busy) {
            Gdk.Cursor? cursor = null;

            if (busy) {
                cursor = busy_cursor;
                disable_drop ();
                chart_stack_switcher.sensitive = false;
                spinner_stack.visible_child = spinner;
                spinner.start ();
                pathbar.sensitive = false;
            } else {
                enable_drop ();
                spinner.stop ();
                spinner_stack.visible_child = chart_stack;
                chart_stack_switcher.sensitive = true;
                pathbar.sensitive = true;
            }

            var window = get_window ();
            if (window != null) {
                window.cursor = cursor;
            }

            foreach (ActionState action_state in actions_while_scanning) {
                var action = lookup_action (action_state.name) as SimpleAction;
                action.set_enabled (busy == action_state.enable);
            }
        }

        void set_ui_state (Gtk.Widget child, bool busy) {
            menu_button.visible = (child == home_page);
            reload_button.visible = (child == result_page);
            back_button.visible = (child == result_page);

            set_busy (busy);

            header_bar.custom_title = null;
            if (child == home_page) {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (false);
                header_bar.title = _("Devices & Locations");
            } else {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (true);
                header_bar.title = active_location.name;
                if (child == result_page) {
                    header_bar.custom_title = pathbar;
                }
            }

            main_stack.visible_child = child;
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
                var primary = _("Could not scan folder “%s”").printf (scanner.directory.get_parse_name ());
                message (primary, e.message, Gtk.MessageType.ERROR);
                set_ui_state (home_page, false);
                return;
            }

            reroot_treeview (new Gtk.TreePath.first ());
            set_chart_location (active_location);
            set_ui_state (result_page, false);

            // Make sure to update the folder display after the scan
            folder_display.path = new Gtk.TreePath.first ();
            copy_treeview_column_sizes ();

            if (!scanner.show_allocated_size) {
                message (_("Could not always detect occupied disk sizes."), _("Apparent sizes may be shown instead."), Gtk.MessageType.INFO);
            }

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

            clear_message ();

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
                var primary = _("“%s” is not a valid folder").printf (directory.get_parse_name ());
                message (primary, _("Could not analyze disk usage."), Gtk.MessageType.ERROR);
                return;
            }

            var location = new Location.for_file (directory, flags);
            scan_location (location);
        }
    }
}
