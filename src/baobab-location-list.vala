/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
 * Copyright (C) 2012  Paolo Borelli <pborelli@gnome.org>
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

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-location-row.ui")]
    private class LocationRow : Gtk.ListBoxRow {
        [GtkChild]
        private Gtk.Image image;
        [GtkChild]
        private Gtk.Label name_label;
        [GtkChild]
        private Gtk.Label path_label;
        [GtkChild]
        private Gtk.Label available_label;
        [GtkChild]
        private Gtk.Label total_size_label;
        [GtkChild]
        private Gtk.LevelBar usage_bar;

        public Location? location { get; private set; }

        public LocationRow (Location l) {
            location = l;
            update ();
        }

        public void update () {
            location.get_fs_usage ();
            image.gicon = location.icon;

            var escaped = GLib.Markup.escape_text (location.name, -1);
            name_label.label = "<b>%s</b>".printf (escaped);

            path_label.hide();
            if (location.file != null) {
                path_label.label = GLib.Markup.escape_text (location.file.get_parse_name (), -1);
                path_label.show();
            }

            // assume for local mounts the end of the mount path is the
            // relevant information, and for remote mounts the beginning is
            // more important
            path_label.ellipsize = location.is_remote ? Pango.EllipsizeMode.END : Pango.EllipsizeMode.START;

            total_size_label.hide();
            if (location.is_volume || location.is_main_volume) {
                if (location.size != null) {
                    total_size_label.label = _("%s Total").printf (format_size (location.size));
                    total_size_label.show();

                    if (location.used != null) {
                        available_label.label = _("%s Available").printf (format_size (location.size - location.used));

                        usage_bar.max_value = location.size;

                        // Set critical color at 90% of the size
                        usage_bar.add_offset_value (Gtk.LEVEL_BAR_OFFSET_LOW, 0.9 * location.size);
                        usage_bar.value = location.used;
                        usage_bar.show ();
                    } else {
                        available_label.label = _("Unknown");
                    }
                } else if (location.used != null) {
                    // useful for some remote mounts where we don't know the
                    // size but do have a usage figure
                    available_label.label = _("%s Used").printf (format_size (location.used));
                } else if (location.mount == null && location.volume.can_mount ()) {
                    available_label.label = _("Unmounted");
                }
            }
        }
    }

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-location-list.ui")]
    public class LocationList : Gtk.Box {
        [GtkChild]
        private Gtk.ListBox local_list_box;
        [GtkChild]
        private Gtk.ListBox remote_list_box;
        [GtkChild]
        private Gtk.Box remote_box;

        public delegate void LocationAction (Location l);
        private LocationAction? location_action;

        private const int MAX_RECENT_LOCATIONS = 5;

        private VolumeMonitor monitor;

        private List<Location> locations = null;

        construct {
            monitor = VolumeMonitor.get ();
            monitor.mount_changed.connect (mount_changed);
            monitor.mount_removed.connect (mount_removed);
            monitor.mount_added.connect (mount_added);
            monitor.volume_changed.connect (volume_changed);
            monitor.volume_removed.connect (volume_removed);
            monitor.volume_added.connect (volume_added);

            local_list_box.set_header_func (update_header);
            local_list_box.row_activated.connect (row_activated);

            remote_list_box.set_header_func (update_header);
            remote_list_box.row_activated.connect (row_activated);

            Timeout.add_seconds(3, (() => {
                update_existing ();
                return Source.CONTINUE;
            }));

            populate ();
        }

        void update_existing () {
            local_list_box.foreach ((widget) => { ((LocationRow)widget).update (); });
            remote_list_box.foreach ((widget) => { ((LocationRow)widget).update (); });
        }

        bool already_present (File file) {
            foreach (var l in locations) {
                if (l.file != null && l.file.equal (file)) {
                    return true;
                }
            }
            return false;
        }

        void volume_changed (Volume volume) {
            update ();
        }

        void volume_removed (Volume volume) {
            foreach (var location in locations) {
                if (location.volume == volume) {
                    locations.remove (location);
                    break;
                }
            }

            update ();
        }

        void volume_added (Volume volume) {
            locations.append (new Location.from_volume (volume));
            update ();
        }

        void mount_changed (Mount mount) {
        }

        void mount_removed (Mount mount) {
            foreach (var location in locations) {
                if (location.mount == mount) {
                    locations.remove (location);
                    break;
                }
            }

            update ();
        }

        void mount_added (Mount mount) {
            var volume = mount.get_volume ();
            if (volume == null) {
                if (!already_present (mount.get_root ())) {
                    locations.append (new Location.from_mount (mount));
                }
            } else {
                foreach (var location in locations) {
                    if (location.volume == volume) {
                        location.update ();
                        break;
                    }
                }
            }

            update ();
        }

        void populate () {
            locations.append (new Location.for_home_folder ());
            locations.append (new Location.for_main_volume ());

            foreach (var volume in monitor.get_volumes ()) {
                volume_added (volume);
            }

            foreach (var mount in monitor.get_mounts ()) {
                mount_added (mount);
            }

            populate_recent ();

            update ();
        }

        void populate_recent () {
            Gtk.RecentManager recent_manager = Gtk.RecentManager.get_default ();
            List<Gtk.RecentInfo> recent_items = recent_manager.get_items ();

            unowned List<Gtk.RecentInfo> iter = recent_items;
            while (iter != null) {
                unowned List<Gtk.RecentInfo> next = iter.next;
                if (!iter.data.has_group ("baobab") || !iter.data.exists () || already_present (File.new_for_uri (iter.data.get_uri ()))) {
                    recent_items.remove_link (iter);
                }
                iter = next;
            }

            recent_items.sort ((a, b) => {
                return (int)(b.get_modified () - a.get_modified ());
            });

            unowned List<Gtk.RecentInfo> last = recent_items.nth (MAX_RECENT_LOCATIONS - 1);
            if (last != null) {
                last.next = null;
            }

            recent_items.reverse ();

            foreach (var info in recent_items) {
                locations.append (new Location.for_recent_info (info));
            }
        }

        void update_header (Gtk.ListBoxRow row, Gtk.ListBoxRow? before_row) {
            if (before_row != null && row.get_header () == null) {
                row.set_header (new Gtk.Separator (Gtk.Orientation.HORIZONTAL));
            } else {
                row.set_header (null);
            }
        }

        void row_activated (Gtk.ListBoxRow row) {
            if (location_action != null) {
                var location_widget = row as LocationRow;
                location_action (location_widget.location);
            }
        }

        public void set_action (owned LocationAction? action) {
            location_action = (owned)action;
        }

        public void update () {
            local_list_box.foreach ((widget) => { widget.destroy (); });
            remote_list_box.foreach ((widget) => { widget.destroy (); });

            remote_box.visible = false;

            foreach (var location in locations) {
                if (location.is_remote) {
                    remote_list_box.add (new LocationRow (location));
                    remote_box.visible = true;
                } else {
                    local_list_box.add (new LocationRow (location));
                }
            }
        }

        public void add_location (Location location) {
            if (location.file == null) {
                return;
            }

            // Add to recent files
            Gtk.RecentData data = Gtk.RecentData ();
            data.display_name = null;
            data.description = null;
            data.mime_type = "inode/directory";
            data.app_name = GLib.Environment.get_application_name ();
            data.app_exec = "%s %%u".printf (GLib.Environment.get_prgname ());
            string[] groups = new string[2];
            groups[0] = "baobab";
            groups[1] = null;
            data.groups = groups;
            Gtk.RecentManager.get_default ().add_full (location.file.get_uri (), data);

            // Reload recent locations
            unowned List<Location> iter = locations;
            while (iter != null) {
                unowned List<Location> next = iter.next;
                if (iter.data.is_recent) {
                    locations.remove_link (iter);
                }
                iter = next;
            }
            populate_recent ();

            update ();
        }
    }
}
