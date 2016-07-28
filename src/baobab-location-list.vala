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

            image.gicon = location.icon;

            var escaped = GLib.Markup.escape_text (location.name, -1);
            name_label.label = "<b>%s</b>".printf (escaped);

            escaped = location.file != null ? GLib.Markup.escape_text (location.file.get_parse_name (), -1) : "";
            path_label.label = escaped;

            // assume for local mounts the end of the mount path is the
            // relevant information, and for remote mounts the beginning is
            // more important
            path_label.ellipsize = location.is_remote ? Pango.EllipsizeMode.END : Pango.EllipsizeMode.START;

            if (location.is_volume || location.is_main_volume) {
                if (location.size != null) {
                    total_size_label.label = "%s Total".printf (format_size (location.size));
                    total_size_label.show ();

                    if (location.used != null) {
                        available_label.label = "%s Available".printf (format_size (location.size - location.used));

                        usage_bar.max_value = location.size;

                        // Set critical color at 90% of the size
                        usage_bar.add_offset_value (Gtk.LEVEL_BAR_OFFSET_LOW, 0.9 * location.size);
                        usage_bar.value = location.used;
                        usage_bar.show ();
                    } else {
                        available_label.label = "Unknown";
                    }
                }

                if (location.used != null) {
                    // useful for some remote mounts where we don't know the
                    // size but do have a usage figure
                    available_label.label = "%s Used".printf (format_size (location.used));
                }

                available_label.show ();
            }
        }
    }

    public class LocationList : Object {
        private const int MAX_RECENT_LOCATIONS = 5;

        private VolumeMonitor monitor;

        private List<Location> locations = null;

        public signal void update ();

        construct {
            monitor = VolumeMonitor.get ();
            monitor.mount_changed.connect (mount_changed);
            monitor.mount_removed.connect (mount_removed);
            monitor.mount_added.connect (mount_added);
            monitor.volume_changed.connect (volume_changed);
            monitor.volume_removed.connect (volume_removed);
            monitor.volume_added.connect (volume_added);

            populate ();
        }

        public void append (Location location) {
            locations.append(location);
        }

        public void @foreach (Func<Location> func) {
            locations.foreach (func);
        }

        public bool already_present (File file) {
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
            append (new Location.from_volume (volume));
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
                    append (new Location.from_mount (mount));
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
            append (new Location.for_home_folder ());
            append (new Location.for_main_volume ());

            foreach (var volume in monitor.get_volumes ()) {
                volume_added (volume);
            }

            foreach (var mount in monitor.get_mounts ()) {
                mount_added (mount);
            }

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
                append (new Location.for_recent_info (info));
            }

            update ();
        }
    }

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-location-list.ui")]
    public abstract class BaseLocationListWidget : Gtk.Box {
        [GtkChild]
        private Gtk.Label label_widget;
        [GtkChild]
        private Gtk.ListBox list;

        public abstract string label { get; }

        private LocationList locations = null;
        public void set_locations (LocationList locations) {
            this.locations = locations;
            locations.update.connect (update);
        }

        public delegate void LocationAction (Location l);
        private LocationAction? location_action;

        construct {
            label_widget.label = _(label);
            list.selection_mode = Gtk.SelectionMode.NONE;
            list.set_header_func (update_header);
            list.row_activated.connect (row_activated);
        }

        public abstract bool allow_display (Location location);

        public override void show_all () {
            base.show_all (); // set children to visible

            if (list.get_children ().length () == 0) {
                visible = false;
            }
        }

        public void set_adjustment (Gtk.Adjustment adj) {
            list.set_adjustment (adj);
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
            list.foreach ((widget) => { widget.destroy (); });

            locations.foreach((location) => {
                if (allow_display (location)) {
                    list.add (new LocationRow (location));
                }
            });

            show_all ();
        }

        public void add_location (Location location) {
            if (location.file == null) {
                return;
            }

            if (!locations.already_present (location.file)) {
                locations.append (location);
            }

            update ();

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
        }
    }

    public class LocalLocationList : BaseLocationListWidget {
        public override string label { get { return "This Computer"; } }
        public override bool allow_display (Location location) {
            return !location.is_remote;
        }
    }

    public class RemoteLocationList : BaseLocationListWidget {
        public override string label { get { return "Remote Locations"; } }
        public override bool allow_display (Location location) {
            return location.is_remote;
        }
    }
}
