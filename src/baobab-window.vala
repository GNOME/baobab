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

    public class Window : Gtk.ApplicationWindow {
        Settings ui_settings;
        Gtk.Notebook main_notebook;
        Gtk.Toolbar toolbar;
        Gtk.ToolItem toolbar_home_toolitem;
        Gtk.ToolButton toolbar_show_home_page;
        Gtk.SeparatorToolItem toolbar_separator;
        Gtk.ToolButton toolbar_rescan;
        Gtk.InfoBar infobar;
        Gtk.Label infobar_primary;
        Gtk.Label infobar_secondary;
        Gtk.ScrolledWindow location_scroll;
        LocationList location_list;
        Gtk.TreeView treeview;
        Gtk.Notebook chart_notebook;
        Chart rings_chart;
        Chart treemap_chart;
        Gtk.Spinner spinner;
        Scanner? scanner;

        static Gdk.Cursor busy_cursor;

        void radio_activate (SimpleAction action, Variant? parameter) {
            action.change_state (parameter);
        }

        private const GLib.ActionEntry[] action_entries = {
            { "show-home-page", on_show_home_page_activate },
            { "active-chart", radio_activate, "s", "'rings'", on_chart_type_changed },
            { "scan-home", on_scan_home_activate },
            { "scan-folder", on_scan_folder_activate },
            { "scan-remote", on_scan_remote_activate },
            { "stop", on_stop_activate },
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
            { "scan-home", false },
            { "scan-folder", false },
            { "scan-remote", false },
            { "stop", true },
            { "reload", false },
            { "show-allocated", false },
            { "expand-all", false },
            { "collapse-all", false }
        };

        private enum UIPage {
            HOME,
            RESULT
        }

        private enum ChartPage {
            RINGS,
            TREEMAP,
            SPINNER
        }

        private enum DndTargets {
            URI_LIST
        }

        private const Gtk.TargetEntry dnd_target_list[1] = {
            {"text/uri-list", 0, DndTargets.URI_LIST}
        };

        public Window (Application app) {
            Object (application: app);

            busy_cursor = new Gdk.Cursor (Gdk.CursorType.WATCH);

            add_action_entries (action_entries, this);

            // Build ourselves.
            var builder = new Gtk.Builder ();
            try {
                builder.add_from_resource ("/org/gnome/baobab/ui/baobab-main-window.ui");
            } catch (Error e) {
                error ("loading main builder file: %s", e.message);
            }

            // Cache some objects from the builder.
            main_notebook = builder.get_object ("main-notebook") as Gtk.Notebook;
            toolbar = builder.get_object ("toolbar") as Gtk.Toolbar;
            toolbar_home_toolitem = builder.get_object ("home-page-toolitem") as Gtk.ToolItem;
            toolbar_show_home_page = builder.get_object ("show-home-page-button") as Gtk.ToolButton;
            toolbar_separator = builder.get_object ("toolbar-separator") as Gtk.SeparatorToolItem;
            toolbar_rescan = builder.get_object ("rescan-button") as Gtk.ToolButton;
            infobar = builder.get_object ("infobar") as Gtk.InfoBar;
            infobar_primary = builder.get_object ("infobar-primary-label") as Gtk.Label;
            infobar_secondary = builder.get_object ("infobar-secondary-label") as Gtk.Label;
            location_scroll = builder.get_object ("location-scrolled-window") as Gtk.ScrolledWindow;
            location_list = builder.get_object ("location-list") as LocationList;
            treeview = builder.get_object ("treeview") as Gtk.TreeView;
            chart_notebook = builder.get_object ("chart-notebook") as Gtk.Notebook;
            rings_chart = builder.get_object ("rings-chart") as Chart;
            treemap_chart = builder.get_object ("treemap-chart") as Chart;
            spinner = builder.get_object ("spinner") as Gtk.Spinner;

            location_list.set_adjustment (location_scroll.get_vadjustment ());
            location_list.set_action (on_scan_location_activate);
            location_list.update ();

            setup_treeview (builder);

            var infobar_close_button = builder.get_object ("infobar-close-button") as Gtk.Button;
            infobar_close_button.clicked.connect (() => { clear_message (); });

            // To make it draggable like a primary toolbar
            toolbar.get_style_context ().add_class (Gtk.STYLE_CLASS_MENUBAR);

            ui_settings = Application.get_ui_settings ();
            lookup_action ("active-chart").change_state (ui_settings.get_value ("active-chart"));

            rings_chart.item_activated.connect (on_chart_item_activated);
            treemap_chart.item_activated.connect (on_chart_item_activated);

            // Setup drag-n-drop
            drag_data_received.connect (on_drag_data_received);
            enable_drop ();

            add (builder.get_object ("window-contents") as Gtk.Widget);
            title = _("Disk Usage Analyzer");
            set_default_size (960, 600);
            set_hide_titlebar_when_maximized (true);

            set_ui_state (UIPage.HOME, false);

            show ();
        }

        void on_show_home_page_activate () {
            if (scanner != null) {
                scanner.cancel ();
            }

            clear_message ();
            set_ui_state (UIPage.HOME, false);
        }

        void on_chart_type_changed (SimpleAction action, Variant value) {
            switch (value as string) {
                case "rings":
                    chart_notebook.page = ChartPage.RINGS;
                    break;
                case "treemap":
                    chart_notebook.page = ChartPage.TREEMAP;
                    break;
                default:
                    return;
            }

            ui_settings.set_value ("active-chart", value);
            action.set_state (value);
        }

        void on_scan_home_activate () {
            var dir = File.new_for_path (GLib.Environment.get_home_dir ());
            scan_directory (dir);
        }

        void on_scan_folder_activate () {
            var file_chooser = new Gtk.FileChooserDialog (_("Select Folder"), this,
                                                          Gtk.FileChooserAction.SELECT_FOLDER,
                                                          Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
                                                          Gtk.Stock.OPEN, Gtk.ResponseType.ACCEPT);

            file_chooser.set_modal (true);

            file_chooser.response.connect ((response) => {
                if (response == Gtk.ResponseType.ACCEPT) {
                    var dir = file_chooser.get_file ();
                    scan_directory (dir);
                }
                file_chooser.destroy ();
            });

            file_chooser.show ();
        }

        void on_scan_remote_activate () {
            var connect_server = new ConnectServer ();

            connect_server.selected.connect ((uri) => {
                if (uri != null) {
                    var dir = File.new_for_uri (uri);
                    scan_directory (dir);
                }
            });

            connect_server.show ();
        }

        void on_scan_location_activate (Location location) {
            if (location.is_volume) {
                location.mount_volume.begin ((location_, res) => {
                    try {
                        location.mount_volume.end (res);
                        scan_directory (location.file, ScanFlags.EXCLUDE_MOUNTS);
                    } catch (Error e) {
                        message (_("Could not analyze volume."), e.message, Gtk.MessageType.ERROR);
                    }
                });
            } else {
                scan_directory (location.file);
            }
        }

        void on_stop_activate () {
            if (scanner != null) {
                scanner.cancel ();
            }
        }

        void on_reload_activate () {
            if (scanner != null) {
                scan_directory (scanner.directory, scanner.scan_flags);
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
                Gtk.show_uri (get_screen (), "help:baobab", Gtk.get_current_event_time ());
            } catch (Error e) {
                warning ("Failed to show help: %s", e.message);
            }
        }

        void on_about_activate () {
            const string authors[] = {
                "Ryan Lortie <desrt@desrt.ca>",
                "Fabio Marzocca <thesaltydog@gmail.com>",
                "Paolo Borelli <pborelli@gnome.com>",
                "Benoît Dejean <benoit@placenet.org>",
                "Igalia (rings-chart and treemap widget) <www.igalia.com>"
            };

            const string copyright = "Copyright \xc2\xa9 2005-2011 Fabio Marzocca, Paolo Borelli, Benoît Dejean, Igalia\n" +
                                     "Copyright \xc2\xa9 2011-2012 Ryan Lortie, Paolo Borelli\n";

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
            var path = scanner.get_path (iter);

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

        bool show_treeview_popup (Gtk.Menu popup, Gdk.EventButton? event) {
            if (event != null) {
                popup.popup (null, null, null, event.button, event.time);
            } else {
                popup.popup (null, null, null, 0, Gtk.get_current_event_time ());
                popup.select_first (false);
            }
            return true;
        }

        void setup_treeview (Gtk.Builder builder) {
            var popup = builder.get_object ("treeview-popup-menu") as Gtk.Menu;
            var open_item = builder.get_object ("treeview-popup-open") as Gtk.MenuItem;
            var trash_item = builder.get_object ("treeview-popup-trash") as Gtk.MenuItem;

            treeview.button_press_event.connect ((event) => {
                if (((Gdk.Event) (&event)).triggers_context_menu ()) {
                    return show_treeview_popup (popup, event);
                }

                return false;
            });

            treeview.popup_menu.connect (() => {
                return show_treeview_popup (popup, null);
            });

            open_item.activate.connect (() => {
                var selection = treeview.get_selection ();
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    string parse_name;
                    scanner.get (iter, Scanner.Columns.PARSE_NAME, out parse_name);
                    var file = File.parse_name (parse_name);
                    try {
                        var info = file.query_info (FileAttribute.STANDARD_CONTENT_TYPE, 0, null);
                        var content = info.get_content_type ();
                        var appinfo = AppInfo.get_default_for_type (content, true);
                        var files = new List<File>();
                        files.append (file);
                        appinfo.launch(files, null);
                    } catch (Error e) {
                        warning ("Failed open file with application: %s", e.message);
                    }
                }
            });

            trash_item.activate.connect (() => {
                var selection = treeview.get_selection ();
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    string parse_name;
                    scanner.get (iter, Scanner.Columns.PARSE_NAME, out parse_name);
                    var file = File.parse_name (parse_name);
                    try {
                        file.trash ();
                        scanner.remove (ref iter);
                    } catch (Error e) {
                        warning ("Failed to move file to the trash: %s", e.message);
                    }
                }
            });

            var selection = treeview.get_selection ();
            selection.changed.connect (() => {
                Gtk.TreeIter iter;
                if (selection.get_selected (null, out iter)) {
                    var path = scanner.get_path (iter);
                    rings_chart.set_root (path);
                    treemap_chart.set_root (path);
                }
            });
        }

        void message (string primary_msg, string secondary_msg, Gtk.MessageType type) {
            infobar.message_type = type;
            infobar_primary.set_label ("<b>%s</b>".printf (_(primary_msg)));
            infobar_secondary.set_label ("<small>%s</small>".printf (_(secondary_msg)));
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
                rings_chart.freeze_updates ();
                treemap_chart.freeze_updates ();
                (lookup_action ("active-chart") as SimpleAction).set_enabled (false);
                chart_notebook.page = ChartPage.SPINNER;
                spinner.start ();
                toolbar_show_home_page.label = _("Cancel Scan");
            } else {
                enable_drop ();
                rings_chart.thaw_updates ();
                treemap_chart.thaw_updates ();
                (lookup_action ("active-chart") as SimpleAction).set_enabled (true);
                spinner.stop ();
                lookup_action ("active-chart").change_state (ui_settings.get_value ("active-chart"));
                toolbar_show_home_page.label = _("All Locations");
            }

            var window = get_window ();
            if (window != null) {
                window.set_cursor (cursor);
            }

            foreach (ActionState action_state in actions_while_scanning) {
                var action = lookup_action (action_state.name) as SimpleAction;
                action.set_enabled (busy == action_state.enable);
            }
        }

        void set_ui_state (UIPage page, bool busy) {
            toolbar_home_toolitem.visible = (page == UIPage.HOME);
            toolbar_show_home_page.visible = (page == UIPage.RESULT);
            toolbar_separator.visible = (page == UIPage.RESULT);
            toolbar_rescan.visible = (page == UIPage.RESULT);

            set_busy (busy);

            if (page == UIPage.HOME) {
                var action = lookup_action ("reload") as SimpleAction;
                action.set_enabled (false);
            }

            main_notebook.page = page;
        }

        public bool check_dir (File directory) {
            //if (Application.is_excluded_location (directory)) {
            //    message("", _("Cannot check an excluded folder!"), Gtk.MessageType.INFO);
            //    return false;
            //}

            try {
                var info = directory.query_info (FileAttribute.STANDARD_TYPE, FileQueryInfoFlags.NONE, null);
                if (info.get_file_type () != FileType.DIRECTORY/* || is_virtual_filesystem ()*/) {
                    var primary = _("\"%s\" is not a valid folder").printf (directory.get_parse_name ());
                    message (primary, _("Could not analyze disk usage."), Gtk.MessageType.ERROR);
                    return false;
                }
                return true;
            } catch (Error e) {
                var primary = _("\"%s\" is not a valid folder").printf (directory.get_parse_name ());
                message (primary, e.message, Gtk.MessageType.ERROR);
                return false;
            }
        }

        void first_row_has_child (Gtk.TreeModel model, Gtk.TreePath path, Gtk.TreeIter iter) {
            model.row_has_child_toggled.disconnect (first_row_has_child);
            treeview.expand_row (path, false);
        }

        void set_model (Gtk.TreeModel model) {
            Gtk.TreeIter first;

            treeview.model = model;

            if (model.iter_children (out first, null) && model.iter_has_child (first)) {
                treeview.expand_row (model.get_path (first), false);
            } else {
                model.row_has_child_toggled.connect (first_row_has_child);
            }

            model.bind_property ("max-depth", rings_chart, "max-depth", BindingFlags.SYNC_CREATE);
            model.bind_property ("max-depth", treemap_chart, "max-depth", BindingFlags.SYNC_CREATE);
            treemap_chart.set_model_with_columns (model,
                                                  Scanner.Columns.DISPLAY_NAME,
                                                  Scanner.Columns.ALLOC_SIZE,
                                                  Scanner.Columns.PARSE_NAME,
                                                  Scanner.Columns.PERCENT,
                                                  Scanner.Columns.ELEMENTS, null);
            rings_chart.set_model_with_columns (model,
                                                Scanner.Columns.DISPLAY_NAME,
                                                Scanner.Columns.ALLOC_SIZE,
                                                Scanner.Columns.PARSE_NAME,
                                                Scanner.Columns.PERCENT,
                                                Scanner.Columns.ELEMENTS, null);
        }

        public void scan_directory (File directory, ScanFlags flags = ScanFlags.NONE) {
            if (!check_dir (directory)) {
                return;
            }

            // FIXME: only add folders and skip volumes?
            location_list.recent_add (directory);

            scanner = new Scanner (directory, flags);
            set_model (scanner);

            scanner.completed.connect(() => {
                set_ui_state (UIPage.RESULT, false);
                try {
                    scanner.finish();
                } catch (IOError.CANCELLED e) {
                    // Handle cancellation silently
                    scanner.clear ();
                } catch (Error e) {
                    var primary = _("Could not scan folder \"%s\" or some of the folders it contains.").printf (scanner.directory.get_parse_name ());
                    message (primary, e.message, Gtk.MessageType.WARNING);
                }
            });

            clear_message ();
            set_ui_state (UIPage.RESULT, true);

            scanner.scan ();
        }
    }
}
