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
    public class Window : Gtk.ApplicationWindow {
        [GtkChild]
        private Gtk.Box vbox;
        [GtkChild]
        private Gtk.HeaderBar header_bar;
        [GtkChild]
        private Gtk.Button scan_button;
        [GtkChild]
        private Gtk.Button back_button;
        [GtkChild]
        private Gtk.Button reload_button;
        [GtkChild]
        private Gtk.Stack main_stack;
        [GtkChild]
        private Gtk.Widget home_page;
        [GtkChild]
        private Gtk.Widget result_page;
        [GtkChild]
        private Gtk.InfoBar infobar;
        [GtkChild]
        private Gtk.Label infobar_primary_label;
        [GtkChild]
        private Gtk.Label infobar_secondary_label;
        [GtkChild]
        private Gtk.Button infobar_close_button;
        [GtkChild]
        private LocationList location_list;
        [GtkChild]
        private Gtk.TreeView treeview;
        [GtkChild]
        private Gtk.TreeViewColumn size_column;
        [GtkChild]
        private CellRendererSize size_column_size_renderer;
        [GtkChild]
        private Gtk.Menu treeview_popup_menu;
        [GtkChild]
        private Gtk.MenuItem treeview_popup_open;
        [GtkChild]
        private Gtk.MenuItem treeview_popup_copy;
        [GtkChild]
        private Gtk.MenuItem treeview_popup_trash;
        [GtkChild]
        private Gtk.Stack chart_stack;
        [GtkChild]
        private Gtk.Stack spinner_stack;
        [GtkChild]
        private Gtk.StackSwitcher chart_stack_switcher;
        [GtkChild]
        private Chart rings_chart;
        [GtkChild]
        private Chart treemap_chart;
        [GtkChild]
        private Gtk.Spinner spinner;
        private Location? active_location;
        private ulong scan_completed_handler;

        static Gdk.Cursor busy_cursor;

        private const GLib.ActionEntry[] action_entries = {
            { "show-home-page", on_show_home_page_activate },
            { "scan-folder", on_scan_folder_activate },
            { "reload", on_reload_activate },
            { "show-allocated", on_show_allocated },
            { "expand-all", on_expand_all },
            { "collapse-all", on_collapse_all },
            { "help", on_help_activate },
            { "about", on_about_activate }
        };

        protected struct ActionState {
            string name;
            bool enable;
        }

        private const ActionState[] actions_while_scanning = {
            { "scan-folder", false },
            { "show-allocated", false },
            { "expand-all", false },
            { "collapse-all", false }
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
                busy_cursor = new Gdk.Cursor.for_display (get_display(), Gdk.CursorType.WATCH);
            }

            var ui_settings = new Settings ("org.gnome.baobab.ui");
            ui_settings.delay ();

            add_action_entries (action_entries, this);
            var action = ui_settings.create_action ("active-chart");
            add_action (action);

            location_list.set_action (on_scan_location_activate);

            setup_treeview ();

            infobar_close_button.clicked.connect (() => { clear_message (); });

            ui_settings.bind ("active-chart", chart_stack, "visible-child-name", SettingsBindFlags.DEFAULT);
            chart_stack.destroy.connect (() => { Settings.unbind (chart_stack, "visible-child-name"); });

            rings_chart.item_activated.connect (on_chart_item_activated);
            treemap_chart.item_activated.connect (on_chart_item_activated);

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
                //ui_settings.set_int ("window-state", event.new_window_state);
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

            active_location = null;
            scan_completed_handler = 0;

            var desktop = Environment.get_variable ("XDG_CURRENT_DESKTOP");

            if (desktop == null || !desktop.contains ("Unity")) {
                this.set_titlebar (header_bar);
            } else {
                header_bar.show_close_button = false;
                header_bar.get_style_context ().remove_class ("titlebar");
                vbox.pack_start (header_bar);
            }

            set_ui_state (home_page, false);

            button_press_event.connect ((event) => {
                uint button;
                event.get_button(out button);
                // mouse back button
                if (button == 8) {
                    lookup_action ("show-home-page").activate (null);
                    return Gdk.EVENT_STOP;
                }
                return Gdk.EVENT_PROPAGATE;
            });

            show ();
        }

        void on_show_home_page_activate () {
            if (active_location != null && active_location.scanner != null) {
                active_location.scanner.cancel ();
            }

            clear_message ();
            set_ui_state (home_page, false);
        }

        void on_scan_folder_activate () {
            var file_chooser = new Gtk.FileChooserDialog (_("Select Folder"), this,
                                                          Gtk.FileChooserAction.SELECT_FOLDER,
                                                          _("_Cancel"), Gtk.ResponseType.CANCEL,
                                                          _("_Open"), Gtk.ResponseType.ACCEPT);

            file_chooser.local_only = false;
            file_chooser.create_folders = false;
            file_chooser.modal = true;

            var check_button = new Gtk.CheckButton.with_label (_("Recursively analyze mount points"));
            file_chooser.extra_widget = check_button;

            file_chooser.response.connect ((response) => {
                if (response == Gtk.ResponseType.ACCEPT) {
                    var flags = check_button.active ? ScanFlags.NONE : ScanFlags.EXCLUDE_MOUNTS;
                    scan_directory (file_chooser.get_file (), flags);
                }
                file_chooser.destroy ();
            });

            file_chooser.show ();
        }

        void set_active_location (Location location) {
            if (scan_completed_handler > 0) {
                active_location.scanner.disconnect (scan_completed_handler);
                scan_completed_handler = 0;
            }

            active_location = location;

            // Update the timestamp for GtkRecentManager
            location_list.add_location (location);
        }

        void on_scan_location_activate (Location location) {
            set_active_location (location);

            if (location.is_volume) {
                location.mount_volume.begin ((location_, res) => {
                    try {
                        location.mount_volume.end (res);
                        scan_active_location (false);
                    } catch (Error e) {
                        message (_("Could not analyze volume."), e.message, Gtk.MessageType.ERROR);
                    }
                });
            } else {
                scan_active_location (false);
            }
        }

        void on_reload_activate () {
            if (active_location != null) {
                if (active_location.scanner != null) {
                    active_location.scanner.cancel ();
                }
                scan_active_location (true);
            }
        }

        void on_show_allocated () {
        }

        void on_expand_all () {
            treeview.expand_all ();
        }

        void on_collapse_all () {
            treeview.collapse_all ();
        }

        void on_help_activate () {
            try {
                Gtk.show_uri_on_window (this, "help:baobab", Gtk.get_current_event_time ());
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
                                   "program-name", _("Baobab"),
                                   "logo-icon-name", "baobab",
                                   "version", Config.VERSION,
                                   "comments", _("A graphical tool to analyze disk usage."),
                                   "copyright", copyright,
                                   "license-type", Gtk.License.GPL_2_0,
                                   "wrap-license", false,
                                   "authors", authors,
                                   "translator-credits", _("translator-credits"),
                                   null);
        }

        void on_chart_item_activated (Chart chart, Gtk.TreeIter iter) {
            var path = active_location.scanner.get_path (iter);

            if (!treeview.is_row_expanded (path)) {
                treeview.expand_to_path (path);
            }

            treeview.set_cursor (path, null, false);
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

        public void open_item (Gtk.TreeIter iter) {
            string parse_name;
            active_location.scanner.get (iter, Scanner.Columns.PARSE_NAME, out parse_name);
            var file = File.parse_name (parse_name);
            try {
                var info = file.query_info (FileAttribute.STANDARD_CONTENT_TYPE, 0, null);
                var content = info.get_content_type ();
                var appinfo = AppInfo.get_default_for_type (content, true);
                var context = get_display ().get_app_launch_context ();
                context.set_timestamp (Gtk.get_current_event_time ());
                var files = new List<File>();
                files.append (file);
                appinfo.launch(files, context);
            } catch (Error e) {
                message (_("Failed to open file"), e.message, Gtk.MessageType.ERROR);
            }
        }

        public void copy_path (Gtk.TreeIter iter) {
            string parse_name;
            active_location.scanner.get (iter, Scanner.Columns.PARSE_NAME, out parse_name);
            var clipboard = Gtk.Clipboard.get (Gdk.SELECTION_CLIPBOARD);
            clipboard.set_text (parse_name, -1);
            clipboard.store ();
        }

        public void trash_file (Gtk.TreeIter iter) {
            string parse_name;
            active_location.scanner.get (iter, Scanner.Columns.PARSE_NAME, out parse_name);
            var file = File.parse_name (parse_name);
            try {
                file.trash ();
                active_location.scanner.remove (ref iter);
            } catch (Error e) {
                message (_("Failed to move file to the trash"), e.message, Gtk.MessageType.ERROR);
            }
        }

        void setup_treeview () {
            treeview.button_press_event.connect ((event) => {
                if (event.triggers_context_menu ()) {
                    Gtk.TreePath path;
                    double x, y;
                    event.get_coords (out x, out y);
                    if (treeview.get_path_at_pos ((int)x, (int)y, out path, null, null, null)) {
                        treeview.get_selection ().select_path (path);
                        treeview_popup_menu.popup_at_pointer (event);
                        return true;
                    }
                }

                return false;
            });

            treeview.popup_menu.connect (() => {
                treeview_popup_menu.popup_at_pointer (null);
                return true;
            });

            treeview_popup_open.activate.connect (() => {
                var selection = treeview.get_selection ();
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    open_item (iter);
                }
            });

            treeview_popup_copy.activate.connect (() => {
                var selection = treeview.get_selection ();
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    copy_path (iter);
                }
            });

            treeview_popup_trash.activate.connect (() => {
                var selection = treeview.get_selection ();
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    trash_file (iter);
                }
            });

            var selection = treeview.get_selection ();
            selection.changed.connect (() => {
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    var path = active_location.scanner.get_path (iter);
                    rings_chart.root = path;
                    treemap_chart.root = path;
                }
            });
        }

        void message (string primary_msg, string secondary_msg, Gtk.MessageType type) {
            infobar.message_type = type;
            infobar_primary_label.label = "<b>%s</b>".printf (primary_msg);
            infobar_secondary_label.label = "<small>%s</small>".printf (secondary_msg);
            infobar.set_revealed (true);
        }

        void clear_message () {
            infobar.set_revealed (false);
        }

        void set_busy (bool busy) {
            Gdk.Cursor? cursor = null;

            if (busy) {
                cursor = busy_cursor;
                disable_drop ();
                chart_stack_switcher.sensitive = false;
                spinner_stack.visible_child = spinner;
                spinner.start ();
            } else {
                enable_drop ();
                spinner.stop ();
                spinner_stack.visible_child = chart_stack;
                chart_stack_switcher.sensitive = true;
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
            scan_button.visible = (child == home_page);
            reload_button.visible = (child == result_page);
            back_button.visible = (child == result_page);

            set_busy (busy);

            if (child == home_page) {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (false);
                header_bar.title = _("Devices & Locations");
            } else {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (true);
                header_bar.title = active_location.name;
            }

            main_stack.visible_child = child;
        }

        void first_row_has_child (Gtk.TreeModel model, Gtk.TreePath path, Gtk.TreeIter iter) {
            model.row_has_child_toggled.disconnect (first_row_has_child);
            treeview.expand_row (path, false);
        }

        void expand_first_row () {
            Gtk.TreeIter first;

            if (treeview.model.get_iter_first (out first) && treeview.model.iter_has_child (first)) {
                treeview.expand_row (treeview.model.get_path (first), false);
            } else {
                treeview.model.row_has_child_toggled.connect (first_row_has_child);
            }
        }

        void set_chart_model (Gtk.TreeModel model, bool show_allocated_size) {
            model.bind_property ("max-depth", rings_chart, "max-depth", BindingFlags.SYNC_CREATE);
            model.bind_property ("max-depth", treemap_chart, "max-depth", BindingFlags.SYNC_CREATE);
            treemap_chart.set_model_with_columns (model,
                                                  Scanner.Columns.DISPLAY_NAME,
                                                  show_allocated_size ? Scanner.Columns.ALLOC_SIZE : Scanner.Columns.SIZE,
                                                  Scanner.Columns.PARSE_NAME,
                                                  Scanner.Columns.PERCENT,
                                                  Scanner.Columns.ELEMENTS, null);
            rings_chart.set_model_with_columns (model,
                                                Scanner.Columns.DISPLAY_NAME,
                                                show_allocated_size ? Scanner.Columns.ALLOC_SIZE : Scanner.Columns.SIZE,
                                                Scanner.Columns.PARSE_NAME,
                                                Scanner.Columns.PERCENT,
                                                Scanner.Columns.ELEMENTS, null);
        }

        void scan_active_location (bool force) {
            var scanner = active_location.scanner;

            scan_completed_handler = scanner.completed.connect(() => {
                try {
                    scanner.finish();
                } catch (IOError.CANCELLED e) {
                    // Handle cancellation silently
                    if (scan_completed_handler > 0) {
                        scanner.disconnect (scan_completed_handler);
                        scan_completed_handler = 0;
                    }
                    return;
                } catch (Error e) {
                    Gtk.TreeIter iter;
                    Scanner.State state;
                    scanner.get_iter_first (out iter);
                    scanner.get (iter, Scanner.Columns.STATE, out state);
                    if (state == Scanner.State.ERROR) {
                        var primary = _("Could not scan folder “%s”").printf (scanner.directory.get_parse_name ());
                        message (primary, e.message, Gtk.MessageType.ERROR);
                    } else {
                        var primary = _("Could not scan some of the folders contained in “%s”").printf (scanner.directory.get_parse_name ());
                        message (primary, e.message, Gtk.MessageType.WARNING);
                    }
                }

                // Use allocated size if available, where available is defined as
                // alloc_size > 0 for the root element
                Gtk.TreeIter iter;
                scanner.get_iter_first (out iter);
                uint64 alloc_size;
                scanner.get (iter, Scanner.Columns.ALLOC_SIZE, out alloc_size, -1);
                bool show_allocated_size = alloc_size > 0;
                size_column_size_renderer.show_allocated_size = show_allocated_size;
                size_column.sort_column_id = show_allocated_size ? Scanner.Columns.ALLOC_SIZE : Scanner.Columns.SIZE;
                set_chart_model (active_location.scanner, show_allocated_size);

                set_ui_state (result_page, false);

                if (!show_allocated_size) {
                    message (_("Could not detect occupied disk sizes."), _("Apparent sizes are shown instead."), Gtk.MessageType.INFO);
                }
            });

            clear_message ();
            set_ui_state (result_page, true);

            scanner.scan (force);

            treeview.model = scanner;
            expand_first_row ();
        }

        public void scan_directory (File directory, ScanFlags flags=ScanFlags.NONE) {
            var location = new Location.for_file (directory, flags);

            if (location.info == null) {
                var primary = _("“%s” is not a valid folder").printf (directory.get_parse_name ());
                message (primary, _("Could not analyze disk usage."), Gtk.MessageType.ERROR);
                return;
            }

            if (location.info.get_file_type () != FileType.DIRECTORY/* || is_virtual_filesystem ()*/) {
                var primary = _("“%s” is not a valid folder").printf (directory.get_parse_name ());
                message (primary, _("Could not analyze disk usage."), Gtk.MessageType.ERROR);
                return;
            }

            set_active_location (location);
            scan_active_location (false);
        }
    }
}
