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
        private unowned Gtk.Image image;
        [GtkChild]
        private unowned Gtk.Label name_label;
        [GtkChild]
        private unowned Gtk.Label path_label;
        [GtkChild]
        private unowned Gtk.Label available_label;
        [GtkChild]
        private unowned Gtk.Label total_size_label;
        [GtkChild]
        private unowned Gtk.LevelBar usage_bar;

        public Location? location { get; private set; }

        public LocationRow (Location l) {
            location = l;

            image.gicon = location.symbolic_icon;

            var escaped = Markup.escape_text (location.name);
            name_label.label = "<b>%s</b>".printf (escaped);

            path_label.hide();
            if (location.file != null) {
                path_label.label = Markup.escape_text (location.file.get_parse_name ());
                path_label.show();
            }

            // assume for local mounts the end of the mount path is the
            // relevant information, and for remote mounts the beginning is
            // more important
            path_label.ellipsize = location.is_remote ? Pango.EllipsizeMode.END : Pango.EllipsizeMode.START;

            update_fs_usage_info ();
            location.changed.connect (() => { update_fs_usage_info (); });
        }

        void update_fs_usage_info () {
            total_size_label.hide();
            if (location.volume != null || location.mount != null || location.is_main_volume) {
                if (location.size != null) {
                    total_size_label.label = _("%s Total").printf (format_size (location.size));
                    total_size_label.show();

                    if (location.used != null) {
                        var actually_used = location.used + (location.reserved ?? 0);
                        available_label.label = _("%s Available").printf (format_size (location.size - actually_used));

                        usage_bar.max_value = location.size;

                        // Set critical color at 90% of the size
                        usage_bar.add_offset_value (Gtk.LEVEL_BAR_OFFSET_LOW, 0.9 * location.size);
                        usage_bar.value = actually_used;
                        usage_bar.show ();
                    } else {
                        available_label.label = _("Unknown");
                    }
                } else if (location.used != null) {
                    // useful for some remote mounts where we don't know the
                    // size but do have a usage figure
                    available_label.label = _("%s Used").printf (format_size (location.used));
                } else if (location.volume != null && location.mount == null && location.volume.can_mount ()) {
                    available_label.label = _("Unmounted");
                }
            }
        }
    }

    [GtkTemplate (ui = "/org/gnome/baobab/ui/baobab-location-list.ui")]
    public class LocationList : Adw.PreferencesPage {
        [GtkChild]
        private unowned Gtk.ListBox local_list_box;
        [GtkChild]
        private unowned Gtk.ListBox remote_list_box;
        [GtkChild]
        private unowned Adw.PreferencesGroup remote_group;

        public signal void location_activated (Location location);

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

            local_list_box.row_activated.connect (row_activated);

            remote_list_box.row_activated.connect (row_activated);

            populate ();

            queue_query_fs_usage ();
            Timeout.add_seconds(2, (() => {
                queue_query_fs_usage ();
                return Source.CONTINUE;
            }));
        }

        void queue_query_fs_usage () {
            foreach (var location in locations) {
                location.queue_query_fs_usage ();
            }
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
            foreach (var location in locations) {
                if (location.volume == volume) {
                    location.update_volume_info ();
                }
            }

            // Collect duplicated mounts
            var mount = volume.get_mount ();
            if (mount == null) {
                return;
            }
            foreach (var location in locations) {
                // We can't just check location.mount == mount because there could be multiple GMounts with the same root...
                var same_mount = location.mount != null && location.mount.get_root ().equal (mount.get_root ());
                if (same_mount && location.volume != volume) {
                    locations.remove(location);
                }
            }

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
            volume.changed.connect (() => {
                volume_changed (volume);
            });

            update ();
        }

        void mount_changed (Mount mount) {
        }

        void mount_removed (Mount mount) {
            var volume = mount.get_volume ();
            if (volume != null) {
                volume_changed (volume);
            }

            foreach (var location in locations) {
                if (location.mount == mount && location.volume == null) {
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
                volume_changed (volume);
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
                return (int)(b.get_modified ().difference (a.get_modified ()));
            });

            unowned List<Gtk.RecentInfo> last = recent_items.nth (MAX_RECENT_LOCATIONS - 1);
            if (last != null) {
                last.next = null;
            }

            foreach (var info in recent_items) {
                locations.append (new Location.for_recent_info (info));
            }
        }

        void row_activated (Gtk.ListBoxRow row) {
            var location_widget = row as LocationRow;
            location_activated (location_widget.location);
        }

        public void update () {
            for (Gtk.Widget? child = local_list_box.get_first_child (); child != null; child = local_list_box.get_first_child ()) {
                local_list_box.remove (child);
            }

            for (Gtk.Widget? child = remote_list_box.get_first_child (); child != null; child = remote_list_box.get_first_child ()) {
                remote_list_box.remove (child);
            }

            remote_group.visible = false;

            foreach (var location in locations) {
                if (location.is_remote) {
                    remote_list_box.append (new LocationRow (location));
                    remote_group.visible = true;
                } else {
                    local_list_box.append (new LocationRow (location));
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
            clear_recent (false);
            populate_recent ();

            update ();
        }

        public void clear_recent (bool remove_from_recent_manager = true) {
            unowned List<Location> iter = locations;
            while (iter != null) {
                unowned List<Location> next = iter.next;
                if (iter.data.is_recent) {
                    try {
                        if (remove_from_recent_manager) {
                            Gtk.RecentManager.get_default ().remove_item (iter.data.file.get_uri ());
                        }
                        locations.remove_link (iter);
                    } catch (Error e) {
                        warning ("Attempting to remove an item from recent locations, but failed: %s", e.message);
                    }
                }
                iter = next;
            }

            update ();
        }
    }
}
