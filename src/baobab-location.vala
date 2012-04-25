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

    public class Location {
        public string name { get; private set; }
        public string? mount_point { get; private set; }

        public uint64? size { get; private set; }
        public uint64? used { get; private set; }
        public uint64? reserved { get; private set; }
        public Icon? icon { get; private set; }

        public Volume? volume { get; private set; }
        public Mount? mount { get; private set; }

        private static const string FS_ATTRIBUTES =
            FileAttribute.FILESYSTEM_SIZE + "," +
            FileAttribute.FILESYSTEM_USED;

        private static Location? home_location = null;

        public static Location get_home_location () {
            return home_location;
        }

        public bool is_home_location {
            get {
                return (this == home_location);
            }
        }

        public Location.from_volume (Volume volume_) {
            volume = volume_;
            volume.changed.connect((vol) => {
                update_volume_info ();
            });
            update_volume_info ();
        }

        public Location.from_mount (Mount mount_) {
            mount = mount_;
            fill_from_mount ();
        }

        public Location.for_main_volume () {
            name = _("Main volume");
            mount_point = "/";
            icon = new ThemedIcon ("drive-harddisk-system");

            get_fs_usage (File.new_for_path (mount_point));
        }

        public Location.for_home_folder () {
            mount_point = Environment.get_home_dir ();
            make_this_home_location ();

            get_fs_usage (File.new_for_path (mount_point));
        }

        public void update () {
            update_volume_info ();
        }

        void make_this_home_location () {
            name = _("Home folder");
            icon = new ThemedIcon ("user-home");

            home_location = this;
        }

        void update_volume_info () {
            mount = volume.get_mount ();

            if (mount != null) {
                fill_from_mount ();
            } else {
                name = volume.get_name ();
                icon = volume.get_icon ();
                mount_point = null;
                size = null;
                used = null;
            }
        }

        void fill_from_mount () {
            name = mount.get_name ();
            icon = mount.get_icon ();

            var file = mount.get_root ();
            mount_point = file.get_path ();

            if (mount_point == Environment.get_home_dir ()) {
                make_this_home_location ();
            }

            get_fs_usage (file);
        }

        private void get_fs_usage (File file) {
            size = null;
            used = null;
            reserved = null;
            try {
                var info = file.query_filesystem_info (FS_ATTRIBUTES, null);
                if (info.has_attribute (FileAttribute.FILESYSTEM_SIZE)) {
                    size = info.get_attribute_uint64 (FileAttribute.FILESYSTEM_SIZE);
                }
                if (info.has_attribute (FileAttribute.FILESYSTEM_USED)) {
                    used = info.get_attribute_uint64 (FileAttribute.FILESYSTEM_USED);
                }
                if (size != null && used != null && info.has_attribute (FileAttribute.FILESYSTEM_FREE)) {
                    var free = info.get_attribute_uint64 (FileAttribute.FILESYSTEM_FREE);
                    reserved = size - free - used;
                }
            } catch (Error e) {
            }
        }

        public async void mount_volume () throws Error {
            if (mount != null || volume == null)
                return;

            var mount_op = new Gtk.MountOperation (null);
            yield volume.mount (MountMountFlags.NONE, mount_op, null);

            update_volume_info ();
        }
    }
}
