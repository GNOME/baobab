/* -*- indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Baobab - disk usage analyzer
 *
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

    public class LocationMonitor {
        private const int MAX_RECENT_LOCATIONS = 5;

        private static LocationMonitor? instance = null;

        private VolumeMonitor monitor;

        private List<Location> locations = null;

        public signal void changed ();

        public static LocationMonitor get () {
            if (instance == null) {
                instance = new LocationMonitor ();
            }

            return instance;
        }

        private LocationMonitor () {
            monitor = VolumeMonitor.get ();
            monitor.mount_changed.connect (mount_changed);
            monitor.mount_removed.connect (mount_removed);
            monitor.mount_added.connect (mount_added);
            monitor.volume_changed.connect (volume_changed);
            monitor.volume_removed.connect (volume_removed);
            monitor.volume_added.connect (volume_added);

            initial_fill ();
        }

        public unowned List<Location> get_locations () {
            return locations;
        }

        void volume_changed (Volume volume) {
            changed ();
        }

        void volume_removed (Volume volume) {
            foreach (var location in locations) {
                if (location.volume == volume) {
                    locations.remove (location);
                    break;
                }
            }

            changed ();
        }

        void volume_added (Volume volume) {
            locations.append (new Location.from_volume (volume));

            changed ();
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

            changed ();
        }

        void mount_added (Mount mount) {
            var volume = mount.get_volume ();
            if (volume == null) {
                 locations.append (new Location.from_mount (mount));
            } else {
                foreach (var location in locations) {
                    if (location.volume == volume) {
                        location.update ();
                        break;
                    }
                }
            }

            changed ();
        }

        void initial_fill () {
            locations.append (new Location.for_main_volume ());

            foreach (var volume in monitor.get_volumes ()) {
                locations.append (new Location.from_volume (volume));
            }

            foreach (var mount in monitor.get_mounts ()) {
                if (mount.get_volume () == null) {
                    locations.append (new Location.from_mount (mount));
                } else {
                    // Already added as volume
                }
            }

            locations.append (Location.get_home_location ());

            Gtk.RecentManager recent_manager = Gtk.RecentManager.get_default ();
            List<Gtk.RecentInfo> recent_items = recent_manager.get_items ();

            int n_recents = 0;
            foreach (var info in recent_items) {
                if (n_recents >= this.MAX_RECENT_LOCATIONS) {
                    break;
                }
                if (info.has_group ("baobab") && info.exists ()) {
                    // FIXME: I do not like this hack to avoid duplucates
                    // and beside locations should have a proper uri field
                    // to be uniquely identified
                    bool dup = false;
                    foreach (var l in locations) {
                        if (l.mount_point == info.get_uri_display ()) {
                            dup = true;
                        }
                    }
                    if (!dup) {
                        locations.append (new Location.for_recent_info (info));
                        n_recents++;
                    }
                }
            }

            changed ();
        }

        public void recent_add (File directory) {
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

            Gtk.RecentManager.get_default ().add_full (directory.get_uri (), data);

            // FIXME: it would probably be cleaner to add the dir to the list of
            // locations ourselves, but for now we cheat and just rebuild the list
            // from scratch so that we get the proper ordering and deduplication
            // FIXME: is this really how I clear a list in vala??
            unowned List<Location> l = locations;
            while (l != null) {
                unowned List<Location> tmp = l.next;
                locations.delete_link (l);
                l = tmp;
            }
            initial_fill ();
        }
    }
}